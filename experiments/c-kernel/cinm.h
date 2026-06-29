/* cinm.h — CINM synaptic-map core (C line, decision D011). SoA hot/cold layout. */
#ifndef CINM_H
#define CINM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

enum { NFEAT = 8, MAX_CELLS = 256 };
#define W_MAX 4.0f   /* weight clamp bound */
#define ETA   0.1f   /* learning rate */

/* adaptive margin-gate tuning (see cinm_update_adaptive) */
#define MARGIN_NEAR_ZERO 0.25f  /* |margin| below this -> undecided band       */
#define MARGIN_STRONG    1.00f  /* |margin| at/above this -> confident region  */
#define CONF_MATURE      0.60f  /* conf at/above this -> cell is "mature"      */
#define STEP_SMALL       0.5f   /* agree strongly -> damp the step             */
#define STEP_MEDIUM      1.0f   /* undecided -> nominal step (== constant step) */
#define STEP_CORRECTIVE  1.5f   /* conflict -> larger corrective step          */
#define CONF_UP          0.05f  /* confidence gain on agreement                */
#define CONF_DOWN        0.10f  /* confidence loss on conflict (> CONF_UP)     */
#define PLAST_FLOOR      0.20f  /* plasticity never falls below this           */

/* Structure-of-arrays: hot fields (scanned/scored) are kept apart from cold
 * provenance so a scan touches only the arrays it needs. A "cell" is an index. */
typedef struct {
    /* hot — address / score / update path */
    uint32_t key[MAX_CELLS];
    float    proto[MAX_CELLS][NFEAT];  /* continuous context key for NN addressing (D019):
                                          the cell's address; birth-fixed, only merge moves it.
                                          distinct from w (what the cell predicts). zero for
                                          exact-key cells. */
    float    w[MAX_CELLS][NFEAT];
    float    plast[MAX_CELLS];
    float    conf[MAX_CELLS];
    /* cold — provenance / bookkeeping */
    uint32_t evidence[MAX_CELLS];
    uint32_t born[MAX_CELLS];
    uint32_t last_touched[MAX_CELLS];
    bool     in_use[MAX_CELLS];
    bool     frozen[MAX_CELLS];   /* frozen by consolidation: decay/eviction exempt (D018) */
    size_t   count;
    uint32_t t;            /* logical event clock */
    uint32_t epoch;        /* consolidation generation; 0 until first consolidate (D018) */
    uint32_t base_seq;     /* first seq of the current epoch; within-epoch replay anchor */
    float    eta;          /* learning rate (runtime; self-adaptation operator target, P) */
} cinm_map;

static inline float clampf(float x, float lo, float hi) {
    return x < lo ? lo : (x > hi ? hi : x);
}

static inline float absf(float x) { return x < 0.0f ? -x : x; }

void   cinm_init(cinm_map *m);
size_t cinm_address(cinm_map *m, uint32_t key, bool *was_novel); /* index; MAX_CELLS if full */
/* Continuous nearest-neighbour addressing (D019): the in-use cell whose prototype is
 * within radius2 (squared L2) of ctx, else a fresh cell whose prototype is ctx itself.
 * Prototype is birth-fixed (only merge consolidation moves it) and the distance is
 * libm-free. radius2 == 0 with one-hot ctx degenerates to exact-key addressing, so NN
 * strictly generalizes cinm_address. Returns MAX_CELLS if the map is full. */
[[nodiscard]] size_t cinm_address_nn(cinm_map *m, const float ctx[static NFEAT],
                                     float radius2, bool *was_novel);
float  cinm_score(const cinm_map *m, size_t i, const float phi[static NFEAT]);

/* Margin of the update direction dphi against cell i: the same dot product as
 * cinm_score, named for the adaptive-update vocabulary (cinm_update_result). */
static inline float cinm_margin(const cinm_map *m, size_t i, const float dphi[static NFEAT]) {
    return cinm_score(m, i, dphi);
}

/* Result of a core update: margins before/after, how many of the NFEAT weights
 * saturated at +/-W_MAX, and whether the update fought a mature cell (adaptive). */
typedef struct {
    float    margin_before;
    float    margin_after;
    uint32_t clamp_count;
    bool     conflict;
} cinm_update_result;

/* Constant-step pairwise update (legacy behavior) now reporting metadata; leaves
 * conf/plast untouched. cinm_update is the void wrapper over this. */
cinm_update_result cinm_update_pairwise(cinm_map *m, size_t i, const float dphi[static NFEAT], float reward, uint32_t t);
/* Margin/confidence-gated bounded update: scales the step by alignment and
 * maturity, evolves conf and (from conf) plast, flags conflict. */
cinm_update_result cinm_update_adaptive(cinm_map *m, size_t i, const float dphi[static NFEAT], float reward, uint32_t t);
/* Multiply every in-use cell's weights by clampf(factor,0,1); provenance intact. */
void   cinm_decay(cinm_map *m, float factor);
void   cinm_update(cinm_map *m, size_t i, const float dphi[static NFEAT], float reward, uint32_t t);
void   cinm_explain(const cinm_map *m, size_t i, uint32_t now);

/* Reversible transaction primitives — whole-map copy (the map is a small POD). */
void   cinm_snapshot(const cinm_map *m, cinm_map *buf);
void   cinm_restore(cinm_map *m, const cinm_map *buf);
/* Snapshot, run candidate (applies + evaluates, returns keep), commit or restore. */
bool   cinm_transaction(cinm_map *m, bool (*candidate)(cinm_map *, void *), void *ctx);

/* Advance the epoch: set the within-epoch replay anchor to new_base_seq and bump the
 * generation. Consolidation (D018) calls this after discarding pre-boundary state;
 * within-epoch replay/undo then runs from base_seq, not from seq 0. */
void   cinm_epoch_advance(cinm_map *m, uint32_t new_base_seq);

/* Consolidation policy (D018, R3 + R3.5). Evict-dead + freeze-strong applies to any map;
 * with nearest-neighbour addressing (D019) consolidation also merges near-duplicate
 * prototypes into one (schema compression, R3.5) when merge_radius2 > 0. Exact-key callers
 * leave merge_radius2 at 0 and get the original evict+freeze behaviour. */
typedef struct {
    float    evict_floor;      /* evict candidates have max|w| <= this (decay-emptied) ... */
    float    evict_conf_max;   /*   ... and conf <= this (weak) ...                        */
    uint32_t evict_idle_age;   /*   ... and (now - last_touched) >= this (stale).          */
    float    promote_conf;     /* freeze candidates have conf >= this ...                  */
    uint32_t promote_evidence; /*   ... and evidence >= this (well-corroborated).          */
    float    merge_radius2;    /* fold non-frozen prototype pairs within this sq-dist; 0 = off */
} cinm_consolidate_policy;

typedef struct {
    uint32_t evicted;          /* dead cells dropped (lossy)                       */
    uint32_t promoted;         /* strong cells frozen (protected; not lossy)       */
    uint32_t merged;           /* near-duplicate prototypes folded away (lossy, R3.5) */
} cinm_consolidate_result;

/* Lossy consolidation: merge near-duplicate prototypes (R3.5, if merge_radius2 > 0), freeze
 * the strong, evict the dead-and-stale, compact the map, and advance the epoch to
 * new_base_seq. Frozen cells never merge, are never evicted, and are exempt from decay. The
 * caller records a receipt in the decision ledger (R2). After this, full-log replay no longer
 * reconstructs the map; within-epoch replay from a post-consolidation snapshot does. */
cinm_consolidate_result cinm_consolidate(cinm_map *m, const cinm_consolidate_policy *p,
                                         uint32_t now, uint32_t new_base_seq);

/* Member-wise semantic equality: count, clock, and the full arrays (cinm_init
 * zeroes unused cells), ignoring struct padding. Verifies that rollback and
 * within-epoch replay reconstruct the same map. */
bool   cinm_equal(const cinm_map *a, const cinm_map *b);

#endif /* CINM_H */
