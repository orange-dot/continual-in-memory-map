/* cinm_index.c — exact-key open-addressing index for cinm_address (Faza 3a). See
 * cinm_index.h. The kernel (cinm.c/.h) is untouched: the linear scan stays the byte-exact
 * reference and scale_index_check.c proves this index selects the identical cell. */
#include "cinm_index.h"
#include <stdlib.h>

/* Smallest power of two >= x (x >= 1). */
static size_t next_pow2(size_t x) {
    size_t p = 1;
    while (p < x) p <<= 1;
    return p;
}

bool cinm_key_index_build(cinm_key_index *idx, const cinm_map *m) {
    size_t cap = next_pow2(2 * (m->count ? m->count : 1));   /* load factor <= 0.5 */
    idx->used  = calloc(cap, sizeof *idx->used);             /* zeroed: every slot empty */
    idx->key   = malloc(cap * sizeof *idx->key);
    idx->cell  = malloc(cap * sizeof *idx->cell);
    idx->cap   = cap;
    idx->count = 0;
    if (!idx->used || !idx->key || !idx->cell) { cinm_key_index_free(idx); return false; }

    size_t mask = cap - 1;
    for (size_t i = 0; i < m->count; i++) {
        if (!m->in_use[i]) continue;
        uint32_t k = m->key[i];
        size_t slot = (size_t)k & mask;
        for (;;) {
            if (!idx->used[slot]) {                 /* empty slot: claim it for this key */
                idx->used[slot] = true;
                idx->key[slot]  = k;
                idx->cell[slot] = i;
                idx->count++;
                break;
            }
            if (idx->key[slot] == k) break;         /* key already present at a lower cell
                                                       index (ascending scan): keep it —
                                                       this is cinm_address's first-match */
            slot = (slot + 1) & mask;               /* collision: probe the next slot */
        }
    }
    return true;
}

size_t cinm_key_index_find(const cinm_key_index *idx, uint32_t key) {
    size_t mask = idx->cap - 1;
    size_t slot = (size_t)key & mask;
    while (idx->used[slot]) {                        /* table is <= half full: terminates */
        if (idx->key[slot] == key) return idx->cell[slot];
        slot = (slot + 1) & mask;
    }
    return MAX_CELLS;                                /* absent: where cinm_address allocates */
}

void cinm_key_index_free(cinm_key_index *idx) {
    free(idx->used);
    free(idx->key);
    free(idx->cell);
    idx->used = nullptr;
    idx->key  = nullptr;
    idx->cell = nullptr;
    idx->cap   = 0;
    idx->count = 0;
}
