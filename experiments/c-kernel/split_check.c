/* split_check.c — prove D019 cell-splitting, the inverse of merge consolidation. When one
 * nearest-neighbour address is forced to carry two address-separable schemas with opposite
 * preferences, a single cell under-fits (its blended w predicts neither). The deferred split
 * pass forks the dissenting sub-population into a child placed along the contradiction
 * direction; the pair then recovers win-rate the torn cell could not. And it correctly
 * DECLINES to split when the two sub-populations are aliased (same address, opposite reward):
 * address-splitting cannot separate what shares an address. Behavioral proof, not byte-exact —
 * splitting changes kernel state by design (D019). Built only with -DCINM_ENABLE_SPLIT. */
#include "cinm.h"
#include "cinm_ledger.h"
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

/* One pairwise-preference contrast under ground-truth wt: draw two options, dphi points from
 * loser to winner. Consumes 2*NFEAT draws — call it in a fixed order to stay deterministic. */
static void make_dphi(const float wt[NFEAT], float dphi[NFEAT]) {
    float pa[NFEAT], pb[NFEAT], ta = 0.0f, tb = 0.0f;
    for (int k = 0; k < NFEAT; k++) { pa[k] = rnd01(); pb[k] = rnd01(); ta += wt[k]*pa[k]; tb += wt[k]*pb[k]; }
    const float *win = ta >= tb ? pa : pb, *los = ta >= tb ? pb : pa;
    for (int k = 0; k < NFEAT; k++) dphi[k] = win[k] - los[k];
}

/* One jittered event near `center`: NN-address it, then a split-aware adaptive update (which
 * accumulates the contradiction signal). Draw order: jitter (NFEAT), then dphi (2*NFEAT). */
static void event_split(cinm_map *m, const float center[NFEAT], const float wt[NFEAT],
                        float r2, float jit, uint32_t t) {
    float ctx[NFEAT];
    for (int k = 0; k < NFEAT; k++) ctx[k] = center[k] + (rnd01()*2.0f - 1.0f) * jit;
    float dphi[NFEAT];
    make_dphi(wt, dphi);
    size_t i = cinm_address_nn(m, ctx, r2, nullptr);
    if (i < MAX_CELLS) cinm_update_adaptive_split(m, i, ctx, dphi, 1.0f, t);
}

/* Build the under-fit fixture: K events interleaving two schemas (centers cA/cB, opposite
 * preferences wtA/wtB) onto whatever cell NN addressing picks. With cB within the novelty
 * radius of cA, both collapse onto one cell, which is torn and predicts neither. Resets the
 * RNG so the same seed reproduces the same map. */
static void build_torn(cinm_map *m, uint64_t seed, int K,
                       const float cA[NFEAT], const float cB[NFEAT],
                       const float wtA[NFEAT], const float wtB[NFEAT], float r2, float jit) {
    rng_state = seed;
    cinm_init(m);
    m->t = 1;
    for (int e = 0; e < K; e++) {
        bool b = e & 1;
        event_split(m, b ? cB : cA, b ? wtB : wtA, r2, jit, (uint32_t)(e + 1));
        m->t = (uint32_t)(e + 2);
    }
}

/* Post-consolidation re-learning: the same interleaved stream, NN-addressed. After a split, A
 * events route to the parent and B events to the child, so each cell learns one schema. */
static void relearn(cinm_map *m, int R, const float cA[NFEAT], const float cB[NFEAT],
                    const float wtA[NFEAT], const float wtB[NFEAT], float r2, float jit, uint32_t t0) {
    for (int e = 0; e < R; e++) {
        bool b = e & 1;
        event_split(m, b ? cB : cA, b ? wtB : wtA, r2, jit, t0 + (uint32_t)e);
        m->t = t0 + (uint32_t)e + 1;
    }
}

/* Read-only nearest in-use cell (no allocation, no RNG): how an eval context would route. */
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

/* Combined win-rate over both schemas: route each test context to its nearest cell and ask
 * whether that cell's w predicts the winner under the schema's ground truth. A torn single
 * cell scores ~0.5 (chance); a correctly split pair scores high. */
static float combined_winrate(const cinm_map *m, const float cA[NFEAT], const float cB[NFEAT],
                              const float wtA[NFEAT], const float wtB[NFEAT], float jit, int trials) {
    int correct = 0;
    for (int n = 0; n < trials; n++) {
        bool b = n & 1;
        const float *center = b ? cB : cA, *wt = b ? wtB : wtA;
        float ctx[NFEAT];
        for (int k = 0; k < NFEAT; k++) ctx[k] = center[k] + (rnd01()*2.0f - 1.0f) * jit;
        float pa[NFEAT], pb[NFEAT], ta = 0.0f, tb = 0.0f, sa = 0.0f, sb = 0.0f;
        for (int k = 0; k < NFEAT; k++) { pa[k] = rnd01(); pb[k] = rnd01(); ta += wt[k]*pa[k]; tb += wt[k]*pb[k]; }
        size_t i = nearest_cell(m, ctx);
        for (int k = 0; k < NFEAT; k++) { sa += m->w[i][k]*pa[k]; sb += m->w[i][k]*pb[k]; }
        if ((ta >= tb) == (sa >= sb)) correct++;
    }
    return (float)correct / (float)trials;
}

static float dir2_of(const cinm_map *m, size_t i) {
    float s = 0.0f;
    for (int k = 0; k < NFEAT; k++) s += m->conflict_dir[i][k] * m->conflict_dir[i][k];
    return s;
}
static float cell_dist2(const cinm_map *m, size_t a, size_t b) {
    float s = 0.0f;
    for (int k = 0; k < NFEAT; k++) { float d = m->proto[a][k] - m->proto[b][k]; s += d * d; }
    return s;
}
static bool protos_finite(const cinm_map *m) {
    for (size_t i = 0; i < m->count; i++) {
        if (!m->in_use[i]) continue;
        for (int k = 0; k < NFEAT; k++) {
            float v = m->proto[i][k];
            if (v != v || absf(v) > 1.0e6f) return false;
        }
    }
    return true;
}

int main(void) {
    constexpr float R2       = 0.50f;   /* novelty radius^2: cB (0.25 from cA) collapses onto cA's cell */
    constexpr float JIT      = 0.05f;   /* per-feature context jitter (norm^2 ~0.007 << 0.25 separation) */
    constexpr float MERGE_R2 = 0.40f;   /* > dist2(parent,child): would re-fold them absent the lock     */
    constexpr uint32_t SMC   = 100;     /* split_min_conflict                                            */
    constexpr uint32_t SME   = 200;     /* split_min_evidence                                            */
    constexpr float SDF      = 0.02f;   /* split_dir_floor2: separable >> this, aliased << this          */
    constexpr uint64_t BUILD_SEED   = 0x5170FCE115170FCEULL;
    constexpr uint64_t RELEARN_SEED = 0x2EA12117D2EA1211ULL;
    enum { K = 2000, RELEARN = 1200 };

    /* opposite ground-truth preferences */
    rng_state = 0xD1CE5EEDD1CE5EEDULL;
    float wtA[NFEAT], wtB[NFEAT];
    for (int k = 0; k < NFEAT; k++) { wtA[k] = rnd01()*2.0f - 1.0f; wtB[k] = -wtA[k]; }

    float cA[NFEAT], cB[NFEAT];
    cluster_center(0, cA);
    for (int k = 0; k < NFEAT; k++) cB[k] = cA[k];
    cB[0] += 0.5f;                                   /* dist2(cA,cB) = 0.25 in (jitter, R2) */

    const cinm_consolidate_policy off = {
        .evict_floor = 0.0f, .evict_conf_max = -1.0f, .evict_idle_age = UINT32_MAX,
        .promote_conf = 2.0f, .promote_evidence = UINT32_MAX, .merge_radius2 = 0.0f,
        .split_min_conflict = 0,                     /* split off */
    };
    const cinm_consolidate_policy on = {
        .evict_floor = 0.0f, .evict_conf_max = -1.0f, .evict_idle_age = UINT32_MAX,
        .promote_conf = 2.0f, .promote_evidence = UINT32_MAX, .merge_radius2 = 0.0f,
        .split_min_conflict = SMC, .split_min_evidence = SME, .split_dir_floor2 = SDF,
    };
    const cinm_consolidate_policy on_merge = {     /* merge + split both on: exercises the lock */
        .evict_floor = 0.0f, .evict_conf_max = -1.0f, .evict_idle_age = UINT32_MAX,
        .promote_conf = 2.0f, .promote_evidence = UINT32_MAX, .merge_radius2 = MERGE_R2,
        .split_min_conflict = SMC, .split_min_evidence = SME, .split_dir_floor2 = SDF,
    };

    /* shared pre-consolidation torn state */
    cinm_map M;
    build_torn(&M, BUILD_SEED, K, cA, cB, wtA, wtB, R2, JIT);
    uint32_t bc = M.conflict[0], be = M.evidence[0];
    float    bdir2 = dir2_of(&M, 0);
    cinm_map M0;
    cinm_snapshot(&M, &M0);

    /* ---- assertion 1: baseline collapses (split OFF) ---- */
    cinm_map Mb;
    cinm_restore(&Mb, &M0);
    cinm_consolidate_result rb = cinm_consolidate(&Mb, &off, Mb.t, Mb.t);
    rng_state = RELEARN_SEED;
    relearn(&Mb, RELEARN, cA, cB, wtA, wtB, R2, JIT, Mb.t);
    float wr_off = combined_winrate(&Mb, cA, cB, wtA, wtB, JIT, EVAL_TRIALS);
    bool baseline_collapsed = Mb.count == 1 && rb.split == 0 && wr_off < 0.65f;

    /* ---- assertion 2: split recovers (split ON) ---- */
    cinm_map Mo;
    cinm_restore(&Mo, &M0);
    cinm_consolidate_result ro = cinm_consolidate(&Mo, &on, Mo.t, Mo.t);
    uint32_t on_t = Mo.t, on_epoch = Mo.epoch, on_base = Mo.base_seq;   /* receipt, before re-learn */
    rng_state = RELEARN_SEED;
    relearn(&Mo, RELEARN, cA, cB, wtA, wtB, R2, JIT, Mo.t);
    float wr_on = combined_winrate(&Mo, cA, cB, wtA, wtB, JIT, EVAL_TRIALS);
    bool split_recovers = ro.split == 1 && Mo.count == 2 && wr_on >= 0.85f && wr_on >= 1.5f * wr_off;

    /* ---- assertion 3: determinism (carries reproduce, new fields included in cinm_equal) ---- */
    cinm_map D1, D2;
    build_torn(&D1, BUILD_SEED, K, cA, cB, wtA, wtB, R2, JIT);
    (void)cinm_consolidate(&D1, &on, D1.t, D1.t);
    build_torn(&D2, BUILD_SEED, K, cA, cB, wtA, wtB, R2, JIT);
    (void)cinm_consolidate(&D2, &on, D2.t, D2.t);
    bool determinism = cinm_equal(&D1, &D2);

    /* ---- assertion 4: no oscillation (locked child survives a second consolidation) ---- */
    cinm_map Os;
    build_torn(&Os, BUILD_SEED, K, cA, cB, wtA, wtB, R2, JIT);
    cinm_consolidate_result o1 = cinm_consolidate(&Os, &on_merge, Os.t, Os.t);
    float pc_d2 = Os.count == 2 ? cell_dist2(&Os, 0, 1) : -1.0f;
    cinm_consolidate_result o2 = cinm_consolidate(&Os, &on_merge, Os.t, Os.t);  /* no new evidence */
    bool no_oscillation = o1.split == 1 && Os.count == 2
                       && o2.split == 0 && o2.merged == 0
                       && pc_d2 > 0.0f && pc_d2 < MERGE_R2;     /* non-vacuous: merge WOULD have folded */

    /* ---- assertion 5: frozen cells are split-exempt ---- */
    cinm_map Fz;
    build_torn(&Fz, BUILD_SEED, K, cA, cB, wtA, wtB, R2, JIT);
    Fz.frozen[0] = true;
    cinm_consolidate_result rf = cinm_consolidate(&Fz, &on, Fz.t, Fz.t);
    bool frozen_exempt = rf.split == 0 && Fz.count == 1;

    /* ---- assertion 6: separability null — aliased schemas (same address) decline to split ---- */
    cinm_map Al;
    build_torn(&Al, BUILD_SEED, K, cA, cA, wtA, wtB, R2, JIT);   /* both schemas at cA */
    uint32_t ac = Al.conflict[0], ae = Al.evidence[0];
    float    adir2 = dir2_of(&Al, 0);
    cinm_consolidate_result ra = cinm_consolidate(&Al, &on, Al.t, Al.t);
    float wr_alias = combined_winrate(&Al, cA, cA, wtA, wtB, JIT, EVAL_TRIALS);
    bool separability_null = ra.split == 0 && ra.split_suppressed == 1 && Al.count == 1 && wr_alias < 0.65f;

    /* ---- assertion 7: bounds + honest receipt (all four structural counts) ---- */
    cinm_ledger led;
    cinm_ledger_init(&led);
    cinm_decision rec = { .t = on_t, .epoch = on_epoch, .kind = CINM_DECISION_CONSOLIDATE,
                          .info_a = ro.evicted, .info_b = ro.promoted,
                          .info_c = ro.merged,  .info_d = ro.split, .base_seq = on_base };
    bool logged = cinm_ledger_append(&led, &rec);
    bool bounds_receipt = logged
                       && led.items[0].info_d == ro.split && led.items[0].info_c == ro.merged
                       && Mo.count <= MAX_CELLS && D1.count <= MAX_CELLS
                       && protos_finite(&Mo) && protos_finite(&Os);

    /* ---- report ---- */
    printf("[build] cell0 conflict=%u evidence=%u rate=%.3f dir2=%.4f  (aliased dir2=%.4f)\n",
           bc, be, be ? (double)bc / (double)be : 0.0, (double)bdir2, (double)adir2);
    printf("[alias] cell0 conflict=%u evidence=%u rate=%.3f\n",
           ac, ae, ae ? (double)ac / (double)ae : 0.0);
    printf("baseline collapses (1 torn cell, split off): %s (count=%zu wr_off=%.3f)\n",
           baseline_collapsed ? "PASS" : "FAIL", Mb.count, (double)wr_off);
    printf("split recovers under-fit win-rate: %s (split=%u count=%zu wr_on=%.3f ratio=%.2f)\n",
           split_recovers ? "PASS" : "FAIL", ro.split, Mo.count, (double)wr_on,
           wr_off > 0.0f ? (double)(wr_on / wr_off) : 0.0);
    printf("determinism (split carries reproduce): %s\n", determinism ? "PASS" : "FAIL");
    printf("no oscillation (locked child survives re-consolidation): %s (split2=%u merged2=%u d2=%.4f)\n",
           no_oscillation ? "PASS" : "FAIL", o2.split, o2.merged, (double)pc_d2);
    printf("frozen cells split-exempt: %s\n", frozen_exempt ? "PASS" : "FAIL");
    printf("separability null (aliased declines, suppressed): %s (split=%u suppressed=%u wr=%.3f)\n",
           separability_null ? "PASS" : "FAIL", ra.split, ra.split_suppressed, (double)wr_alias);
    printf("bounds + honest receipt (info_c/info_d): %s\n", bounds_receipt ? "PASS" : "FAIL");

    cinm_ledger_free(&led);
    bool all = baseline_collapsed && split_recovers && determinism && no_oscillation
            && frozen_exempt && separability_null && bounds_receipt;
    return all ? 0 : 1;
}
