/* cinm.h — CINM synaptic-map core (C line, decision D011). SoA hot/cold layout. */
#ifndef CINM_H
#define CINM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

enum { NFEAT = 8, MAX_CELLS = 256 };
#define W_MAX 4.0f   /* weight clamp bound */
#define ETA   0.1f   /* learning rate */

/* Structure-of-arrays: hot fields (scanned/scored) are kept apart from cold
 * provenance so a scan touches only the arrays it needs. A "cell" is an index. */
typedef struct {
    /* hot — address / score / update path */
    uint32_t key[MAX_CELLS];
    float    w[MAX_CELLS][NFEAT];
    float    plast[MAX_CELLS];
    float    conf[MAX_CELLS];
    /* cold — provenance / bookkeeping */
    uint32_t evidence[MAX_CELLS];
    uint32_t born[MAX_CELLS];
    uint32_t last_touched[MAX_CELLS];
    bool     in_use[MAX_CELLS];
    size_t   count;
    uint32_t t;            /* logical event clock */
} cinm_map;

static inline float clampf(float x, float lo, float hi) {
    return x < lo ? lo : (x > hi ? hi : x);
}

void   cinm_init(cinm_map *m);
size_t cinm_address(cinm_map *m, uint32_t key, bool *was_novel); /* index; MAX_CELLS if full */
float  cinm_score(const cinm_map *m, size_t i, const float phi[static NFEAT]);
void   cinm_update(cinm_map *m, size_t i, const float dphi[static NFEAT], float reward, uint32_t t);
void   cinm_explain(const cinm_map *m, size_t i, uint32_t now);

/* Reversible transaction primitives — whole-map copy (the map is a small POD). */
void   cinm_snapshot(const cinm_map *m, cinm_map *buf);
void   cinm_restore(cinm_map *m, const cinm_map *buf);
/* Snapshot, run candidate (applies + evaluates, returns keep), commit or restore. */
bool   cinm_transaction(cinm_map *m, bool (*candidate)(cinm_map *, void *), void *ctx);

/* Member-wise semantic equality: count, clock, and the full arrays (cinm_init
 * zeroes unused cells), ignoring struct padding. Verifies that rollback and
 * store replay reconstruct the same map. */
bool   cinm_equal(const cinm_map *a, const cinm_map *b);

#endif /* CINM_H */
