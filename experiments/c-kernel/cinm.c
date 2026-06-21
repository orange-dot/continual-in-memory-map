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

void cinm_update(cinm_map *m, size_t i, const float dphi[static NFEAT], float reward, uint32_t t) {
    assert(i < m->count);
    /* Bounded local pairwise step: nudge w toward the winner, clamp each weight. */
    for (int k = 0; k < NFEAT; k++)
        m->w[i][k] = clampf(m->w[i][k] + ETA * reward * m->plast[i] * dphi[k], -W_MAX, W_MAX);
    m->evidence[i]++;
    m->last_touched[i] = t;
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
