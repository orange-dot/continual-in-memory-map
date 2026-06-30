/* cinm.c — synaptic-map implementation (SoA). Pure C, no external libraries. */
#include "cinm.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

void cinm_init(cinm_map *m) {
    /* Zero the whole map, not just in_use: unused cells must be deterministic so
     * raw snapshots and content hashes are reproducible. */
    memset(m, 0, sizeof *m);
    m->eta = ETA;          /* runtime learning rate; self-adaptation (P) may tune it */
}

/* Allocate and zero-initialize a fresh cell at the tail; MAX_CELLS if the map is full.
 * Shared by exact (cinm_address) and nearest-neighbour (cinm_address_nn) addressing so
 * both modes birth a cell identically. The caller sets the address field (key or proto). */
static size_t alloc_cell(cinm_map *m) {
    if (m->count >= MAX_CELLS) return MAX_CELLS;       /* bounded growth */
    size_t i = m->count++;
    for (int k = 0; k < NFEAT; k++) { m->w[i][k] = 0.0f; m->proto[i][k] = 0.0f; }
    m->plast[i]        = 1.0f;
    m->conf[i]         = 0.0f;
    m->evidence[i]     = 0;
    m->born[i]         = m->t;
    m->last_touched[i] = m->t;
    m->in_use[i]       = true;
    m->frozen[i]       = false;
#ifdef CINM_ENABLE_SPLIT
    m->conflict[i]     = 0;
    for (int k = 0; k < NFEAT; k++) m->conflict_dir[i][k] = 0.0f;
    m->split_locked[i] = false;
#endif
    return i;
}

size_t cinm_address(cinm_map *m, uint32_t key, bool *was_novel) {
    for (size_t i = 0; i < m->count; i++) {
        if (m->in_use[i] && m->key[i] == key) {
            if (was_novel) *was_novel = false;
            return i;
        }
    }
    size_t i = alloc_cell(m);
    if (i == MAX_CELLS) {
        if (was_novel) *was_novel = false;
        return MAX_CELLS;
    }
    m->key[i] = key;
    if (was_novel) *was_novel = true;
    return i;
}

/* Squared L2 distance between cell i's prototype and ctx (libm-free: no sqrt). */
static float proto_dist2(const cinm_map *m, size_t i, const float ctx[static NFEAT]) {
    float s = 0.0f;
    for (int k = 0; k < NFEAT; k++) {
        float d = m->proto[i][k] - ctx[k];
        s += d * d;
    }
    return s;
}

size_t cinm_address_nn(cinm_map *m, const float ctx[static NFEAT], float radius2, bool *was_novel) {
    size_t best    = MAX_CELLS;
    float  best_d2 = radius2;                       /* a cell qualifies only within the radius */
    for (size_t i = 0; i < m->count; i++) {
        if (!m->in_use[i]) continue;
        float d2 = proto_dist2(m, i, ctx);
        if (d2 <= best_d2) { best_d2 = d2; best = i; }
    }
    if (best != MAX_CELLS) {                         /* hit: birth-fixed proto, no state change */
        if (was_novel) *was_novel = false;
        return best;
    }
    size_t i = alloc_cell(m);                        /* miss: a fresh prototype anchored at ctx */
    if (i == MAX_CELLS) {
        if (was_novel) *was_novel = false;
        return MAX_CELLS;
    }
    for (int k = 0; k < NFEAT; k++) m->proto[i][k] = ctx[k];
    m->key[i] = (uint32_t)i;                         /* provenance id for explain; address is proto */
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
    /* same operand grouping as the legacy update: ((eta*reward)*plast) then *dphi[k] */
    uint32_t clamps = apply_step(m, i, dphi, m->eta * reward * m->plast[i]);
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

    uint32_t clamps = apply_step(m, i, dphi, m->eta * reward * m->plast[i] * scale);
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

#ifdef CINM_ENABLE_SPLIT
cinm_update_result cinm_update_adaptive_split(cinm_map *m, size_t i, const float ctx[static NFEAT],
                                              const float dphi[static NFEAT], float reward, uint32_t t) {
    cinm_update_result r = cinm_update_adaptive(m, i, dphi, reward, t);   /* proven path, zero-diff */
    /* A broad contradiction (the cell scored against the reward) is the split signal — the
     * mature-only `r.conflict` flag misses born-torn cells (see docs/06). Count it, and EWMA
     * the context-space dissent direction so the split pass can place a child by address. */
    if (reward * r.margin_before < 0.0f) {
        m->conflict[i]++;
        constexpr float A = 0.25f;                                       /* EWMA weight on the new sample */
        for (int k = 0; k < NFEAT; k++)
            m->conflict_dir[i][k] = (1.0f - A) * m->conflict_dir[i][k]
                                  + A * (ctx[k] - m->proto[i][k]);
    }
    return r;
}
#endif

void cinm_decay(cinm_map *m, float factor) {
    float f = clampf(factor, 0.0f, 1.0f);
    for (size_t i = 0; i < m->count; i++)
        if (m->in_use[i] && !m->frozen[i])      /* frozen cells are decay-exempt (D018) */
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

void cinm_epoch_advance(cinm_map *m, uint32_t new_base_seq) {
    m->base_seq = new_base_seq;
    m->epoch++;
}

/* Largest absolute weight of a cell. decay drives idle cells' weights toward 0, so a
 * small max|w| marks a cell that decay has emptied. */
static float cell_strength(const cinm_map *m, size_t i) {
    float s = 0.0f;
    for (int k = 0; k < NFEAT; k++) {
        float a = absf(m->w[i][k]);
        if (a > s) s = a;
    }
    return s;
}

cinm_consolidate_result cinm_consolidate(cinm_map *m, const cinm_consolidate_policy *p,
                                         uint32_t now, uint32_t new_base_seq) {
    cinm_consolidate_result res = { .evicted = 0, .promoted = 0, .merged = 0,
                                    .split = 0, .split_suppressed = 0 };

    /* Pass 0: merge (R3.5, D019). Fold each non-frozen cell into an earlier non-frozen cell
     * whose prototype is within merge_radius2 — schema compression that exact keys cannot do.
     * Evidence-weighted blend of address (proto) and content (w); the folded cell is freed and
     * reclaimed by the compaction below. Single pass, O(count^2 * NFEAT): fine at MAX_CELLS and
     * off the hot path. Skipped entirely when merge_radius2 == 0 (exact-key callers). */
    if (p->merge_radius2 > 0.0f) {
        for (size_t j = 0; j < m->count; j++) {
            if (!m->in_use[j] || m->frozen[j]) continue;
#ifdef CINM_ENABLE_SPLIT
            if (m->split_locked[j]) continue;           /* a split child is merge-exempt (hysteresis) */
#endif
            for (size_t i = 0; i < j; i++) {
                if (!m->in_use[i] || m->frozen[i]) continue;
#ifdef CINM_ENABLE_SPLIT
                if (m->split_locked[i]) continue;
#endif
                if (proto_dist2(m, i, m->proto[j]) > p->merge_radius2) continue;

                float ei = (float)m->evidence[i], ej = (float)m->evidence[j];
                float tot = ei + ej;
                float wi = tot > 0.0f ? ei / tot : 0.5f;
                float wj = tot > 0.0f ? ej / tot : 0.5f;
                for (int k = 0; k < NFEAT; k++) {
                    m->proto[i][k] = wi * m->proto[i][k] + wj * m->proto[j][k];
                    m->w[i][k]     = wi * m->w[i][k]     + wj * m->w[j][k];
                }
                m->evidence[i] += m->evidence[j];
                if (m->conf[j] > m->conf[i])                 m->conf[i] = m->conf[j];
                if (m->born[j] < m->born[i])                 m->born[i] = m->born[j];
                if (m->last_touched[j] > m->last_touched[i]) m->last_touched[i] = m->last_touched[j];
#ifdef CINM_ENABLE_SPLIT
                m->conflict[i] = 0;                          /* a consolidated cell starts tension-free */
                for (int k = 0; k < NFEAT; k++) m->conflict_dir[i][k] = 0.0f;
#endif
                m->in_use[j] = false;
                res.merged++;
                break;                          /* j is folded away; on to the next cell */
            }
        }
    }

#ifdef CINM_ENABLE_SPLIT
    /* Pass 0.5: split (D019, the inverse of merge). A non-frozen cell whose broad-
     * contradiction rate crossed one third with a consistent context direction is carrying two
     * address-separable schemas; fork the dissenting sub-population into a fresh child placed
     * along the dissent direction (data-driven, libm-free — no sqrt). Lossy: child content is
     * re-learned, not recovered; evidence is apportioned by the dissent count alone. Iterate
     * only the pre-split range n0 so children are never re-split. Off when split_min_conflict==0. */
    if (p->split_min_conflict > 0) {
        size_t n0 = m->count;
        for (size_t i = 0; i < n0; i++) {
            if (!m->in_use[i] || m->frozen[i])           continue;
            if (m->evidence[i] < p->split_min_evidence)  continue;   /* rate not yet trustworthy */
            if (m->conflict[i] < p->split_min_conflict)  continue;   /* below the absolute floor  */
            if (3u * m->conflict[i] < m->evidence[i])    continue;   /* contradiction rate < 1/3  */
            if (m->conf[i] >= p->promote_conf)           continue;   /* never split a freeze-candidate */

            float dir2 = 0.0f;
            for (int k = 0; k < NFEAT; k++) dir2 += m->conflict_dir[i][k] * m->conflict_dir[i][k];
            if (dir2 < p->split_dir_floor2) { res.split_suppressed++; continue; }  /* aliased: no split helps */

            size_t j = alloc_cell(m);
            if (j == MAX_CELLS) { res.split_suppressed++; continue; }              /* map full: honest skip */

            for (int k = 0; k < NFEAT; k++) {
                m->proto[j][k] = m->proto[i][k] + m->conflict_dir[i][k];  /* child address along the dissent */
                m->w[j][k]     = 0.0f;                                    /* fresh slate; parent w mispredicts */
            }
            m->key[j]   = (uint32_t)j;
            m->plast[j] = 1.0f;
            m->conf[j]  = 0.0f;
            m->evidence[j] = m->conflict[i];                             /* corroboration apportioned (lossy) */
            m->evidence[i] = m->evidence[i] > m->conflict[i] ? m->evidence[i] - m->conflict[i] : 1u;
            m->born[j]         = now;
            m->last_touched[j] = now;
            m->in_use[j]       = true;
            m->frozen[j]       = false;
            m->split_locked[j] = true;                                   /* child is merge-exempt (hysteresis) */

            m->conflict[i] = 0;                                          /* refractory reset on the parent */
            for (int k = 0; k < NFEAT; k++) m->conflict_dir[i][k] = 0.0f;
            res.split++;
        }
    }
#endif

    /* Pass 1: mark. Freeze the strong (frozen, plasticity floored); evict the
     * weak-and-stale that decay has already emptied. A frozen cell is never evicted. */
    for (size_t i = 0; i < m->count; i++) {
        if (!m->in_use[i] || m->frozen[i]) continue;

        if (m->conf[i] >= p->promote_conf && m->evidence[i] >= p->promote_evidence) {
            m->frozen[i] = true;
            m->plast[i]  = PLAST_FLOOR;
            res.promoted++;
            continue;
        }

        bool weak  = cell_strength(m, i) <= p->evict_floor && m->conf[i] <= p->evict_conf_max;
        bool stale = (now - m->last_touched[i]) >= p->evict_idle_age;
        if (weak && stale) {
            m->in_use[i] = false;
            res.evicted++;
        }
    }

    /* Pass 2: compact live cells to the front so eviction reclaims capacity, then zero
     * the freed tail so snapshots stay reproducible (matches cinm_init). */
    size_t d = 0;
    for (size_t s = 0; s < m->count; s++) {
        if (!m->in_use[s]) continue;
        if (d != s) {
            m->key[d] = m->key[s];
            for (int k = 0; k < NFEAT; k++) { m->w[d][k] = m->w[s][k]; m->proto[d][k] = m->proto[s][k]; }
            m->plast[d]        = m->plast[s];
            m->conf[d]         = m->conf[s];
            m->evidence[d]     = m->evidence[s];
            m->born[d]         = m->born[s];
            m->last_touched[d] = m->last_touched[s];
            m->in_use[d]       = true;
            m->frozen[d]       = m->frozen[s];
#ifdef CINM_ENABLE_SPLIT
            m->conflict[d]     = m->conflict[s];
            for (int k = 0; k < NFEAT; k++) m->conflict_dir[d][k] = m->conflict_dir[s][k];
            m->split_locked[d] = m->split_locked[s];
#endif
        }
        d++;
    }
    for (size_t i = d; i < m->count; i++) {
        m->key[i] = 0;
        for (int k = 0; k < NFEAT; k++) { m->w[i][k] = 0.0f; m->proto[i][k] = 0.0f; }
        m->plast[i]        = 0.0f;
        m->conf[i]         = 0.0f;
        m->evidence[i]     = 0;
        m->born[i]         = 0;
        m->last_touched[i] = 0;
        m->in_use[i]       = false;
        m->frozen[i]       = false;
#ifdef CINM_ENABLE_SPLIT
        m->conflict[i]     = 0;
        for (int k = 0; k < NFEAT; k++) m->conflict_dir[i][k] = 0.0f;
        m->split_locked[i] = false;
#endif
    }
    m->count = d;

    cinm_epoch_advance(m, new_base_seq);
    return res;
}

bool cinm_equal(const cinm_map *a, const cinm_map *b) {
    return a->count == b->count
        && a->t == b->t
        && a->epoch == b->epoch
        && a->base_seq == b->base_seq
        && a->eta == b->eta
        && memcmp(a->key, b->key, sizeof a->key) == 0
        && memcmp(a->proto, b->proto, sizeof a->proto) == 0
        && memcmp(a->w, b->w, sizeof a->w) == 0
        && memcmp(a->plast, b->plast, sizeof a->plast) == 0
        && memcmp(a->conf, b->conf, sizeof a->conf) == 0
        && memcmp(a->evidence, b->evidence, sizeof a->evidence) == 0
        && memcmp(a->born, b->born, sizeof a->born) == 0
        && memcmp(a->last_touched, b->last_touched, sizeof a->last_touched) == 0
        && memcmp(a->in_use, b->in_use, sizeof a->in_use) == 0
#ifdef CINM_ENABLE_SPLIT
        && memcmp(a->conflict, b->conflict, sizeof a->conflict) == 0
        && memcmp(a->conflict_dir, b->conflict_dir, sizeof a->conflict_dir) == 0
        && memcmp(a->split_locked, b->split_locked, sizeof a->split_locked) == 0
#endif
        && memcmp(a->frozen, b->frozen, sizeof a->frozen) == 0;
}

bool cinm_transaction(cinm_map *m, bool (*candidate)(cinm_map *, void *), void *ctx) {
    cinm_map snap;
    cinm_snapshot(m, &snap);
    if (candidate(m, ctx)) return true;     /* committed */
    cinm_restore(m, &snap);                  /* rolled back */
    return false;
}
