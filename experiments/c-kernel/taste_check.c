/* taste_check.c — drum-shaped taste loop (O), M1 gate-1. A "context" is a law-axis
 * coordinate (key); a "groove" is a feature vector phi[NFEAT]; the hidden true taste per
 * context picks the better groove. Proves: context-addressed taste beats a context-blind
 * online baseline, retains an old context after learning a new one, and is reversibly
 * undoable (R4). Synthetic preferences — no Rust, no render (D016 M1). */
#include "cinm.h"
#include "cinm_log.h"
#include "cinm_undo.h"
#include <stdio.h>
#include <stdint.h>

enum { NCTX = 4, TRAIN = 1500, EVAL = 600, PER_CTX = 800,
       UNDO_STEPS = 200, UNDO_BACK = 20, STRIDE = 16, HORIZON = 48 };

static uint64_t rng = 0x1234567ABCDEF89ULL;
static uint64_t nx(void) {
    uint64_t z = (rng += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}
static float rnd01(void) { return (float)(nx() >> 40) / (float)(1u << 24); }

static float w_true[NCTX][NFEAT];
static void init_taste(void) {
    for (int c = 0; c < NCTX; c++)
        for (int k = 0; k < NFEAT; k++) w_true[c][k] = rnd01() * 2.0f - 1.0f;
}

/* A preference in context ctx: two grooves, winner has higher true taste; fill dphi. */
static void gen(uint32_t ctx, float dphi[static NFEAT]) {
    float a[NFEAT], b[NFEAT], ta = 0.0f, tb = 0.0f;
    for (int k = 0; k < NFEAT; k++) { a[k] = rnd01(); b[k] = rnd01(); ta += w_true[ctx][k]*a[k]; tb += w_true[ctx][k]*b[k]; }
    const float *win = ta >= tb ? a : b, *los = ta >= tb ? b : a;
    for (int k = 0; k < NFEAT; k++) dphi[k] = win[k] - los[k];
}

/* Held-out win-rate in one context: does the addressed cell rank the true winner above
 * the loser? `blind` addresses one shared cell (key 0), ignoring context. */
static float winrate_ctx(cinm_map *m, uint32_t ctx, bool blind, int trials) {
    int correct = 0;
    for (int n = 0; n < trials; n++) {
        float a[NFEAT], b[NFEAT], ta = 0.0f, tb = 0.0f;
        for (int k = 0; k < NFEAT; k++) { a[k] = rnd01(); b[k] = rnd01(); ta += w_true[ctx][k]*a[k]; tb += w_true[ctx][k]*b[k]; }
        size_t i = cinm_address(m, blind ? 0u : ctx, NULL);
        if ((ta >= tb) == (cinm_score(m, i, a) >= cinm_score(m, i, b))) correct++;
    }
    return (float)correct / (float)trials;
}

static float winrate_all(cinm_map *m, bool blind, int per_ctx) {
    float s = 0.0f;
    for (uint32_t c = 0; c < NCTX; c++) s += winrate_ctx(m, c, blind, per_ctx);
    return s / (float)NCTX;
}

int main(void) {
    init_taste();

    /* learning: same stream into context-addressed (M) and context-blind (Bl) cells */
    cinm_map M, Bl;
    cinm_init(&M);
    cinm_init(&Bl);
    for (int t = 0; t < TRAIN; t++) {
        float dphi[NFEAT];
        uint32_t ctx = (uint32_t)(nx() % NCTX);
        gen(ctx, dphi);
        cinm_update(&M,  cinm_address(&M,  ctx, NULL), dphi, 1.0f, (uint32_t)t);
        cinm_update(&Bl, cinm_address(&Bl, 0u,  NULL), dphi, 1.0f, (uint32_t)t);
    }
    float wr_cim  = winrate_all(&M,  false, EVAL / NCTX);
    float wr_base = winrate_all(&Bl, true,  EVAL / NCTX);
    bool beats = wr_cim > wr_base + 0.05f;

    /* retention: learn ctx0, then learn ctx1; context cells keep ctx0, the shared cell
     * forgets it. */
    cinm_map Mr, Br;
    cinm_init(&Mr);
    cinm_init(&Br);
    uint32_t tk = 0;
    for (int s = 0; s < PER_CTX; s++, tk++) {
        float dphi[NFEAT]; gen(0u, dphi);
        cinm_update(&Mr, cinm_address(&Mr, 0u, NULL), dphi, 1.0f, tk);
        cinm_update(&Br, cinm_address(&Br, 0u, NULL), dphi, 1.0f, tk);
    }
    float mr_before = winrate_ctx(&Mr, 0u, false, EVAL);
    float br_before = winrate_ctx(&Br, 0u, true,  EVAL);
    for (int s = 0; s < PER_CTX; s++, tk++) {
        float dphi[NFEAT]; gen(1u, dphi);
        cinm_update(&Mr, cinm_address(&Mr, 1u, NULL), dphi, 1.0f, tk);
        cinm_update(&Br, cinm_address(&Br, 0u, NULL), dphi, 1.0f, tk);
    }
    float mr_after = winrate_ctx(&Mr, 0u, false, EVAL);
    float br_after = winrate_ctx(&Br, 0u, true,  EVAL);
    bool retains = mr_after >= mr_before - 0.05f && (mr_after - br_after) > 0.10f;

    /* reversible: undo the last UNDO_BACK taste picks, byte-exact, via the R4 window */
    cinm_log lg;
    cinm_log_init(&lg);
    cinm_undo_window u;
    cinm_undo_init(&u, STRIDE, HORIZON);
    cinm_map Mu;
    cinm_init(&Mu);
    cinm_undo_mark(&u, &Mu, 0);
    uint32_t seq = 0;
    for (; seq < UNDO_STEPS; seq++) {
        cinm_event ev = { .seq = seq, .type = 0, .key = (uint32_t)(nx() % NCTX), .reward = 1.0f };
        gen(ev.key, ev.dphi);
        cinm_update(&Mu, cinm_address(&Mu, ev.key, NULL), ev.dphi, ev.reward, seq);
        Mu.t = seq + 1;
        cinm_log_append(&lg, &ev);
        cinm_undo_mark(&u, &Mu, seq + 1);
    }
    uint32_t target = seq - UNDO_BACK;
    cinm_map refn;
    cinm_init(&refn);
    cinm_log_replay_range(&lg, &refn, 0, target);
    cinm_map Uu = Mu;
    bool reversible = cinm_undo_to(&u, &lg, &Uu, target, seq) && cinm_equal(&Uu, &refn);
    cinm_log_free(&lg);

    printf("context-addressed taste beats blind baseline: %s (cim=%.3f base=%.3f)\n",
           beats ? "PASS" : "FAIL", (double)wr_cim, (double)wr_base);
    printf("old-context retention after a new context: %s (cim %.3f->%.3f, base %.3f->%.3f)\n",
           retains ? "PASS" : "FAIL", (double)mr_before, (double)mr_after,
           (double)br_before, (double)br_after);
    printf("undo N taste picks is byte-exact (R4): %s\n", reversible ? "PASS" : "FAIL");

    return (beats && retains && reversible) ? 0 : 1;
}
