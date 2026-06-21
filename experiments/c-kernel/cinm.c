/* cinm.c — synaptic-map implementation (SoA). Pure C, no external libraries. */
#include "cinm.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

void cinm_init(cinm_map *m) {
    /* Zero the whole map, not just in_use: unused cells must be deterministic so
     * raw snapshots and content hashes are reproducible. */
    memset(m, 0, sizeof *m);
}

size_t cinm_address(cinm_map *m, uint32_t key, bool *was_novel) {
    for (size_t i = 0; i < m->count; i++) {
        if (m->in_use[i] && m->key[i] == key) {
            if (was_novel) *was_novel = false;
            return i;
        }
    }
    if (m->count >= MAX_CELLS) {            /* bounded growth */
        if (was_novel) *was_novel = false;
        return MAX_CELLS;
    }
    size_t i = m->count++;
    m->key[i] = key;
    for (int k = 0; k < NFEAT; k++) m->w[i][k] = 0.0f;
    m->plast[i] = 1.0f;
    m->conf[i] = 0.0f;
    m->evidence[i] = 0;
    m->born[i] = m->t;
    m->last_touched[i] = m->t;
    m->in_use[i] = true;
    if (was_novel) *was_novel = true;
    return i;
}

float cinm_score(const cinm_map *m, size_t i, const float phi[static NFEAT]) {
    assert(i < m->count);
    float s = 0.0f;
    for (int k = 0; k < NFEAT; k++)
        s += m->w[i][k] * phi[k];
    return s;
}

/* Apply a bounded additive step to every weight of cell i; return how many of the
 * NFEAT weights saturated at +/-W_MAX. `step` already folds ETA*reward*plast, so the
 * raw sum stays bit-identical to the legacy constant-step update. */
static uint32_t apply_step(cinm_map *m, size_t i, const float dphi[static NFEAT], float step) {
    uint32_t clamps = 0;
    for (int k = 0; k < NFEAT; k++) {
        float raw     = m->w[i][k] + step * dphi[k];
        float clamped = clampf(raw, -W_MAX, W_MAX);
        if (clamped != raw) clamps++;
        m->w[i][k] = clamped;
    }
    return clamps;
}

cinm_update_result cinm_update_pairwise(cinm_map *m, size_t i, const float dphi[static NFEAT], float reward, uint32_t t) {
    assert(i < m->count);
    float margin_before = cinm_score(m, i, dphi);
    /* same operand grouping as the legacy update: ((ETA*reward)*plast) then *dphi[k] */
    uint32_t clamps = apply_step(m, i, dphi, ETA * reward * m->plast[i]);
    float margin_after = cinm_score(m, i, dphi);
    m->evidence[i]++;
    m->last_touched[i] = t;
    return (cinm_update_result){
        .margin_before = margin_before,
        .margin_after  = margin_after,
        .clamp_count   = clamps,
        .conflict      = false,
    };
}

cinm_update_result cinm_update_adaptive(cinm_map *m, size_t i, const float dphi[static NFEAT], float reward, uint32_t t) {
    assert(i < m->count);
    float margin_before = cinm_score(m, i, dphi);
    float agree = reward * margin_before;          /* >0 agree, <0 contradict */
    float mag   = absf(margin_before);

    float scale;
    bool  conflict = false;
    if (agree < 0.0f && m->conf[i] >= CONF_MATURE) {
        scale = STEP_CORRECTIVE;                    /* contradicts a mature cell */
        conflict = true;
    } else if (mag < MARGIN_NEAR_ZERO) {
        scale = STEP_MEDIUM;                        /* undecided */
    } else if (agree > 0.0f && mag >= MARGIN_STRONG) {
        scale = STEP_SMALL;                         /* already strongly correct */
    } else {
        scale = STEP_MEDIUM;                        /* weak signal / default */
    }

    uint32_t clamps = apply_step(m, i, dphi, ETA * reward * m->plast[i] * scale);
    float margin_after = cinm_score(m, i, dphi);

    /* Confidence rises on agreement, falls mildly on conflict; plasticity follows
     * confidence down but never past the floor. */
    float dconf = conflict ? -CONF_DOWN : (agree > 0.0f ? CONF_UP : 0.0f);
    m->conf[i]  = clampf(m->conf[i] + dconf, 0.0f, 1.0f);
    m->plast[i] = clampf(1.0f - m->conf[i], PLAST_FLOOR, 1.0f);

    m->evidence[i]++;
    m->last_touched[i] = t;
    return (cinm_update_result){
        .margin_before = margin_before,
        .margin_after  = margin_after,
        .clamp_count   = clamps,
        .conflict      = conflict,
    };
}

void cinm_decay(cinm_map *m, float factor) {
    float f = clampf(factor, 0.0f, 1.0f);
    for (size_t i = 0; i < m->count; i++)
        if (m->in_use[i])
            for (int k = 0; k < NFEAT; k++)
                m->w[i][k] *= f;
}

/* Compatibility wrapper: existing callers and the replay log stay on the pairwise
 * constant step. Adaptive update is deliberately never logged/replayed. */
void cinm_update(cinm_map *m, size_t i, const float dphi[static NFEAT], float reward, uint32_t t) {
    (void)cinm_update_pairwise(m, i, dphi, reward, t);
}

void cinm_explain(const cinm_map *m, size_t i, uint32_t now) {
    if (i >= m->count) { printf("  (no cell)\n"); return; }
    printf("  cell key=%u  age=%u  last_touched=%u  evidence=%u  plast=%.2f\n",
           m->key[i], now - m->born[i], m->last_touched[i], m->evidence[i], m->plast[i]);
    printf("  w = [");
    for (int k = 0; k < NFEAT; k++)
        printf("%s%+.2f", k ? " " : "", m->w[i][k]);
    printf("]\n");
}

void cinm_snapshot(const cinm_map *m, cinm_map *buf) { *buf = *m; }

void cinm_restore(cinm_map *m, const cinm_map *buf) { *m = *buf; }

bool cinm_equal(const cinm_map *a, const cinm_map *b) {
    return a->count == b->count
        && a->t == b->t
        && memcmp(a->key, b->key, sizeof a->key) == 0
        && memcmp(a->w, b->w, sizeof a->w) == 0
        && memcmp(a->plast, b->plast, sizeof a->plast) == 0
        && memcmp(a->conf, b->conf, sizeof a->conf) == 0
        && memcmp(a->evidence, b->evidence, sizeof a->evidence) == 0
        && memcmp(a->born, b->born, sizeof a->born) == 0
        && memcmp(a->last_touched, b->last_touched, sizeof a->last_touched) == 0
        && memcmp(a->in_use, b->in_use, sizeof a->in_use) == 0;
}

bool cinm_transaction(cinm_map *m, bool (*candidate)(cinm_map *, void *), void *ctx) {
    cinm_map snap;
    cinm_snapshot(m, &snap);
    if (candidate(m, ctx)) return true;     /* committed */
    cinm_restore(m, &snap);                  /* rolled back */
    return false;
}
