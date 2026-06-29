/* consolidate_check.c — prove R3 lossy consolidation (D018): evict dead cells,
 * freeze strong ones, retain active cells, bounded revival cost (R, K), and that
 * after consolidation full-log replay no longer reconstructs the map while
 * within-epoch replay from a post-consolidation snapshot still does. */
#include "cinm.h"
#include "cinm_log.h"
#include "cinm_ledger.h"
#include <stdio.h>
#include <stdint.h>

/* revival bound: re-learn an evicted context to within REVIVE_R of its pre-eviction
 * win-rate (relative, doc 18) within REVIVE_K trials. */
enum { REVIVE_K = 100, EVAL_TRIALS = 400 };
constexpr float REVIVE_R = 0.85f;   /* recover to >= 85% of pre-eviction win-rate */

static uint64_t rng_state = 0x0ABCDEF123456789ULL;
static uint64_t nx(void) {
    uint64_t z = (rng_state += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}
static float rnd01(void) { return (float)(nx() >> 40) / (float)(1u << 24); }  /* [0,1) */

/* Train cell `key` on a separable pairwise preference defined by wt, via adaptive
 * updates (which raise confidence — pairwise/replay deliberately do not). */
static void train(cinm_map *m, uint32_t key, const float wt[NFEAT], int n, uint32_t at) {
    size_t i = cinm_address(m, key, nullptr);
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

int main(void) {
    const cinm_consolidate_policy pol = {
        .evict_floor = 0.01f, .evict_conf_max = 0.20f, .evict_idle_age = 100,
        .promote_conf = 0.60f, .promote_evidence = 10,
    };

    /* ---- checks 1-3: evict / freeze / count / epoch / frozen stability / active retention ---- */
    cinm_map M;
    cinm_init(&M);
    M.t = 1000;

    size_t s1 = cinm_address(&M, 1, nullptr);                 /* strong -> frozen */
    for (int k = 0; k < NFEAT; k++) M.w[s1][k] = 2.0f;
    M.conf[s1] = 0.9f; M.evidence[s1] = 50; M.last_touched[s1] = 990;

    for (uint32_t key = 2; key <= 3; key++) {              /* dead -> evicted */
        size_t i = cinm_address(&M, key, nullptr);
        for (int k = 0; k < NFEAT; k++) M.w[i][k] = 0.001f;
        M.conf[i] = 0.05f; M.evidence[i] = 1; M.last_touched[i] = 100;
    }

    size_t a1 = cinm_address(&M, 4, nullptr);                 /* active -> kept */
    for (int k = 0; k < NFEAT; k++) M.w[a1][k] = 1.0f;
    M.conf[a1] = 0.30f; M.evidence[a1] = 5; M.last_touched[a1] = 995;
    float active_w_before = M.w[a1][0];

    size_t count_before = M.count;
    cinm_consolidate_result r = cinm_consolidate(&M, &pol, M.t, 1000);
    size_t count_after = M.count;          /* before evicted_gone re-addresses and recreates */

    cinm_ledger led;
    cinm_ledger_init(&led);
    cinm_decision rec = { .t = M.t, .epoch = M.epoch, .kind = CINM_DECISION_CONSOLIDATE,
                          .info_a = r.evicted, .info_b = r.promoted, .base_seq = M.base_seq };
    bool logged = cinm_ledger_append(&led, &rec);

    bool counts = r.evicted == 2 && r.promoted == 1
               && M.count == count_before - 2
               && M.epoch == 1 && M.base_seq == 1000;

    size_t f1 = cinm_address(&M, 1, nullptr);                 /* frozen cell survived */
    bool frozen_ok = M.frozen[f1] && M.plast[f1] <= PLAST_FLOOR + 1e-6f;

    float frozen_w_before = M.w[f1][0];
    cinm_decay(&M, 0.5f);                                  /* frozen exempt; active halved */
    bool frozen_stable = M.w[f1][0] == frozen_w_before;

    size_t k1 = cinm_address(&M, 4, nullptr);
    bool active_kept = M.w[k1][0] == active_w_before * 0.5f;   /* kept by consolidate, then one decay */

    bool n2 = false, n3 = false;
    (void)cinm_address(&M, 2, &n2);
    (void)cinm_address(&M, 3, &n3);
    bool evicted_gone = n2 && n3;                          /* re-addressing recreates -> were gone */

    /* ---- check 4: revival cost (R, K) ---- */
    cinm_map RV;
    cinm_init(&RV);
    RV.t = 1;
    float wt[NFEAT];
    for (int k = 0; k < NFEAT; k++) wt[k] = rnd01() * 2.0f - 1.0f;
    train(&RV, 9, wt, 200, 1);
    size_t ri = cinm_address(&RV, 9, nullptr);
    float w0 = winrate(&RV, ri, wt, EVAL_TRIALS);

    for (int k = 0; k < NFEAT; k++) RV.w[ri][k] = 0.0f;    /* model dormancy: faded, lapsed, idle */
    RV.conf[ri] = 0.05f; RV.last_touched[ri] = 0; RV.t = 100000;
    cinm_consolidate_result rr = cinm_consolidate(&RV, &pol, RV.t, 100000);

    bool revived_novel = false;
    (void)cinm_address(&RV, 9, &revived_novel);            /* evicted, so re-address is novel */
    train(&RV, 9, wt, REVIVE_K, 100001);
    size_t ri2 = cinm_address(&RV, 9, nullptr);
    float wr = winrate(&RV, ri2, wt, EVAL_TRIALS);
    bool revival_ok = rr.evicted == 1 && revived_novel && wr >= REVIVE_R * w0;

    /* ---- check 5: lossy + within-epoch replay (logged pairwise path) ---- */
    const cinm_consolidate_policy pol5 = {
        .evict_floor = 0.01f, .evict_conf_max = 0.50f, .evict_idle_age = 10,
        .promote_conf = 2.0f, .promote_evidence = UINT32_MAX,   /* pairwise conf==0: nothing freezes */
    };
    cinm_log lg;
    cinm_log_init(&lg);
    cinm_map L;
    cinm_init(&L);
    uint32_t seq = 0;

    for (uint32_t key = 5; key <= 6; key++) {             /* dead: one tiny-dphi update, early/stale */
        cinm_event ev = { .seq = seq, .type = 0, .key = key, .reward = 1.0f };
        for (int k = 0; k < NFEAT; k++) ev.dphi[k] = 0.001f;
        size_t i = cinm_address(&L, key, nullptr);
        cinm_update(&L, i, ev.dphi, ev.reward, seq);
        cinm_log_append(&lg, &ev);
        L.t = seq + 1; seq++;
    }
    for (int round = 0; round < 30; round++)              /* active: real updates, recent */
        for (uint32_t key = 0; key <= 2; key++) {
            cinm_event ev = { .seq = seq, .type = 0, .key = key, .reward = 1.0f };
            for (int k = 0; k < NFEAT; k++) ev.dphi[k] = (k % 2 == 0) ? 0.5f : -0.3f;
            size_t i = cinm_address(&L, key, nullptr);
            cinm_update(&L, i, ev.dphi, ev.reward, seq);
            cinm_log_append(&lg, &ev);
            L.t = seq + 1; seq++;
        }

    uint32_t base = seq;
    cinm_consolidate_result r5 = cinm_consolidate(&L, &pol5, L.t, base);

    cinm_map S;
    cinm_snapshot(&L, &S);                                /* post-consolidation checkpoint */

    for (uint32_t key = 0; key <= 2; key++) {             /* epoch-1 tail: kept contexts only */
        cinm_event ev = { .seq = seq, .type = 0, .key = key, .reward = 1.0f };
        for (int k = 0; k < NFEAT; k++) ev.dphi[k] = 0.2f;
        size_t i = cinm_address(&L, key, nullptr);
        cinm_update(&L, i, ev.dphi, ev.reward, seq);
        cinm_log_append(&lg, &ev);
        L.t = seq + 1; seq++;
    }

    cinm_map WE;
    cinm_restore(&WE, &S);
    bool within_epoch = cinm_log_replay(&lg, &WE, base) && cinm_equal(&WE, &L);

    cinm_map FR;
    cinm_init(&FR);
    bool full_diverges = cinm_log_replay(&lg, &FR, 0) && !cinm_equal(&FR, &L);

    bool lossy_ok = r5.evicted == 2 && within_epoch && full_diverges;

    /* ---- report ---- */
    bool c123 = logged && counts && frozen_ok && frozen_stable && active_kept && evicted_gone;
    printf("evict/freeze/count/epoch: %s (evicted=%u promoted=%u count %zu->%zu epoch=%u)\n",
           c123 ? "PASS" : "FAIL", r.evicted, r.promoted, count_before, count_after, M.epoch);
    printf("revival to >= %.0f%% of pre-eviction within K=%d: %s (w0=%.3f wr=%.3f ratio=%.3f)\n",
           (double)(REVIVE_R * 100.0f), REVIVE_K, revival_ok ? "PASS" : "FAIL",
           (double)w0, (double)wr, (double)(wr / w0));
    printf("lossy + within-epoch replay: %s (full replay diverges=%s, within-epoch matches=%s)\n",
           lossy_ok ? "PASS" : "FAIL", full_diverges ? "yes" : "no", within_epoch ? "yes" : "no");

    cinm_ledger_free(&led);
    cinm_log_free(&lg);
    return (c123 && revival_ok && lossy_ok) ? 0 : 1;
}
