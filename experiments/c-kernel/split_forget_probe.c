/* split_forget_probe.c — the doc-18 forgetting probe for D019 cell-splitting. A
 * non-stationary taste stream: learn taste A, then a second taste B whose contexts fall
 * within A's novelty radius (so B collapses onto A's cell and overwrites it — catastrophic
 * interference), then keep seeing BOTH. Measures how well taste A survives, and the revival
 * cost (events to recover A to R of its original win-rate), with splitting OFF vs ON.
 *
 * This is a measurement, not a pass/fail gate. Honest two-way outcome: either splitting
 * isolates B onto a child and protects A under continued interference, or it does not and we
 * record the measured null and name the next lever (lower-confidence / route-by-hidden-context,
 * docs/06). Built only with -DCINM_ENABLE_SPLIT. */
#include "cinm.h"
#include <stdio.h>
#include <stdint.h>

enum { EVAL_TRIALS = 400 };

static uint64_t rng_state;
static uint64_t nx(void) {
    uint64_t z = (rng_state += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}
static float rnd01(void) { return (float)(nx() >> 40) / (float)(1u << 24); }  /* [0,1) */

static void cluster_center(int c, float out[NFEAT]) {
    for (int k = 0; k < NFEAT; k++) out[k] = ((c >> k) & 1) ? 1.0f : 0.0f;
}

static void make_dphi(const float wt[NFEAT], float dphi[NFEAT]) {
    float pa[NFEAT], pb[NFEAT], ta = 0.0f, tb = 0.0f;
    for (int k = 0; k < NFEAT; k++) { pa[k] = rnd01(); pb[k] = rnd01(); ta += wt[k]*pa[k]; tb += wt[k]*pb[k]; }
    const float *win = ta >= tb ? pa : pb, *los = ta >= tb ? pb : pa;
    for (int k = 0; k < NFEAT; k++) dphi[k] = win[k] - los[k];
}

/* One jittered event near `center`, NN-addressed, split-aware adaptive update. */
static void event_split(cinm_map *m, const float center[NFEAT], const float wt[NFEAT],
                        float r2, float jit, uint32_t t) {
    float ctx[NFEAT];
    for (int k = 0; k < NFEAT; k++) ctx[k] = center[k] + (rnd01()*2.0f - 1.0f) * jit;
    float dphi[NFEAT];
    make_dphi(wt, dphi);
    size_t i = cinm_address_nn(m, ctx, r2, nullptr);
    if (i < MAX_CELLS) cinm_update_adaptive_split(m, i, ctx, dphi, 1.0f, t);
}

static size_t nearest_cell(const cinm_map *m, const float ctx[static NFEAT]) {
    size_t best = 0;
    float  bd = 1.0e30f;
    for (size_t i = 0; i < m->count; i++) {
        if (!m->in_use[i]) continue;
        float s = 0.0f;
        for (int k = 0; k < NFEAT; k++) { float d = m->proto[i][k] - ctx[k]; s += d * d; }
        if (s < bd) { bd = s; best = i; }
    }
    return best;
}

/* Win-rate of whatever cell taste-A contexts route to, on its own evaluation RNG so the
 * measurement never perturbs the learning stream (OFF and ON see identical event draws). */
static float winrate_at(const cinm_map *m, const float center[NFEAT], const float wt[NFEAT],
                        float jit, int trials, uint64_t eval_seed) {
    uint64_t saved = rng_state;
    rng_state = eval_seed;
    int correct = 0;
    for (int n = 0; n < trials; n++) {
        float ctx[NFEAT];
        for (int k = 0; k < NFEAT; k++) ctx[k] = center[k] + (rnd01()*2.0f - 1.0f) * jit;
        float pa[NFEAT], pb[NFEAT], ta = 0.0f, tb = 0.0f, sa = 0.0f, sb = 0.0f;
        for (int k = 0; k < NFEAT; k++) { pa[k] = rnd01(); pb[k] = rnd01(); ta += wt[k]*pa[k]; tb += wt[k]*pb[k]; }
        size_t i = nearest_cell(m, ctx);
        for (int k = 0; k < NFEAT; k++) { sa += m->w[i][k]*pa[k]; sb += m->w[i][k]*pb[k]; }
        if ((ta >= tb) == (sa >= sb)) correct++;
    }
    rng_state = saved;
    return (float)correct / (float)trials;
}

typedef struct {
    float    w0;                /* taste-A win-rate after learning A alone                 */
    float    retain_immediate;  /* taste-A win-rate right after interference + consolidate  */
    float    retain_final;      /* taste-A win-rate after the revival budget                */
    int      revival_events;    /* events of continued stream to recover A to R*w0 (NR = never) */
    uint32_t splits;            /* splits performed at consolidation                        */
    size_t   cells;             /* cells after consolidation                                */
} probe_result;

/* Run the whole non-stationary episode under one splitting policy. */
static probe_result run_episode(const cinm_consolidate_policy *pol, uint64_t seed,
                                const float cA[NFEAT], const float cB[NFEAT],
                                const float wtA[NFEAT], const float wtB[NFEAT],
                                float r2, float jit,
                                int na, int nb, int nr, int chunk, float revive_R) {
    constexpr uint64_t EVAL_SEED = 0x7E57A11C7E57A11CULL;
    cinm_map m;
    rng_state = seed;
    cinm_init(&m);
    m.t = 1;
    uint32_t t = 1;

    for (int e = 0; e < na; e++) { event_split(&m, cA, wtA, r2, jit, t); m.t = ++t; }   /* learn A */
    float w0 = winrate_at(&m, cA, wtA, jit, EVAL_TRIALS, EVAL_SEED);

    for (int e = 0; e < nb; e++) {                                                       /* interfere with B */
        bool b = e & 1;
        event_split(&m, b ? cB : cA, b ? wtB : wtA, r2, jit, t);
        m.t = ++t;
    }

    cinm_consolidate_result r = cinm_consolidate(&m, pol, m.t, m.t);
    float retain_immediate = winrate_at(&m, cA, wtA, jit, EVAL_TRIALS, EVAL_SEED);

    int revival = nr;                                                                    /* revive under ongoing A+B */
    for (int e = 0; e < nr; e++) {
        bool b = e & 1;
        event_split(&m, b ? cB : cA, b ? wtB : wtA, r2, jit, t);
        m.t = ++t;
        if ((e % chunk) == chunk - 1 && revival == nr
            && winrate_at(&m, cA, wtA, jit, EVAL_TRIALS, EVAL_SEED) >= revive_R * w0)
            revival = e + 1;
    }
    float retain_final = winrate_at(&m, cA, wtA, jit, EVAL_TRIALS, EVAL_SEED);

    return (probe_result){ .w0 = w0, .retain_immediate = retain_immediate, .retain_final = retain_final,
                           .revival_events = revival, .splits = r.split, .cells = m.count };
}

int main(void) {
    constexpr float R2  = 0.50f;
    constexpr float JIT = 0.05f;
    constexpr float REVIVE_R = 0.85f;
    constexpr uint32_t SMC = 100;
    constexpr uint32_t SME = 200;
    constexpr float SDF = 0.02f;
    constexpr uint64_t SEED = 0xF0F6E77E1F0F6E77ULL;
    enum { NA = 400, NB = 1200, NR = 1200, CHUNK = 100 };

    rng_state = 0xD1CE5EEDD1CE5EEDULL;
    float wtA[NFEAT], wtB[NFEAT];
    for (int k = 0; k < NFEAT; k++) { wtA[k] = rnd01()*2.0f - 1.0f; wtB[k] = -wtA[k]; }

    float cA[NFEAT], cB[NFEAT];
    cluster_center(0, cA);
    for (int k = 0; k < NFEAT; k++) cB[k] = cA[k];
    cB[0] += 0.5f;

    const cinm_consolidate_policy off = {
        .evict_floor = 0.0f, .evict_conf_max = -1.0f, .evict_idle_age = UINT32_MAX,
        .promote_conf = 2.0f, .promote_evidence = UINT32_MAX, .merge_radius2 = 0.0f,
        .split_min_conflict = 0,
    };
    const cinm_consolidate_policy on = {
        .evict_floor = 0.0f, .evict_conf_max = -1.0f, .evict_idle_age = UINT32_MAX,
        .promote_conf = 2.0f, .promote_evidence = UINT32_MAX, .merge_radius2 = 0.0f,
        .split_min_conflict = SMC, .split_min_evidence = SME, .split_dir_floor2 = SDF,
    };

    probe_result off_r = run_episode(&off, SEED, cA, cB, wtA, wtB, R2, JIT, NA, NB, NR, CHUNK, REVIVE_R);
    probe_result on_r  = run_episode(&on,  SEED, cA, cB, wtA, wtB, R2, JIT, NA, NB, NR, CHUNK, REVIVE_R);

    /* Verdict: did splitting protect taste A under continued interference? Require a clear
     * retention gain and at least no worse revival cost. Honest either way. */
    bool protected_final = on_r.retain_final >= off_r.retain_final + 0.15f;
    bool revival_better  = on_r.revival_events <= off_r.revival_events;
    bool forgetting_protected = protected_final && revival_better && on_r.splits >= 1;

    printf("forgetting probe (D019 split), non-stationary A->B->A+B, REVIVE_R=%.2f\n", (double)REVIVE_R);
    printf("  %-22s  %8s  %8s\n", "metric", "split-OFF", "split-ON");
    printf("  %-22s  %8.3f  %8.3f\n", "w0 (A learned)",       (double)off_r.w0,               (double)on_r.w0);
    printf("  %-22s  %8.3f  %8.3f\n", "A retain (immediate)", (double)off_r.retain_immediate, (double)on_r.retain_immediate);
    printf("  %-22s  %8.3f  %8.3f\n", "A retain (final)",     (double)off_r.retain_final,     (double)on_r.retain_final);
    printf("  %-22s  %8d  %8d\n",     "revival events (<=NR)", off_r.revival_events,          on_r.revival_events);
    printf("  %-22s  %8u  %8u\n",     "splits",               off_r.splits,                   on_r.splits);
    printf("  %-22s  %8zu  %8zu\n",   "cells",                off_r.cells,                    on_r.cells);
    printf("verdict: %s\n", forgetting_protected
           ? "PROTECTED — splitting isolates B and preserves A under continued interference"
           : "NULL — splitting did not measurably protect A (record + next lever, docs/06)");

    /* Measurement probe: exit 0 once it runs and the contrast is computed. The verdict is
     * recorded (bundle summary), not gated — a measured null is a result, not a failure. */
    return 0;
}
