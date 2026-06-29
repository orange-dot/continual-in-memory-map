/* learning_experiment.c — local-rule experiments beyond the core scalar update. */
#include "cinm.h"
#include <stdio.h>
#include <stdint.h>
#include <math.h>

enum { NCTX = 3, NSTEPS = 5000, EVAL = 800, RND_BITS = 24 };
constexpr uint64_t SEED = 0x3141592653589793ULL;

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
    for (int k = 0; k < NFEAT; k++)
        phi[k] = rnd01();
}

static float dot_true(const float wt[static NFEAT], const float phi[static NFEAT]) {
    float s = 0.0f;
    for (int k = 0; k < NFEAT; k++)
        s += wt[k] * phi[k];
    return s;
}

static float margin_of(const cinm_map *m, size_t i, const float dphi[static NFEAT]) {
    float s = 0.0f;
    for (int k = 0; k < NFEAT; k++)
        s += m->w[i][k] * dphi[k];
    return s;
}

static void update_const(cinm_map *m, size_t i, const float dphi[static NFEAT],
                         float reward, uint32_t t) {
    cinm_update(m, i, dphi, reward, t);
}

static void update_sigma(cinm_map *m, size_t i, const float dphi[static NFEAT],
                         float reward, uint32_t t) {
    float g = reward / (1.0f + expf(margin_of(m, i, dphi)));
    for (int k = 0; k < NFEAT; k++)
        m->w[i][k] = clampf(m->w[i][k] + ETA * g * m->plast[i] * dphi[k], -W_MAX, W_MAX);
    m->evidence[i]++;
    m->last_touched[i] = t;
}

static void update_confidence(cinm_map *m, size_t i, const float dphi[static NFEAT],
                              float reward, uint32_t t) {
    float g = reward / (1.0f + expf(margin_of(m, i, dphi)));
    for (int k = 0; k < NFEAT; k++)
        m->w[i][k] = clampf(m->w[i][k] + ETA * g * m->plast[i] * dphi[k], -W_MAX, W_MAX);
    m->conf[i] = clampf(m->conf[i] + 0.002f, 0.0f, 1.0f);
    m->plast[i] = clampf(1.0f - 0.75f * m->conf[i], 0.25f, 1.0f);
    m->evidence[i]++;
    m->last_touched[i] = t;
}

typedef void (*rule_fn)(cinm_map *, size_t, const float *, float, uint32_t);

typedef struct {
    float win;
    float clamp_frac;
    float mean_absw;
} metrics;

static void make_truth(float w_true[NCTX][NFEAT]) {
    for (int c = 0; c < NCTX; c++)
        for (int k = 0; k < NFEAT; k++)
            w_true[c][k] = rnd01() * 2.0f - 1.0f;
}

static void train_once(cinm_map *m, float w_true[NCTX][NFEAT], rule_fn update,
                       uint32_t step, bool reverse, int *conflicts) {
    uint32_t ctx = (uint32_t)(nx() % NCTX);
    float pa[NFEAT], pb[NFEAT], dphi[NFEAT];
    random_features(pa);
    random_features(pb);
    float sa = dot_true(w_true[ctx], pa);
    float sb = dot_true(w_true[ctx], pb);
    const float *win = (sa >= sb) ? pa : pb;
    const float *los = (sa >= sb) ? pb : pa;
    if (reverse) { const float *tmp = win; win = los; los = tmp; }
    for (int k = 0; k < NFEAT; k++)
        dphi[k] = win[k] - los[k];
    size_t i = cinm_address(m, ctx, nullptr);
    if (conflicts && m->evidence[i] > 50 && margin_of(m, i, dphi) < -0.25f)
        (*conflicts)++;
    update(m, i, dphi, 1.0f, step);
    m->t++;
}

static float eval_win(cinm_map *m, float w_true[NCTX][NFEAT]) {
    int hit = 0;
    for (uint32_t c = 0; c < NCTX; c++) {
        size_t i = cinm_address(m, c, nullptr);
        for (int e = 0; e < EVAL; e++) {
            float pa[NFEAT], pb[NFEAT];
            random_features(pa);
            random_features(pb);
            float sa = dot_true(w_true[c], pa), sb = dot_true(w_true[c], pb);
            const float *win = (sa >= sb) ? pa : pb;
            const float *los = (sa >= sb) ? pb : pa;
            hit += cinm_score(m, i, win) > cinm_score(m, i, los);
        }
    }
    return (float)hit / (float)(NCTX * EVAL);
}

static metrics run_rule(rule_fn update, bool confidence) {
    rng_state = SEED;
    float w_true[NCTX][NFEAT];
    make_truth(w_true);
    cinm_map m;
    cinm_init(&m);
    for (uint32_t step = 0; step < NSTEPS; step++)
        train_once(&m, w_true, update, step, false, nullptr);
    metrics out = { eval_win(&m, w_true), 0.0f, 0.0f };
    long total = 0, clamped = 0;
    double sum = 0.0;
    for (size_t i = 0; i < m.count; i++)
        for (int k = 0; k < NFEAT; k++) {
            float a = m.w[i][k] < 0.0f ? -m.w[i][k] : m.w[i][k];
            sum += a;
            clamped += a >= 0.99f * W_MAX;
            total++;
        }
    out.clamp_frac = total ? (float)clamped / (float)total : 0.0f;
    out.mean_absw = total ? (float)(sum / (double)total) : 0.0f;
    (void)confidence;
    return out;
}

static bool negative_reward_pass(void) {
    cinm_map m;
    cinm_init(&m);
    float dphi[NFEAT];
    for (int k = 0; k < NFEAT; k++)
        dphi[k] = 1.0f;
    size_t i = cinm_address(&m, 99, nullptr);
    for (uint32_t t = 0; t < 60; t++)
        cinm_update(&m, i, dphi, 1.0f, t);
    float before = margin_of(&m, i, dphi);
    for (uint32_t t = 60; t < 180; t++)
        cinm_update(&m, i, dphi, -1.0f, t);
    float after = margin_of(&m, i, dphi);
    return before > 0.0f && after < 0.0f && after >= -NFEAT * W_MAX;
}

static bool decay_pass(void) {
    rng_state = SEED;
    float w_true[NCTX][NFEAT];
    make_truth(w_true);
    cinm_map m;
    cinm_init(&m);
    for (uint32_t step = 0; step < NSTEPS; step++)
        train_once(&m, w_true, update_sigma, step, false, nullptr);
    for (size_t i = 0; i < m.count; i++)
        for (int k = 0; k < NFEAT; k++)
            m.w[i][k] *= 0.90f;
    return eval_win(&m, w_true) > 0.80f;
}

static bool conflict_pass(void) {
    rng_state = SEED;
    float w_true[NCTX][NFEAT];
    make_truth(w_true);
    cinm_map m;
    cinm_init(&m);
    int conflicts = 0;
    for (uint32_t step = 0; step < 1800; step++)
        train_once(&m, w_true, update_sigma, step, false, nullptr);
    for (uint32_t step = 1800; step < 2400; step++)
        train_once(&m, w_true, update_sigma, step, true, &conflicts);
    return conflicts > 50;
}

int main(void) {
    metrics c = run_rule(update_const, false);
    metrics s = run_rule(update_sigma, false);
    metrics p = run_rule(update_confidence, true);
    bool neg = negative_reward_pass();
    bool dec = decay_pass();
    bool con = conflict_pass();
    printf("rule          win    clamp  mean_absw\n");
    printf("constant      %.3f  %.3f   %.3f\n", c.win, c.clamp_frac, c.mean_absw);
    printf("sigma         %.3f  %.3f   %.3f\n", s.win, s.clamp_frac, s.mean_absw);
    printf("confidence    %.3f  %.3f   %.3f\n", p.win, p.clamp_frac, p.mean_absw);
    printf("negative reward reversal: %s\n", neg ? "PASS" : "FAIL");
    printf("decay retention: %s\n", dec ? "PASS" : "FAIL");
    printf("conflict quarantine signal: %s\n", con ? "PASS" : "FAIL");
    return (s.win > c.win && s.clamp_frac < c.clamp_frac && neg && dec && con) ? 0 : 1;
}
