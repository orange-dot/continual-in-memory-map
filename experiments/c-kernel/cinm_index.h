/* cinm_index.h — exact-key acceleration index for cinm_address (Faza 3a, decision A).
 *
 * The linear scan in cinm_address (cinm.c:31) is O(count) and is the measured scaling
 * bottleneck (cim-sys-scale-v1: crossover_n=256 — addressing dominates from the smallest
 * swept map). This is an open-addressing hash mapping key -> cell index in O(1) average,
 * built over a cinm_map's live cells. It is an AUXILIARY structure: the kernel (cinm.c/.h)
 * is untouched and the linear scan stays the byte-exact reference (doc 11 rule #1). The
 * default kernel build never links this file.
 *
 * Equivalence contract (proved by scale_index_check.c): for every in-use key,
 * cinm_key_index_find returns the SAME cell cinm_address would — the LOWEST index on a
 * duplicate key, because cinm_address returns on its first match (cinm.c:33). A miss
 * returns MAX_CELLS, exactly the slot where cinm_address would birth a fresh cell.
 *
 * Like cinm_log / cinm_ledger, this helper uses the heap; the kernel does not. */
#ifndef CINM_INDEX_H
#define CINM_INDEX_H

#include "cinm.h"
#include <stdint.h>
#include <stddef.h>

/* Open-addressing (linear-probing) hash over live keys. cap is a power of two kept at
 * >= 2*count, so the table is at most half full and a probe always terminates at an empty
 * slot. Structure-of-arrays so a probe touches only the arrays it needs. key[slot]/cell[slot]
 * are meaningful only where used[slot] is true. */
typedef struct {
    bool     *used;   /* slot occupied? (calloc-zeroed == empty) */
    uint32_t *key;    /* the key stored at this slot             */
    size_t   *cell;   /* -> cinm_map cell index for that key     */
    size_t    cap;    /* power of two; mask == cap - 1           */
    size_t    count;  /* live entries inserted                   */
} cinm_key_index;

/* Build the index over m's in-use cells. Inserts in ascending cell order and keeps the
 * first (lowest) index per key, mirroring cinm_address's first-match. O(count). Returns
 * false on allocation failure (idx is left safe to pass to cinm_key_index_free). */
[[nodiscard]] bool cinm_key_index_build(cinm_key_index *idx, const cinm_map *m);

/* Cell index for key, or MAX_CELLS if absent (where cinm_address would allocate). O(1) avg. */
[[nodiscard]] size_t cinm_key_index_find(const cinm_key_index *idx, uint32_t key);

/* Release the index's arrays and zero it. Safe on a zeroed or failed-build index. */
void cinm_key_index_free(cinm_key_index *idx);

#endif /* CINM_INDEX_H */
