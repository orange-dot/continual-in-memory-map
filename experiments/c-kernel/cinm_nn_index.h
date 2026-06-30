/* cinm_nn_index.h — NN (prototype) addressing index for cinm_address_nn (Faza 3b, D019).
 *
 * Phase 3a indexed the exact-key scan (cinm_address -> cinm_index). The similarity path
 * cinm_address_nn (cinm.c:58) is still a full O(count*NFEAT) scan over every cell's
 * proto[NFEAT] and is the remaining scaling wall (doc 12/14: NN p99 ~13.4 ms at 1M).
 * This is a static KD-tree over the live prototypes giving O(log count) average lookup.
 * Like cinm_index it is an AUXILIARY structure: the kernel (cinm.c/.h) is untouched and the
 * linear scan stays the byte-exact reference (doc 11 rule #1). The default kernel build
 * never links this file. Heap-backed, like cinm_index / cinm_log.
 *
 * Equivalence contract (proved by scale_nn_index_check.c): for any ctx and radius2,
 * cinm_nn_index_find returns the SAME cell cinm_address_nn would select — the in-use cell
 * of minimum squared-L2 distance to ctx within radius2, and on a distance tie the HIGHEST
 * cell index (cinm.c:64 updates best on `d2 <= best_d2` while scanning ascending; this is
 * the OPPOSITE tie-break to exact-key addressing's first-match, see scale_index_check.c:8).
 * A miss (nothing within radius2) returns MAX_CELLS, exactly where cinm_address_nn would
 * birth a fresh cell. The find is READ-ONLY: the allocate-on-miss tail stays in the kernel.
 *
 * Exactness under float: the search prunes the far side of a split only when the squared
 * distance from ctx to the splitting plane (diff*diff, a single per-axis term) exceeds the
 * running best. That bound is a provable lower bound on every far cell's distance even in
 * IEEE float (subtraction is monotone, squaring is monotone on non-negatives, and a sum of
 * non-negatives never decreases), so no minimizer is ever pruned away — the index stays
 * byte-exact with the scan without any epsilon slack. */
#ifndef CINM_NN_INDEX_H
#define CINM_NN_INDEX_H

#include "cinm.h"
#include <stdint.h>
#include <stddef.h>

/* Static KD-tree over the map's live cells, built once. Node arrays are sized to the live
 * count; each node owns one cell (its prototype is the splitting point). proto holds an
 * exact NFEAT-float copy of that cell's prototype so a query is self-contained (the find
 * never re-reads the map) and the copy keeps the distance bit-identical to the kernel's.
 * Structure-of-arrays so a tree walk touches only the arrays it needs. A child index of
 * (size_t)-1 is "no child"; root is (size_t)-1 when the map has no live cells. */
typedef struct {
    float  *proto;   /* NFEAT contiguous floats per node: the cell's prototype copy */
    size_t *cell;    /* map cell index per node (the value find returns)            */
    int    *axis;    /* split axis per node (depth % NFEAT)                         */
    size_t *left;    /* left child node index, or (size_t)-1                        */
    size_t *right;   /* right child node index, or (size_t)-1                       */
    size_t  n;       /* node count == indexed live cells                            */
    size_t  root;    /* root node index, or (size_t)-1 when empty                   */
} cinm_nn_index;

/* Build the KD-tree over m's in-use cells. Position-balanced (always splits at the middle
 * index, so tree height is log2(count) even with duplicate prototypes). O(count log count);
 * the cost is a benchmark concern reported separately, not part of a query. Returns false on
 * allocation failure (idx is left safe to pass to cinm_nn_index_free). */
[[nodiscard]] bool cinm_nn_index_build(cinm_nn_index *idx, const cinm_map *m);

/* The cell cinm_address_nn(m, ctx, radius2, ...) would select, or MAX_CELLS if none is within
 * radius2. radius2 == FLT_MAX is an unbounded global 1-NN. Read-only; O(log count) average. */
[[nodiscard]] size_t cinm_nn_index_find(const cinm_nn_index *idx,
                                        const float ctx[static NFEAT], float radius2);

/* Release the node arrays and zero the index. Safe on a zeroed or failed-build index. */
void cinm_nn_index_free(cinm_nn_index *idx);

#endif /* CINM_NN_INDEX_H */
