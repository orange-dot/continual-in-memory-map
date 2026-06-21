/* replay_check.c — prove: in-RAM event log = truth; snapshot+replay reconstructs state. */
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

        size_t i = cinm_address(&M, ctx, NULL);
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

    /* RECOVER B: pure replay from the log alone (no snapshot). */
    cinm_map B;
    cinm_init(&B);
    bool b = cinm_log_replay(&log, &B, 0) && cinm_equal(&B, &M);

    printf("snapshot+replay recovers state: %s (base_seq=%u)\n", a ? "PASS" : "FAIL", base);
    printf("event-log alone recovers state: %s\n", b ? "PASS" : "FAIL");

    cinm_log_free(&log);
    return (a && b) ? 0 : 1;
}
