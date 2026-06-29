/* replay_check.c — prove: within-epoch replay reconstructs state; snapshot+tail
 * recovers; epoch is explicit map state (D018). */
#include "cinm.h"
#include "cinm_log.h"
#include <stdio.h>
#include <stdint.h>

enum { NCTX = 3, NSTEPS = 4000, SNAP_AT = 2000 };
enum { RND_BITS = 24 };

static uint64_t rng_state = 0x0ABCDEF123456789ULL;
static uint64_t nx(void) {
    uint64_t z = (rng_state += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}
static float rnd01(void) {
    return (float)(nx() >> (64 - RND_BITS)) / (float)(1u << RND_BITS);
}
static void random_features(float phi[static NFEAT]) {
    for (int k = 0; k < NFEAT; k++) phi[k] = rnd01();
}
static float dot_true(const float wt[static NFEAT], const float phi[static NFEAT]) {
    float s = 0.0f;
    for (int k = 0; k < NFEAT; k++) s += wt[k] * phi[k];
    return s;
}

int main(void) {
    cinm_log log;
    cinm_log_init(&log);

    /* ORIGINAL: train M, log every update, snapshot (in RAM) midway. */
    cinm_map M;
    cinm_init(&M);
    float w_true[NCTX][NFEAT];
    for (int c = 0; c < NCTX; c++)
        for (int k = 0; k < NFEAT; k++)
            w_true[c][k] = rnd01() * 2.0f - 1.0f;

    cinm_map snap;
    uint32_t base = 0;
    bool snapped = false;

    for (uint32_t step = 0; step < NSTEPS; step++) {
        uint32_t ctx = (uint32_t)(nx() % NCTX);
        float pa[NFEAT], pb[NFEAT];
        random_features(pa);
        random_features(pb);
        float sa = dot_true(w_true[ctx], pa), sb = dot_true(w_true[ctx], pb);
        const float *win = (sa >= sb) ? pa : pb;
        const float *los = (sa >= sb) ? pb : pa;

        size_t i = cinm_address(&M, ctx, nullptr);
        if (i == MAX_CELLS) { printf("map full\n"); cinm_log_free(&log); return 1; }
        cinm_event ev = { .seq = step, .type = 0, .key = ctx, .reward = 1.0f };
        for (int k = 0; k < NFEAT; k++) ev.dphi[k] = win[k] - los[k];

        cinm_update(&M, i, ev.dphi, ev.reward, step);
        M.t = step + 1;
        if (!cinm_log_append(&log, &ev)) {
            printf("append failed\n");
            cinm_log_free(&log);
            return 1;
        }
        if (step == SNAP_AT) {              /* snapshot = in-RAM map copy + log position */
            cinm_snapshot(&M, &snap);
            base = step + 1;
            snapped = true;
        }
    }

    /* RECOVER A: load snapshot + replay the tail (the real recovery path). */
    cinm_map R;
    bool a = snapped;
    if (a) {
        cinm_restore(&R, &snap);
        a = cinm_log_replay(&log, &R, base) && cinm_equal(&R, &M);
    }

    /* RECOVER B: within-epoch replay from the epoch base (M.base_seq).
     * Pre-consolidation base_seq == 0, so this equals full replay; R1 names it. */
    cinm_map B;
    cinm_init(&B);
    B.base_seq = M.base_seq;             /* share the live map's epoch anchor */
    bool b = cinm_log_replay(&log, &B, M.base_seq) && cinm_equal(&B, &M);

    /* Epoch is real, distinct map state: advancing it is observable and breaks equality. */
    cinm_map E = M;
    cinm_epoch_advance(&E, NSTEPS);
    bool e = E.epoch == M.epoch + 1 && E.base_seq == NSTEPS && !cinm_equal(&E, &M);

    printf("snapshot+replay recovers state: %s (base_seq=%u)\n", a ? "PASS" : "FAIL", base);
    printf("within-epoch replay recovers state: %s (epoch=%u base_seq=%u)\n",
           b ? "PASS" : "FAIL", M.epoch, M.base_seq);
    printf("epoch advance is observable: %s\n", e ? "PASS" : "FAIL");

    cinm_log_free(&log);
    return (a && b && e) ? 0 : 1;
}
