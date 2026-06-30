/* cinm_nn_index.c — static KD-tree NN index for cinm_address_nn (Faza 3b). See
 * cinm_nn_index.h. The kernel (cinm.c/.h) is untouched: the linear prototype scan stays
 * the byte-exact reference and scale_nn_index_check.c proves this index selects the
 * identical cell, ties included. */
#include "cinm_nn_index.h"
#include <stdlib.h>

static const size_t NPOS = (size_t)-1;

/* Squared L2 distance, byte-identical to cinm.c:49-56 (same operand order and summation):
 * the index stores an exact prototype copy, so a find compares distances bit-for-bit with
 * the scan. Kept local because cinm.c's proto_dist2 is static (cf. scale_index_check.c's
 * local scan_find), leaving the kernel untouched. */
static float nn_dist2(const float a[static NFEAT], const float b[static NFEAT]) {
    float s = 0.0f;
    for (int k = 0; k < NFEAT; k++) {
        float d = a[k] - b[k];
        s += d * d;
    }
    return s;
}

static void swap_sz(size_t *a, size_t *b) { size_t t = *a; *a = *b; *b = t; }

/* Partition order[lo,hi) by cell prototype value on `axis` so that order[k] holds the
 * value-median and every earlier entry is <= it, every later entry >= it. Three-way
 * (Dutch-flag) quickselect with a middle pivot: linear-ish even on the duplicate-heavy
 * prototypes the bench fixtures produce. */
static void select_kth(const cinm_map *m, size_t *order, size_t lo, size_t hi, size_t k, int axis) {
    while (hi - lo > 1) {
        float pivot = m->proto[order[lo + (hi - lo) / 2]][axis];
        size_t lt = lo, i = lo, gt = hi;        /* [lo,lt) < pivot, [lt,i) == pivot, [gt,hi) > pivot */
        while (i < gt) {
            float v = m->proto[order[i]][axis];
            if (v < pivot)      swap_sz(&order[lt++], &order[i++]);
            else if (v > pivot) swap_sz(&order[i], &order[--gt]);
            else                i++;
        }
        if (k < lt)       hi = lt;
        else if (k >= gt) lo = gt;
        else              return;               /* k in the == band: invariant already holds */
    }
}

/* Recursively build the subtree over order[lo,hi); returns its node slot (NPOS if empty).
 * The split is at the middle POSITION (mid), so height stays log2(n) regardless of values;
 * select_kth only arranges which cell lands at mid and that the sides straddle its value. */
static size_t build_node(cinm_nn_index *idx, const cinm_map *m, size_t *order,
                         size_t lo, size_t hi, int depth, size_t *next_node) {
    if (lo >= hi) return NPOS;
    int axis = depth % NFEAT;
    size_t mid = lo + (hi - lo) / 2;
    select_kth(m, order, lo, hi, mid, axis);

    size_t slot = (*next_node)++;
    size_t cell = order[mid];
    idx->cell[slot] = cell;
    idx->axis[slot] = axis;
    for (int k = 0; k < NFEAT; k++) idx->proto[slot * (size_t)NFEAT + k] = m->proto[cell][k];

    idx->left[slot]  = build_node(idx, m, order, lo, mid, depth + 1, next_node);
    idx->right[slot] = build_node(idx, m, order, mid + 1, hi, depth + 1, next_node);
    return slot;
}

bool cinm_nn_index_build(cinm_nn_index *idx, const cinm_map *m) {
    *idx = (cinm_nn_index){ .root = NPOS };

    size_t n = 0;
    for (size_t i = 0; i < m->count; i++) if (m->in_use[i]) n++;
    idx->n = n;
    if (n == 0) return true;                    /* empty index: every find misses (MAX_CELLS) */

    idx->proto = malloc(n * (size_t)NFEAT * sizeof *idx->proto);
    idx->cell  = malloc(n * sizeof *idx->cell);
    idx->axis  = malloc(n * sizeof *idx->axis);
    idx->left  = malloc(n * sizeof *idx->left);
    idx->right = malloc(n * sizeof *idx->right);
    size_t *order = malloc(n * sizeof *order);
    if (!idx->proto || !idx->cell || !idx->axis || !idx->left || !idx->right || !order) {
        free(order);
        cinm_nn_index_free(idx);
        return false;
    }

    size_t w = 0;
    for (size_t i = 0; i < m->count; i++) if (m->in_use[i]) order[w++] = i;

    size_t next_node = 0;
    idx->root = build_node(idx, m, order, 0, n, 0, &next_node);
    free(order);
    return true;
}

/* Branch-and-bound 1-NN bounded by radius2. best_d2 starts at radius2 and shrinks; the
 * comparator reproduces cinm.c:64 (min distance, ties -> highest cell index) independent of
 * visit order, and the far side is taken whenever it could still hold a cell within best_d2
 * (inclusive of equals, so an equal-distance higher index is never missed). */
static void nn_search(const cinm_nn_index *idx, size_t node, const float ctx[static NFEAT],
                      size_t *best, float *best_d2) {
    if (node == NPOS) return;

    const float *p = &idx->proto[node * (size_t)NFEAT];
    float  d2   = nn_dist2(p, ctx);
    size_t cell = idx->cell[node];
    bool better = (*best == MAX_CELLS)
                ? (d2 <= *best_d2)                                   /* first hit within radius */
                : (d2 < *best_d2 || (d2 == *best_d2 && cell > *best));
    if (better) { *best = cell; *best_d2 = d2; }

    int   axis = idx->axis[node];
    float diff = ctx[axis] - p[axis];
    size_t near = diff <= 0.0f ? idx->left[node]  : idx->right[node];
    size_t far  = diff <= 0.0f ? idx->right[node] : idx->left[node];

    nn_search(idx, near, ctx, best, best_d2);
    if (diff * diff <= *best_d2)
        nn_search(idx, far, ctx, best, best_d2);
}

size_t cinm_nn_index_find(const cinm_nn_index *idx, const float ctx[static NFEAT], float radius2) {
    size_t best    = MAX_CELLS;
    float  best_d2 = radius2;
    nn_search(idx, idx->root, ctx, &best, &best_d2);
    return best;
}

void cinm_nn_index_free(cinm_nn_index *idx) {
    free(idx->proto);
    free(idx->cell);
    free(idx->axis);
    free(idx->left);
    free(idx->right);
    idx->proto = nullptr;
    idx->cell  = nullptr;
    idx->axis  = nullptr;
    idx->left  = nullptr;
    idx->right = nullptr;
    idx->n    = 0;
    idx->root = NPOS;
}
