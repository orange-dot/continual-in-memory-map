/* scale_index_check.c — prove the exact-key index (cinm_index.c) selects the IDENTICAL
 * cell as the linear scan cinm_address (cinm.c:31), so the kernel's byte-exact behaviour
 * is preserved when the index accelerates addressing (Faza 3a, decision A; doc 11 rule #1).
 *
 * The contract has a tie-break edge: cinm_address returns on its FIRST match (cinm.c:33),
 * so on a duplicate key the LOWEST cell index wins. The index must reproduce exactly that.
 * (NN addressing's `<=` tie-break is the opposite — highest index — but the NN index is out
 * of this phase's scope.) Constructed fixtures use direct field writes, the bench/gate-only
 * setup shortcut already used by scale_bench.c:fill_direct — never a kernel API. */
#include "cinm.h"
#include "cinm_index.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

/* Distinct, scrambled keys: i -> i*GOLDEN (odd multiplier => a bijection on uint32, so
 * keys are distinct for distinct i and the low bits exercise hash collisions / probing). */
static const uint32_t GOLDEN = 2654435761u;
static uint32_t key_for(size_t i) { return (uint32_t)i * GOLDEN; }

/* cinm_address's search loop WITHOUT the allocate-on-miss tail: the read-only reference
 * for the miss case (calling cinm_address on a miss would mutate the map). Byte-for-byte
 * the same scan and first-match rule as cinm.c:31-37. */
static size_t scan_find(const cinm_map *m, uint32_t k) {
    for (size_t i = 0; i < m->count; i++)
        if (m->in_use[i] && m->key[i] == k) return i;
    return MAX_CELLS;
}

/* Fill n live cells with distinct scrambled keys (direct writes; gate-only, like fill_direct). */
static void fill_keys(cinm_map *m, size_t n) {
    cinm_init(m);
    for (size_t i = 0; i < n; i++) {
        m->key[i]    = key_for(i);
        m->in_use[i] = true;
    }
    m->count = n;
}

int main(void) {
    cinm_map *m = malloc(sizeof *m);
    if (!m) { fprintf(stderr, "alloc failed\n"); return 1; }

    /* ---- check 1: hit-equivalence over a full distinct-key map ---- */
    /* For every in-use key, the index agrees with the real cinm_address (a hit does not
     * mutate, so cinm_address itself is the reference here, not just scan_find). Clamp N to
     * the build capacity: equivalence is scale-invariant, so this gate is correct at the
     * default MAX_CELLS=256 and unchanged when built with a large -DCINM_MAX_CELLS. */
    const size_t N = MAX_CELLS < 4096 ? (size_t)MAX_CELLS : 4096;
    fill_keys(m, N);
    cinm_key_index idx;
    if (!cinm_key_index_build(&idx, m)) { fprintf(stderr, "index build failed\n"); free(m); return 1; }

    bool hit_ok = true;
    for (size_t i = 0; i < N; i++) {
        uint32_t k = m->key[i];
        bool nov = true;
        size_t scan = cinm_address(m, k, &nov);          /* hit: read-only, returns i */
        size_t got  = cinm_key_index_find(&idx, k);
        if (got != scan || got != i || nov) { hit_ok = false; break; }
    }

    /* ---- check 2: a miss returns MAX_CELLS, exactly where the scan would allocate ---- */
    uint32_t absent = key_for(N);                        /* i=N is outside [0,N): bijection => absent */
    bool miss_ok = cinm_key_index_find(&idx, absent) == MAX_CELLS
                && scan_find(m, absent) == MAX_CELLS;
    cinm_key_index_free(&idx);

    /* ---- check 3: duplicate-key fixture -> LOWEST index wins (matches cinm.c:33) ---- */
    const size_t DN = 16, p = 3, q = 11;                 /* p < q share a key */
    fill_keys(m, DN);
    uint32_t dk = m->key[p];
    m->key[q] = dk;
    cinm_key_index didx;
    if (!cinm_key_index_build(&didx, m)) { fprintf(stderr, "dup index build failed\n"); free(m); return 1; }
    size_t dscan = cinm_address(m, dk, nullptr);         /* first match -> p */
    size_t dfind = cinm_key_index_find(&didx, dk);
    bool dup_ok = dscan == p && dfind == p;
    cinm_key_index_free(&didx);

    /* ---- check 4: after a mutation (evict a cell), a rebuilt index re-agrees with scan ---- */
    /* The index is an auxiliary structure rebuilt as the map changes; this locks that a
     * freed cell is excluded and every key still resolves to the scan's cell. */
    const size_t RN = 64, evicted = 10;
    fill_keys(m, RN);
    m->in_use[evicted] = false;                          /* like a consolidation eviction */
    cinm_key_index ridx;
    if (!cinm_key_index_build(&ridx, m)) { fprintf(stderr, "rebuild index failed\n"); free(m); return 1; }
    bool rebuild_ok = true;
    for (size_t i = 0; i < RN; i++) {
        uint32_t k = m->key[i];                          /* i==evicted: key absent (not in_use) -> both miss */
        if (cinm_key_index_find(&ridx, k) != scan_find(m, k)) { rebuild_ok = false; break; }
    }
    bool evicted_absent = cinm_key_index_find(&ridx, key_for(evicted)) == MAX_CELLS;
    cinm_key_index_free(&ridx);

    free(m);

    /* ---- report ---- */
    printf("index == scan over %zu distinct keys (hit): %s\n", N, hit_ok ? "PASS" : "FAIL");
    printf("absent key -> miss (MAX_CELLS), matches scan: %s\n", miss_ok ? "PASS" : "FAIL");
    printf("duplicate key -> lowest index wins (cinm.c:33): %s (scan=%zu find=%zu want=%zu)\n",
           dup_ok ? "PASS" : "FAIL", dscan, dfind, p);
    printf("rebuild after evict re-agrees with scan: %s\n", (rebuild_ok && evicted_absent) ? "PASS" : "FAIL");

    bool all = hit_ok && miss_ok && dup_ok && rebuild_ok && evicted_absent;
    return all ? 0 : 1;
}
