/* scale_nn_index_check.c — prove the NN index (cinm_nn_index.c) selects the IDENTICAL cell
 * as the linear prototype scan cinm_address_nn (cinm.c:58), so the kernel's byte-exact
 * behaviour is preserved when the KD-tree accelerates NN addressing (Faza 3b, D019; doc 11
 * rule #1).
 *
 * The contract's hard edge is the tie-break: cinm.c:64 updates best on `d2 <= best_d2` while
 * scanning ascending, so among cells at the minimum squared distance the HIGHEST index wins
 * (the opposite of exact-key's first-match, scale_index_check.c:8). A cell exactly at radius2
 * is included; a miss returns MAX_CELLS. Constructed fixtures use direct field writes, the
 * gate-only setup shortcut already used by scale_bench.c:fill_direct — never a kernel API. */
#include "cinm.h"
#include "cinm_nn_index.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <float.h>

static uint32_t lcg(uint32_t *s) { *s = *s * 1664525u + 1013904223u; return *s; }

/* cinm_address_nn's search loop WITHOUT the allocate-on-miss tail: the read-only reference
 * for cases that may miss (calling cinm_address_nn on a miss would birth a cell and mutate
 * the map). Byte-for-byte the same scan and `<=` rule as cinm.c:58-65. */
static size_t nn_scan(const cinm_map *m, const float ctx[static NFEAT], float radius2) {
    size_t best = MAX_CELLS;
    float  best_d2 = radius2;
    for (size_t i = 0; i < m->count; i++) {
        if (!m->in_use[i]) continue;
        float s = 0.0f;
        for (int k = 0; k < NFEAT; k++) { float d = m->proto[i][k] - ctx[k]; s += d * d; }
        if (s <= best_d2) { best_d2 = s; best = i; }
    }
    return best;
}

/* n live cells with pseudo-random, mostly-distinct prototypes (direct writes; gate-only). */
static void fill_random(cinm_map *m, size_t n, uint32_t seed) {
    cinm_init(m);
    uint32_t s = seed;
    for (size_t i = 0; i < n; i++) {
        for (int k = 0; k < NFEAT; k++)
            m->proto[i][k] = (float)(lcg(&s) % 4096u) * 0.01f;
        m->key[i]    = (uint32_t)i;
        m->in_use[i] = true;
    }
    m->count = n;
    m->t = (uint32_t)n;
}

static void rand_ctx(float ctx[static NFEAT], uint32_t *s) {
    for (int k = 0; k < NFEAT; k++) ctx[k] = (float)(lcg(s) % 4096u) * 0.01f;
}

int main(void) {
    cinm_map *m = malloc(sizeof *m);
    if (!m) { fprintf(stderr, "alloc failed\n"); return 1; }

    /* ---- check 1: hit-equivalence vs the real kernel over random queries (FLT_MAX) ---- */
    /* At FLT_MAX every query is a hit, so cinm_address_nn is read-only and is itself the
     * reference; nn_scan is bound to it too. Clamp N to the build capacity (equivalence is
     * scale-invariant, like scale_index_check.c). */
    const size_t N = MAX_CELLS < 4096 ? (size_t)MAX_CELLS : 4096;
    fill_random(m, N, 0xC1Au);
    cinm_nn_index idx;
    if (!cinm_nn_index_build(&idx, m)) { fprintf(stderr, "nn index build failed\n"); free(m); return 1; }

    bool hit_ok = true;
    uint32_t qs = 0x9E37u;
    for (size_t r = 0; r < 4096; r++) {
        float ctx[NFEAT];
        rand_ctx(ctx, &qs);
        bool   nov  = true;
        size_t ref  = cinm_address_nn(m, ctx, FLT_MAX, &nov);   /* read-only hit */
        size_t got  = cinm_nn_index_find(&idx, ctx, FLT_MAX);
        size_t sref = nn_scan(m, ctx, FLT_MAX);
        if (got != ref || sref != ref || nov || m->count != N) { hit_ok = false; break; }
    }

    /* ---- check 2: a miss (all prototypes beyond radius2) returns MAX_CELLS ---- */
    float far_ctx[NFEAT];
    for (int k = 0; k < NFEAT; k++) far_ctx[k] = 1000.0f;       /* protos live in [0,40.96) */
    bool miss_ok = cinm_nn_index_find(&idx, far_ctx, 1.0f) == MAX_CELLS
                && nn_scan(m, far_ctx, 1.0f) == MAX_CELLS;
    cinm_nn_index_free(&idx);

    /* ---- check 3: distance tie -> HIGHEST index wins (matches cinm.c:64) ---- */
    const size_t DN = 16, p = 3, q = 11;
    cinm_init(m);
    for (size_t i = 0; i < DN; i++) {
        for (int k = 0; k < NFEAT; k++) m->proto[i][k] = (float)((i + 1) * 10) + (float)k;
        m->in_use[i] = true;
    }
    float tie_ctx[NFEAT];
    for (int k = 0; k < NFEAT; k++) {                          /* cells p and q both sit on ctx */
        tie_ctx[k] = 0.0f;
        m->proto[p][k] = 0.0f;
        m->proto[q][k] = 0.0f;
    }
    m->count = DN;
    cinm_nn_index tidx;
    if (!cinm_nn_index_build(&tidx, m)) { fprintf(stderr, "tie index build failed\n"); free(m); return 1; }
    size_t tscan = nn_scan(m, tie_ctx, FLT_MAX);
    size_t tfind = cinm_nn_index_find(&tidx, tie_ctx, FLT_MAX);
    bool tie_ok = tscan == q && tfind == q;
    cinm_nn_index_free(&tidx);

    /* ---- check 4: a cell exactly at radius2 is included (inclusive <=) ---- */
    const size_t BN = 8, at = 5;
    cinm_init(m);
    for (size_t i = 0; i < BN; i++) {
        for (int k = 0; k < NFEAT; k++) m->proto[i][k] = 100.0f;   /* far: d2 = NFEAT*100^2 */
        m->in_use[i] = true;
    }
    float bnd_ctx[NFEAT] = {0};
    for (int k = 0; k < NFEAT; k++) m->proto[at][k] = 0.0f;
    m->proto[at][0] = 1.0f;                                     /* d2 to origin == 1.0 exactly */
    m->count = BN;
    cinm_nn_index bidx;
    if (!cinm_nn_index_build(&bidx, m)) { fprintf(stderr, "boundary index build failed\n"); free(m); return 1; }
    size_t bscan = nn_scan(m, bnd_ctx, 1.0f);
    size_t bfind = cinm_nn_index_find(&bidx, bnd_ctx, 1.0f);
    bool bnd_ok = bscan == at && bfind == at;
    cinm_nn_index_free(&bidx);

    /* ---- check 5: after a mutation (evict a cell), a rebuilt index re-agrees with scan ---- */
    const size_t RN = 64, evicted = 10;
    fill_random(m, RN, 0x5EEDu);
    m->in_use[evicted] = false;                                /* like a consolidation eviction */
    cinm_nn_index ridx;
    if (!cinm_nn_index_build(&ridx, m)) { fprintf(stderr, "rebuild index failed\n"); free(m); return 1; }
    bool rebuild_ok = ridx.n == RN - 1;
    uint32_t rs = 0x1234u;
    for (size_t r = 0; r < 4096 && rebuild_ok; r++) {
        float ctx[NFEAT];
        rand_ctx(ctx, &rs);
        size_t got = cinm_nn_index_find(&ridx, ctx, FLT_MAX);
        if (got != nn_scan(m, ctx, FLT_MAX) || got == evicted) rebuild_ok = false;
    }
    cinm_nn_index_free(&ridx);

    free(m);

    /* ---- report ---- */
    printf("nn index == scan over %zu random queries (hit): %s\n", (size_t)4096, hit_ok ? "PASS" : "FAIL");
    printf("all prototypes beyond radius -> miss (MAX_CELLS): %s\n", miss_ok ? "PASS" : "FAIL");
    printf("distance tie -> highest index wins (cinm.c:64): %s (scan=%zu find=%zu want=%zu)\n",
           tie_ok ? "PASS" : "FAIL", tscan, tfind, q);
    printf("cell exactly at radius2 included (inclusive <=): %s (scan=%zu find=%zu want=%zu)\n",
           bnd_ok ? "PASS" : "FAIL", bscan, bfind, at);
    printf("rebuild after evict re-agrees with scan: %s\n", rebuild_ok ? "PASS" : "FAIL");

    bool all = hit_ok && miss_ok && tie_ok && bnd_ok && rebuild_ok;
    return all ? 0 : 1;
}
