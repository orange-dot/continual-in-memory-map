/* core_check.c — core adaptive-cell gates: clamp pressure, margin direction,
 * maturity, conflict, and decay. The log/replay sidecar is checked separately. */
#include "cinm.h"
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

/* A uniform +1 feature direction, so margin == the sum of the cell's weights. */
static void uniform(float v[static NFEAT], float x) {
    for (int k = 0; k < NFEAT; k++) v[k] = x;
}

/* One fresh cell in a fresh map; returns its index. */
static size_t fresh_cell(cinm_map *m) {
    cinm_init(m);
    return cinm_address(m, 1, NULL);
}

/* Adaptive damps the step as a cell commits, so it saturates far less often than
 * the constant pairwise step over the same driven stream. */
static bool adaptive_clamps_less_than_pairwise(void) {
    float dphi[NFEAT]; uniform(dphi, 1.0f);
    cinm_map mp, ma;
    size_t ip = fresh_cell(&mp);
    size_t ia = fresh_cell(&ma);
    uint32_t pair = 0, adap = 0;
    for (uint32_t s = 0; s < 200; s++) {
        pair += cinm_update_pairwise(&mp, ip, dphi, +1.0f, s).clamp_count;
        adap += cinm_update_adaptive(&ma, ia, dphi, +1.0f, s).clamp_count;
    }
    return adap < pair;
}

/* Negative reward against a positive feature delta moves the margin down. */
static bool negative_reward_lowers_margin(void) {
    float dphi[NFEAT]; uniform(dphi, 1.0f);
    cinm_map m;
    size_t i = fresh_cell(&m);
    for (uint32_t s = 0; s < 5; s++)            /* prime a positive preference */
        cinm_update_pairwise(&m, i, dphi, +1.0f, s);
    cinm_update_result r = cinm_update_adaptive(&m, i, dphi, -1.0f, 5);
    return r.margin_after < r.margin_before;
}

/* A matured (high-confidence, low-plasticity) cell moves less under an identical
 * update than a fresh one. */
static bool mature_changes_less_than_fresh(void) {
    float dphi[NFEAT]; uniform(dphi, 1.0f);
    cinm_map mm, mf;
    size_t im = fresh_cell(&mm);
    size_t jf = fresh_cell(&mf);
    for (uint32_t s = 0; s < 30; s++)
        cinm_update_adaptive(&mm, im, dphi, +1.0f, s);
    cinm_update_result rm = cinm_update_adaptive(&mm, im, dphi, +1.0f, 30);
    cinm_update_result rf = cinm_update_adaptive(&mf, jf, dphi, +1.0f, 0);
    float dmature = rm.margin_after - rm.margin_before;
    float dfresh  = rf.margin_after - rf.margin_before;
    return dmature < dfresh;
}

/* Reversed reward flags conflict only once the cell is mature; a fresh cell takes
 * the same reversed update without raising the flag. */
static bool conflict_fires_on_mature_reversal(void) {
    float dphi[NFEAT]; uniform(dphi, 1.0f);
    cinm_map mm, mf;
    size_t im = fresh_cell(&mm);
    size_t jf = fresh_cell(&mf);
    for (uint32_t s = 0; s < 30; s++)
        cinm_update_adaptive(&mm, im, dphi, +1.0f, s);
    bool mature_conflict = cinm_update_adaptive(&mm, im, dphi, -1.0f, 30).conflict;
    bool fresh_conflict  = cinm_update_adaptive(&mf, jf, dphi, -1.0f, 0).conflict;
    return mature_conflict && !fresh_conflict;
}

/* Decay shrinks weights but keeps a strong preference clearly above zero. */
static bool decay_preserves_preference(void) {
    float dphi[NFEAT]; uniform(dphi, 1.0f);
    cinm_map m;
    size_t i = fresh_cell(&m);
    for (uint32_t s = 0; s < 20; s++)
        cinm_update_pairwise(&m, i, dphi, +1.0f, s);
    float before = cinm_score(&m, i, dphi);
    cinm_decay(&m, 0.5f);
    float after = cinm_score(&m, i, dphi);
    return before > 1.0f && after < before && after > 1.0f;
}

int main(void) {
    bool a = adaptive_clamps_less_than_pairwise();
    bool b = negative_reward_lowers_margin();
    bool c = mature_changes_less_than_fresh();
    bool d = conflict_fires_on_mature_reversal();
    bool e = decay_preserves_preference();
    printf("adaptive clamps less than pairwise: %s\n", a ? "PASS" : "FAIL");
    printf("negative reward lowers margin: %s\n", b ? "PASS" : "FAIL");
    printf("mature cell changes less than fresh: %s\n", c ? "PASS" : "FAIL");
    printf("conflict fires on mature reversal: %s\n", d ? "PASS" : "FAIL");
    printf("decay preserves strong preference: %s\n", e ? "PASS" : "FAIL");
    return (a && b && c && d && e) ? 0 : 1;
}
