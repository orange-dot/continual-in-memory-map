/* cmp_update.c — A/B: constant-step vs sigma(-margin) update, identical streams.
 * Links cinm.c + -lm.  Build: make run-cmp
 */
#include "cinm.h"
#include <stdio.h>
#include <stdint.h>
#include <math.h>

enum { NCTX = 3, NSTEPS = 6000, EVAL = 1000 };
enum { RND_BITS = 24 };
constexpr uint64_t SEED = 0x123456789ABCDEFULL;

static uint64_t rng_state = SEED;
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

/* sigma(-margin) variant: step scaled by reward * P(model is wrong). */
static void update_sigmoid(cinm_map *m, size_t i, const float dphi[static NFEAT],
                           float reward, uint32_t t) {
    float margin = 0.0f;
    for (int k = 0; k < NFEAT; k++) margin += m->w[i][k] * dphi[k];
    float g = reward / (1.0f + expf(margin));
    for (int k = 0; k < NFEAT; k++)
        m->w[i][k] = clampf(m->w[i][k] + ETA * g * m->plast[i] * dphi[k], -W_MAX, W_MAX);
    m->evidence[i]++;
    m->last_touched[i] = t;
}

typedef void (*update_fn)(cinm_map *, size_t, const float *, float, uint32_t);

typedef struct {
    float  win[NCTX];
    double mean_dw;      /* steady-state mean |dw|2 over the last 20% of steps */
    float  mean_absw, max_absw, clamp_frac;
} metrics;

static void run_experiment(update_fn update, metrics *out) {
    rng_state = SEED;                       /* identical stream per run */
    cinm_map map;
    cinm_init(&map);

    float w_true[NCTX][NFEAT];
    for (int c = 0; c < NCTX; c++)
        for (int k = 0; k < NFEAT; k++)
            w_true[c][k] = rnd01() * 2.0f - 1.0f;

    const int warm = NSTEPS * 4 / 5;        /* measure |dw| over the last 20% */
    double dw_sum = 0.0;
    long   dw_n = 0;

    for (int step = 0; step < NSTEPS; step++) {
        uint32_t ctx = (uint32_t)(nx() % NCTX);
        float pa[NFEAT], pb[NFEAT];
        random_features(pa);
        random_features(pb);
        float sa = dot_true(w_true[ctx], pa);
        float sb = dot_true(w_true[ctx], pb);
        const float *win = (sa >= sb) ? pa : pb;
        const float *los = (sa >= sb) ? pb : pa;

        size_t i = cinm_address(&map, ctx, nullptr);
        float dphi[NFEAT];
        for (int k = 0; k < NFEAT; k++) dphi[k] = win[k] - los[k];

        if (step >= warm) {
            float before[NFEAT];
            for (int k = 0; k < NFEAT; k++) before[k] = map.w[i][k];
            update(&map, i, dphi, +1.0f, map.t);
            double d2 = 0.0;
            for (int k = 0; k < NFEAT; k++) {
                double d = (double)map.w[i][k] - before[k];
                d2 += d * d;
            }
            dw_sum += sqrt(d2);
            dw_n++;
        } else {
            update(&map, i, dphi, +1.0f, map.t);
        }
        map.t++;
    }

    for (uint32_t c = 0; c < NCTX; c++) {
        int hit = 0;
        size_t i = cinm_address(&map, c, nullptr);
        for (int e = 0; e < EVAL; e++) {
            float pa[NFEAT], pb[NFEAT];
            random_features(pa);
            random_features(pb);
            float sa = dot_true(w_true[c], pa), sb = dot_true(w_true[c], pb);
            const float *win = (sa >= sb) ? pa : pb;
            const float *los = (sa >= sb) ? pb : pa;
            if (cinm_score(&map, i, win) > cinm_score(&map, i, los)) hit++;
        }
        out->win[c] = (float)hit / EVAL;
    }

    double absw_sum = 0.0;
    float  absw_max = 0.0f;
    long   at_clamp = 0, total = 0;
    for (size_t i = 0; i < map.count; i++)
        for (int k = 0; k < NFEAT; k++) {
            float a = map.w[i][k] < 0.0f ? -map.w[i][k] : map.w[i][k];
            absw_sum += a;
            if (a > absw_max) absw_max = a;
            if (a >= 0.99f * W_MAX) at_clamp++;
            total++;
        }
    out->mean_dw    = dw_n ? dw_sum / (double)dw_n : 0.0;
    out->mean_absw  = total ? (float)(absw_sum / (double)total) : 0.0f;
    out->max_absw   = absw_max;
    out->clamp_frac = total ? (float)at_clamp / (float)total : 0.0f;
}

int main(void) {
    metrics cst, sig;
    run_experiment(cinm_update, &cst);
    run_experiment(update_sigmoid, &sig);

    printf("metric                      const      sigma\n");
    printf("win ctx0                    %.3f      %.3f\n", cst.win[0], sig.win[0]);
    printf("win ctx1                    %.3f      %.3f\n", cst.win[1], sig.win[1]);
    printf("win ctx2                    %.3f      %.3f\n", cst.win[2], sig.win[2]);
    printf("steady |dw| (last 20%%)     %.4f     %.4f\n", cst.mean_dw, sig.mean_dw);
    printf("mean |w|                    %.3f      %.3f\n", cst.mean_absw, sig.mean_absw);
    printf("max  |w|                    %.3f      %.3f\n", cst.max_absw, sig.max_absw);
    printf("clamp fraction              %.3f      %.3f\n", cst.clamp_frac, sig.clamp_frac);
    return 0;
}
