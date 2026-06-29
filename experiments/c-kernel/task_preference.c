/* task_preference.c — Contextual Preference Memory toy task (docs/03). Pure C. */
#include "cinm.h"
#include <stdio.h>
#include <stdint.h>

enum { NCTX = 3, NSTEPS = 6000, WINDOW = 300, EVAL = 1000 };
enum { RND_BITS = 24 };

/* splitmix64 — seeded PRNG, reproducible. */
static uint64_t rng_state = 0x123456789ABCDEFULL;
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
    cinm_map map;
    cinm_init(&map);

    /* Hidden ground-truth preference per context. */
    float w_true[NCTX][NFEAT];
    for (int c = 0; c < NCTX; c++)
        for (int k = 0; k < NFEAT; k++)
            w_true[c][k] = rnd01() * 2.0f - 1.0f;

    bool win_buf[WINDOW] = {0};
    int  win_sum = 0, win_pos = 0;
    bool filled = false;

    printf("step     rolling win-rate (last %d)\n", WINDOW);
    for (int step = 0; step < NSTEPS; step++) {
        uint32_t ctx = (uint32_t)(nx() % NCTX);

        float pa[NFEAT], pb[NFEAT];
        random_features(pa);
        random_features(pb);

        float sa = dot_true(w_true[ctx], pa);
        float sb = dot_true(w_true[ctx], pb);
        const float *win = (sa >= sb) ? pa : pb;
        const float *los = (sa >= sb) ? pb : pa;

        size_t cell = cinm_address(&map, ctx, nullptr);

        /* Predict before learning; ties -> coin flip. */
        float sw = cinm_score(&map, cell, win), sl = cinm_score(&map, cell, los);
        bool correct = (sw > sl) ? true : (sw < sl) ? false : (bool)(nx() & 1);

        win_sum -= win_buf[win_pos];
        win_buf[win_pos] = correct;
        win_sum += correct;
        win_pos = (win_pos + 1) % WINDOW;
        if (win_pos == 0) filled = true;

        float dphi[NFEAT];
        for (int k = 0; k < NFEAT; k++) dphi[k] = win[k] - los[k];
        cinm_update(&map, cell, dphi, +1.0f, map.t);
        map.t++;

        if ((step + 1) % 1000 == 0) {
            int denom = filled ? WINDOW : win_pos;
            printf("%-8d %.3f\n", step + 1, denom ? (float)win_sum / denom : 0.0f);
        }
    }

    printf("\nper-context win-rate after training (%d fresh trials each):\n", EVAL);
    for (uint32_t c = 0; c < NCTX; c++) {
        int hit = 0;
        size_t cell = cinm_address(&map, c, nullptr);
        for (int i = 0; i < EVAL; i++) {
            float pa[NFEAT], pb[NFEAT];
            random_features(pa);
            random_features(pb);
            float sa = dot_true(w_true[c], pa), sb = dot_true(w_true[c], pb);
            const float *win = (sa >= sb) ? pa : pb;
            const float *los = (sa >= sb) ? pb : pa;
            float sw = cinm_score(&map, cell, win), sl = cinm_score(&map, cell, los);
            hit += (sw > sl) ? 1 : (sw < sl) ? 0 : (int)(nx() & 1);
        }
        printf("  context %u: %.3f\n", c, (float)hit / EVAL);
    }

    printf("\ncells allocated: %zu / %d\n", map.count, MAX_CELLS);
    printf("\nexample cell (context 0):\n");
    cinm_explain(&map, cinm_address(&map, 0, nullptr), map.t);
    return 0;
}
