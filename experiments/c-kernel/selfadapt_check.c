/* selfadapt_check.c — Gödel-Darwin self-adaptation gate (D012, P). The taste DRIFTS over
 * the training window, so the right forgetting rate is unknown a priori. Operators (decay
 * proposals) are scored on a held-out slate drawn from the CURRENT (final) taste; the best
 * per niche is archived (MAP-Elites); every proposal leaves a ledger receipt; and a
 * rejected operator is byte-exactly reversible on the live map. Proposer != grader:
 * scoring is on held-out, disjoint from the training window (anti-hack, D010). */
#include "cinm.h"
#include "cinm_ledger.h"
#include "cinm_selfadapt.h"
#include <stdio.h>
#include <stdint.h>

enum { NCTX = 3, WIN = 360, DECAY_EVERY = 6, HELD = 900, ROUNDS = 48 };

static uint64_t rng = 0xFEEDFACECAFEBEEFULL;
static uint64_t nx(void) {
    uint64_t z = (rng += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}
static float rnd01(void) { return (float)(nx() >> 40) / (float)(1u << 24); }

/* taste drifts linearly from w_start to w_end over each context's slice of the window */
static float w_start[NCTX][NFEAT], w_end[NCTX][NFEAT];
static struct { uint32_t ctx; float dphi[NFEAT]; } win_set[WIN];
static struct { uint32_t ctx; float a[NFEAT], b[NFEAT]; bool a_better; } slate[HELD];

static void taste_at(uint32_t ctx, float frac, float out[static NFEAT]) {
    for (int k = 0; k < NFEAT; k++) out[k] = w_start[ctx][k] * (1.0f - frac) + w_end[ctx][k] * frac;
}

static void build_task(void) {
    for (int c = 0; c < NCTX; c++)
        for (int k = 0; k < NFEAT; k++) {
            w_start[c][k] = rnd01() * 2.0f - 1.0f;
            w_end[c][k]   = rnd01() * 2.0f - 1.0f;   /* independent end taste -> real drift */
        }
    int per = WIN / NCTX;
    for (int t = 0; t < WIN; t++) {
        uint32_t ctx = (uint32_t)(t % NCTX);
        int step = t / NCTX;                                 /* index within this context's slice */
        float frac = (float)step / (float)(per - 1);         /* 0 -> 1 across the slice */
        float wt[NFEAT];
        taste_at(ctx, frac, wt);
        float a[NFEAT], b[NFEAT], ta = 0.0f, tb = 0.0f;
        for (int k = 0; k < NFEAT; k++) { a[k]=rnd01(); b[k]=rnd01(); ta+=wt[k]*a[k]; tb+=wt[k]*b[k]; }
        const float *win = ta>=tb?a:b, *los = ta>=tb?b:a;
        win_set[t].ctx = ctx;
        for (int k = 0; k < NFEAT; k++) win_set[t].dphi[k] = win[k] - los[k];
    }
    for (int n = 0; n < HELD; n++) {                          /* held-out: the CURRENT (final) taste */
        uint32_t ctx = (uint32_t)(nx() % NCTX);
        float ta = 0.0f, tb = 0.0f;
        for (int k = 0; k < NFEAT; k++) { slate[n].a[k]=rnd01(); slate[n].b[k]=rnd01(); ta+=w_end[ctx][k]*slate[n].a[k]; tb+=w_end[ctx][k]*slate[n].b[k]; }
        slate[n].ctx = ctx;
        slate[n].a_better = ta >= tb;
    }
}

static double holdout_score(cinm_map *m) {
    int ok = 0;
    for (int n = 0; n < HELD; n++) {
        size_t i = cinm_address(m, slate[n].ctx, NULL);
        if (slate[n].a_better == (cinm_score(m, i, slate[n].a) >= cinm_score(m, i, slate[n].b))) ok++;
    }
    return (double)ok / (double)HELD;
}

/* Speculatively train a fresh map over the drifting window with decay factor `dec`
 * applied every DECAY_EVERY steps; score on held-out (vs the final taste). */
static double eval_decay(float dec) {
    cinm_map m;
    cinm_init(&m);
    for (int t = 0; t < WIN; t++) {
        cinm_update(&m, cinm_address(&m, win_set[t].ctx, NULL), win_set[t].dphi, 1.0f, (uint32_t)t);
        if ((t + 1) % DECAY_EVERY == 0) cinm_decay(&m, dec);
    }
    return holdout_score(&m);
}

int main(void) {
    build_task();

    double baseline = eval_decay(1.0f);     /* default: no forgetting -> lags the drift */

    /* search: propose decay factors, score on held-out, archive MAP-Elites, log a receipt */
    cinm_sa_archive arc;
    cinm_sa_init(&arc, 0.50f, 1.0f);
    cinm_ledger led;
    cinm_ledger_init(&led);
    uint32_t commits = 0, rollbacks = 0;
    for (int r = 0; r < ROUNDS; r++) {
        cinm_op op = { .val = 0.50f + rnd01() * 0.50f };
        double score = eval_decay(op.val);
        bool kept = cinm_sa_offer(&arc, &op, score);
        cinm_decision rec = { .t = (uint32_t)r, .epoch = 0,
                              .kind = kept ? CINM_DECISION_COMMIT : CINM_DECISION_ROLLBACK,
                              .info_a = (uint32_t)cinm_sa_niche_of(&arc, &op) };
        cinm_ledger_append(&led, &rec);
        if (kept) commits++; else rollbacks++;
    }

    cinm_op best = { .val = 0.0f };
    double best_score = 0.0;
    bool have_best = cinm_sa_best(&arc, &best, &best_score);

    bool adapts   = have_best && best_score > baseline + 0.03;
    bool receipts = led.len == ROUNDS && commits >= 1 && rollbacks >= 1;

    /* anti-hack: over-forgetting (decay 0.5) scores worse on held-out than the adapted best,
     * so held-out (not the training window) is what selects — the best is interior. */
    double overforget = eval_decay(0.50f);
    bool antihack = have_best && best.val > 0.55f && best.val < 0.999f
                 && overforget < best_score && baseline < best_score;

    /* reversible on live state: apply an operator (aggressive decay) + training, roll back */
    cinm_map live;
    cinm_init(&live);
    for (int t = 0; t < WIN; t++)
        cinm_update(&live, cinm_address(&live, win_set[t].ctx, NULL), win_set[t].dphi, 1.0f, (uint32_t)t);
    cinm_map pre;
    cinm_snapshot(&live, &pre);
    cinm_decay(&live, 0.50f);
    for (int t = 0; t < WIN; t++)
        cinm_update(&live, cinm_address(&live, win_set[t].ctx, NULL), win_set[t].dphi, 1.0f, (uint32_t)(WIN + t));
    bool mutated = !cinm_equal(&live, &pre);
    cinm_restore(&live, &pre);
    bool reversible = mutated && cinm_equal(&live, &pre);

    printf("self-adaptation beats no-forgetting baseline: %s (best_decay=%.3f best=%.3f base=%.3f)\n",
           adapts ? "PASS" : "FAIL", (double)best.val, best_score, baseline);
    printf("every proposal leaves a ledger receipt: %s (commits=%u rollbacks=%u len=%zu)\n",
           receipts ? "PASS" : "FAIL", commits, rollbacks, led.len);
    printf("held-out rejects extreme operators: %s (overforget=%.3f base=%.3f best=%.3f)\n",
           antihack ? "PASS" : "FAIL", overforget, baseline, best_score);
    printf("rejected operator is byte-exactly reversible: %s\n", reversible ? "PASS" : "FAIL");

    cinm_ledger_free(&led);
    return (adapts && receipts && antihack && reversible) ? 0 : 1;
}
