/* nn_address_check.c — prove continuous nearest-neighbour / prototype addressing (D019)
 * and R3.5 merge consolidation on a neutral task (contextual preference with contexts
 * that repeat with variation, doc 03). No drum dependency. Shows: NN addressing is
 * deterministic; jittered contexts cluster instead of fragmenting (the thing exact keys
 * cannot do); novelty is a radius decision; radius-0 degenerates to exact-key learning;
 * consolidation merges near-duplicate prototypes, lossily, faithfully, and reproducibly. */
#include "cinm.h"
#include "cinm_ledger.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

enum { EVAL_TRIALS = 400 };

static uint64_t rng_state;
static uint64_t nx(void) {
    uint64_t z = (rng_state += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}
static float rnd01(void) { return (float)(nx() >> 40) / (float)(1u << 24); }  /* [0,1) */

/* Cluster center c as a binary code across the first features: distinct clusters differ by
 * >= 1.0 per differing feature, so they are far apart (sq-dist >= 1.0) while jitter stays
 * tiny — separable under a small novelty radius. */
static void cluster_center(int c, float out[NFEAT]) {
    for (int k = 0; k < NFEAT; k++) out[k] = ((c >> k) & 1) ? 1.0f : 0.0f;
}

static void one_hot(float out[NFEAT], int idx) {
    for (int k = 0; k < NFEAT; k++) out[k] = (k == idx) ? 1.0f : 0.0f;
}

/* Exact-key view of a continuous context: every distinct float vector is its own key.
 * This is what "exact-symbolic addressing on continuous contexts" does — it cannot absorb
 * variation, so each jittered context fragments into a new cell. FNV-1a over the bytes. */
static uint32_t hash_ctx(const float ctx[static NFEAT]) {
    uint32_t h = 2166136261u;
    const unsigned char *b = (const unsigned char *)ctx;
    for (size_t n = 0; n < sizeof(float) * NFEAT; n++) { h ^= b[n]; h *= 16777619u; }
    return h;
}

/* One pairwise preference trial against ground-truth wt, applied to cell i via the adaptive
 * (confidence-raising) update. */
static void train_idx(cinm_map *m, size_t i, const float wt[NFEAT], int n, uint32_t at) {
    for (int s = 0; s < n; s++) {
        float pa[NFEAT], pb[NFEAT], ta = 0.0f, tb = 0.0f, dphi[NFEAT];
        for (int k = 0; k < NFEAT; k++) { pa[k] = rnd01(); pb[k] = rnd01(); ta += wt[k]*pa[k]; tb += wt[k]*pb[k]; }
        const float *win = ta >= tb ? pa : pb, *los = ta >= tb ? pb : pa;
        for (int k = 0; k < NFEAT; k++) dphi[k] = win[k] - los[k];
        cinm_update_adaptive(m, i, dphi, 1.0f, at + (uint32_t)s);
    }
}

static float winrate(const cinm_map *m, size_t i, const float wt[NFEAT], int trials) {
    int correct = 0;
    for (int n = 0; n < trials; n++) {
        float pa[NFEAT], pb[NFEAT], ta = 0.0f, tb = 0.0f, sa = 0.0f, sb = 0.0f;
        for (int k = 0; k < NFEAT; k++) { pa[k] = rnd01(); pb[k] = rnd01(); ta += wt[k]*pa[k]; tb += wt[k]*pb[k]; }
        for (int k = 0; k < NFEAT; k++) { sa += m->w[i][k]*pa[k]; sb += m->w[i][k]*pb[k]; }
        if ((ta >= tb) == (sa >= sb)) correct++;
    }
    return (float)correct / (float)trials;
}

/* One jittered contextual-preference event: a fresh context near cluster c (within the
 * novelty radius), NN-addressed, then one pairwise update on that cluster's preference. */
static void event_nn(cinm_map *m, int c, const float wt[NFEAT], float radius2, float jit, uint32_t at) {
    float ctx[NFEAT];
    cluster_center(c, ctx);
    for (int k = 0; k < NFEAT; k++) ctx[k] += (rnd01()*2.0f - 1.0f) * jit;
    size_t i = cinm_address_nn(m, ctx, radius2, nullptr);
    if (i >= MAX_CELLS) return;
    float pa[NFEAT], pb[NFEAT], ta = 0.0f, tb = 0.0f, dphi[NFEAT];
    for (int k = 0; k < NFEAT; k++) { pa[k] = rnd01(); pb[k] = rnd01(); ta += wt[k]*pa[k]; tb += wt[k]*pb[k]; }
    const float *win = ta >= tb ? pa : pb, *los = ta >= tb ? pb : pa;
    for (int k = 0; k < NFEAT; k++) dphi[k] = win[k] - los[k];
    cinm_update_adaptive(m, i, dphi, 1.0f, at);
}

/* NN stream: K jittered events round-robin over C clusters. Resets the RNG so the same
 * seed reproduces the same map. */
static void build_clusters(cinm_map *m, uint64_t seed, int K, int C,
                           const float wt[][NFEAT], float radius2, float jit) {
    rng_state = seed;
    cinm_init(m);
    m->t = 1;
    for (int e = 0; e < K; e++) {
        int c = e % C;
        event_nn(m, c, wt[c], radius2, jit, (uint32_t)(e + 1));
        m->t = (uint32_t)(e + 2);
    }
}

/* The same stream under exact-key addressing (each jittered context hashed to its own key),
 * consuming the RNG in the same order so only the addressing differs. */
static void build_exact(cinm_map *m, uint64_t seed, int K, int C,
                        const float wt[][NFEAT], float jit) {
    rng_state = seed;
    cinm_init(m);
    m->t = 1;
    for (int e = 0; e < K; e++) {
        int c = e % C;
        float ctx[NFEAT];
        cluster_center(c, ctx);
        for (int k = 0; k < NFEAT; k++) ctx[k] += (rnd01()*2.0f - 1.0f) * jit;
        size_t i = cinm_address(m, hash_ctx(ctx), nullptr);
        if (i >= MAX_CELLS) { /* saturated: still consume the pairwise draws to stay aligned */
            for (int k = 0; k < 2*NFEAT; k++) (void)rnd01();
            m->t = (uint32_t)(e + 2);
            continue;
        }
        float pa[NFEAT], pb[NFEAT], ta = 0.0f, tb = 0.0f, dphi[NFEAT];
        for (int k = 0; k < NFEAT; k++) { pa[k] = rnd01(); pb[k] = rnd01(); ta += wt[c][k]*pa[k]; tb += wt[c][k]*pb[k]; }
        const float *win = ta >= tb ? pa : pb, *los = ta >= tb ? pb : pa;
        for (int k = 0; k < NFEAT; k++) dphi[k] = win[k] - los[k];
        cinm_update_adaptive(m, i, dphi, 1.0f, (uint32_t)(e + 1));
        m->t = (uint32_t)(e + 2);
    }
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

/* Build the two-near-duplicate-prototypes fixture (cells at ca and ca+0.5*e0: sq-dist 0.25,
 * separate under radius2=0.10, mergeable under merge_radius2=0.40), each trained on wt. */
static void build_merge_fixture(cinm_map *m, uint64_t seed, const float wt[NFEAT],
                                float radius2, int n, size_t *ia, size_t *ib) {
    rng_state = seed;
    cinm_init(m);
    m->t = 1;
    float ca[NFEAT], cb[NFEAT];
    cluster_center(0, ca);
    for (int k = 0; k < NFEAT; k++) cb[k] = ca[k];
    cb[0] += 0.5f;
    *ia = cinm_address_nn(m, ca, radius2, nullptr);
    *ib = cinm_address_nn(m, cb, radius2, nullptr);
    train_idx(m, *ia, wt, n, 1);
    train_idx(m, *ib, wt, n, (uint32_t)(1 + n));
    m->t = (uint32_t)(1 + 2*n);
}

int main(void) {
    constexpr float R2       = 0.10f;   /* novelty radius^2: jitter (<=~0.02) stays inside  */
    constexpr float JIT      = 0.05f;   /* per-feature context jitter                       */
    constexpr float MERGE_R2 = 0.40f;   /* merge folds prototypes closer than this          */
    constexpr uint64_t STREAM_SEED = 0x0ABCDEF123456789ULL;
    constexpr uint64_t LEARN_SEED  = 0x1234567890ABCDEFULL;
    constexpr uint64_t MERGE_SEED  = 0x0F0E0D0C0B0A0908ULL;
    enum { C = 4, K = 2000, NL = 200, NM = 300 };

    /* ground-truth preference per cluster (fixed, deterministic) */
    rng_state = 0xD1CE5EEDD1CE5EEDULL;
    float wt[C][NFEAT];
    for (int c = 0; c < C; c++)
        for (int k = 0; k < NFEAT; k++) wt[c][k] = rnd01()*2.0f - 1.0f;

    /* ---- check 1: NN addressing is deterministic ---- */
    cinm_map A1, A2;
    build_clusters(&A1, STREAM_SEED, K, C, wt, R2, JIT);
    build_clusters(&A2, STREAM_SEED, K, C, wt, R2, JIT);
    bool deterministic = cinm_equal(&A1, &A2);

    /* ---- check 2: clusters, no over-fragmentation (the headline) ---- */
    cinm_map EX;
    build_exact(&EX, STREAM_SEED, K, C, wt, JIT);
    bool no_frag = A1.count == (size_t)C && EX.count >= (size_t)C * 8;

    /* ---- check 3: novelty is a radius decision (allocate vs reuse) ---- */
    cinm_map NV;
    cinm_init(&NV);
    NV.t = 1;
    float c0[NFEAT];
    cluster_center(0, c0);
    bool nv0 = false;
    size_t i0 = cinm_address_nn(&NV, c0, R2, &nv0);          /* empty map -> novel */
    float near[NFEAT];
    for (int k = 0; k < NFEAT; k++) near[k] = c0[k] + 0.02f; /* sq-dist 0.0032 < R2 -> reuse */
    bool nv_near = false;
    size_t i_near = cinm_address_nn(&NV, near, R2, &nv_near);
    float far[NFEAT];
    cluster_center(1, far);                                  /* sq-dist >= 1.0 > R2 -> novel */
    bool nv_far = false;
    size_t i_far = cinm_address_nn(&NV, far, R2, &nv_far);
    bool novelty_ok = nv0 && i0 == 0 && !nv_near && i_near == i0 && nv_far && i_far != i0;

    /* ---- check 4: radius-0 NN reproduces exact-key learning ---- */
    float oh0[NFEAT], oh1[NFEAT];
    one_hot(oh0, 0);
    one_hot(oh1, 1);
    cinm_map ZN;
    cinm_init(&ZN);
    ZN.t = 1;
    size_t zi = cinm_address_nn(&ZN, oh0, 0.0f, nullptr);
    rng_state = LEARN_SEED;
    train_idx(&ZN, zi, wt[0], NL, 1);
    bool reuse0 = false;
    size_t zi2 = cinm_address_nn(&ZN, oh0, 0.0f, &reuse0);   /* identical proto -> reuse */
    bool novel1 = false;
    (void)cinm_address_nn(&ZN, oh1, 0.0f, &novel1);          /* different proto -> novel */

    cinm_map ZE;
    cinm_init(&ZE);
    ZE.t = 1;
    size_t ei = cinm_address(&ZE, 42, nullptr);
    rng_state = LEARN_SEED;
    train_idx(&ZE, ei, wt[0], NL, 1);
    bool generalizes = memcmp(ZN.w[zi], ZE.w[ei], sizeof ZN.w[zi]) == 0
                    && !reuse0 && zi2 == zi && novel1;

    /* ---- check 5 + 6: merge folds near-duplicate prototypes (faithful, lossy, deterministic) ---- */
    const cinm_consolidate_policy mpol = {
        .evict_floor = 0.0f, .evict_conf_max = -1.0f, .evict_idle_age = UINT32_MAX,
        .promote_conf = 2.0f, .promote_evidence = UINT32_MAX,   /* evict/freeze off: isolate merge */
        .merge_radius2 = MERGE_R2,
    };
    size_t ia, ib;
    cinm_map MG;
    build_merge_fixture(&MG, MERGE_SEED, wt[0], R2, NM, &ia, &ib);
    size_t count_before = MG.count;
    float wr_a = winrate(&MG, ia, wt[0], EVAL_TRIALS);
    float wr_b = winrate(&MG, ib, wt[0], EVAL_TRIALS);
    float wr_min = wr_a < wr_b ? wr_a : wr_b;

    cinm_map pre;
    cinm_snapshot(&MG, &pre);                                /* pre-merge checkpoint */
    cinm_consolidate_result mr = cinm_consolidate(&MG, &mpol, MG.t, MG.t);

    cinm_ledger led;
    cinm_ledger_init(&led);
    cinm_decision rec = { .t = MG.t, .epoch = MG.epoch, .kind = CINM_DECISION_CONSOLIDATE,
                          .info_a = mr.merged, .info_b = 0, .base_seq = MG.base_seq };
    bool logged = cinm_ledger_append(&led, &rec);

    size_t count_after = MG.count;
    float wr_after = winrate(&MG, 0, wt[0], EVAL_TRIALS);    /* ia survived as cell 0 */
    bool merge_ok = mr.merged == 1 && count_after == count_before - 1 && logged
                 && wr_after >= 0.85f * wr_min;

    /* lossy + deterministic: construction reproduces; merge reproduces; a cell was lost */
    size_t ja, jb;
    cinm_map MG2;
    build_merge_fixture(&MG2, MERGE_SEED, wt[0], R2, NM, &ja, &jb);
    bool rebuilt = cinm_equal(&MG2, &pre);
    cinm_consolidate_result mr2 = cinm_consolidate(&MG2, &mpol, pre.t, pre.t);
    bool merge_det = mr2.merged == mr.merged && cinm_equal(&MG2, &MG);
    bool lossy_det = rebuilt && merge_det && count_after < count_before;

    /* ---- check 7: bounds ---- */
    bool bounds = A1.count <= MAX_CELLS && EX.count <= MAX_CELLS && MG.count <= MAX_CELLS
               && protos_finite(&A1) && protos_finite(&MG);

    /* ---- report ---- */
    printf("NN addressing deterministic: %s\n", deterministic ? "PASS" : "FAIL");
    printf("clusters, no over-fragmentation: %s (nn cells=%zu==C, exact-key cells=%zu)\n",
           no_frag ? "PASS" : "FAIL", A1.count, EX.count);
    printf("novelty is a radius decision (allocate vs reuse): %s\n", novelty_ok ? "PASS" : "FAIL");
    printf("radius-0 NN reproduces exact-key learning: %s\n", generalizes ? "PASS" : "FAIL");
    printf("merge folds near-duplicate prototypes (faithful): %s (merged=%u count %zu->%zu wr %.3f->%.3f)\n",
           merge_ok ? "PASS" : "FAIL", mr.merged, count_before, count_after,
           (double)wr_min, (double)wr_after);
    printf("merge is lossy + deterministic: %s\n", lossy_det ? "PASS" : "FAIL");
    printf("bounds (count<=MAX_CELLS, protos finite): %s\n", bounds ? "PASS" : "FAIL");

    cinm_ledger_free(&led);
    bool all = deterministic && no_frag && novelty_ok && generalizes && merge_ok && lossy_det && bounds;
    return all ? 0 : 1;
}
