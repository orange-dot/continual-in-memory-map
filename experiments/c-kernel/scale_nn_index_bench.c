/* scale_nn_index_bench.c — the payoff of Faza 3b: NN (prototype) addressing cost with the
 * cinm_nn_index KD-tree vs the linear prototype scan cinm_address_nn, across the doc-12 cache
 * regimes. Companion to scale_bench.c (which measured the NN-scan baseline, cim-sys-scale-v1)
 * and scale_index_bench.c (the exact-key payoff). The kernel is untouched; this links
 * cinm.c + cinm_nn_index.c.
 *
 * Build with a large capacity, e.g.:
 *   cc -O2 -std=c23 -DCINM_MAX_CELLS=1048576 -o scale-nn-index-bench scale_nn_index_bench.c cinm.c cinm_nn_index.c
 *
 * Honest accounting (doc 18): the index is build-once / query-many, so the fair number for
 * read-heavy NN addressing is the AMORTIZED per-query cost. The O(count log count) build cost
 * is reported SEPARATELY (nn_index_build_ns / _per_cell), never hidden inside the query
 * figure. Prototypes here are high-entropy (near-distinct), the case a metric index is built
 * for; on heavily-duplicated prototypes the KD-tree degrades toward the scan (a doc-18
 * watch, see the v3 summary). Output is machine-parseable key=value lines. */
#define _POSIX_C_SOURCE 200809L
#include "cinm.h"
#include "cinm_nn_index.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <float.h>
#include <time.h>

/* Same sweep points as scale_bench.c; clamped to MAX_CELLS at build. */
static const size_t SWEEP[] = { 256, 819, 4096, 35000, 131072, 420000, 1048576 };
enum { NSWEEP = (int)(sizeof SWEEP / sizeof SWEEP[0]) };

enum { MIN_REPS = 64, MAX_REPS = 20000, NN_INDEX_REPS = 200000 };
static const uint64_t WORK_BUDGET = 100000000ULL;   /* ~ prototype comparisons across the scan loop */

static volatile uint32_t sinku;

static uint64_t ns_now(void) {                      /* same idiom as scale_bench.c:43 */
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static uint32_t lcg(uint32_t *s) { *s = *s * 1664525u + 1013904223u; return *s; }

static size_t clampz(uint64_t x, size_t lo, size_t hi) {
    return x < lo ? lo : (x > hi ? hi : (size_t)x);
}

static void rand_ctx(float ctx[static NFEAT], uint32_t *s) {
    for (int k = 0; k < NFEAT; k++) ctx[k] = (float)(lcg(s) & 0xFFFFu) * (1.0f / 6553.6f); /* [0,10) */
}

/* n live cells with high-entropy prototypes (direct writes; bench-only, like fill_direct). */
static void fill_random(cinm_map *m, size_t n) {
    cinm_init(m);
    uint32_t s = 0xBEEFu;
    for (size_t i = 0; i < n; i++) {
        float ctx[NFEAT];
        rand_ctx(ctx, &s);
        for (int k = 0; k < NFEAT; k++) m->proto[i][k] = ctx[k];
        m->key[i]    = (uint32_t)i;
        m->in_use[i] = true;
    }
    m->count = n;
    m->t = (uint32_t)n;
}

int main(void) {
    cinm_map *m = malloc(sizeof *m);
    if (!m) { fprintf(stderr, "alloc failed\n"); return 1; }

    printf("build_max_cells=%d nfeat=%d\n", MAX_CELLS, NFEAT);

    for (int si = 0; si < NSWEEP; si++) {
        size_t n = SWEEP[si];
        if (n > (size_t)MAX_CELLS) continue;
        fill_random(m, n);

        /* index build cost (reported separately) */
        cinm_nn_index idx;
        uint64_t b0 = ns_now();
        if (!cinm_nn_index_build(&idx, m)) { fprintf(stderr, "nn index build failed\n"); free(m); return 1; }
        uint64_t b1 = ns_now();
        double build_ns = (double)(b1 - b0);

        /* scan: cinm_address_nn on random queries (FLT_MAX => always a read-only hit) */
        uint32_t s = 0x1234u;
        size_t scan_reps = clampz(WORK_BUDGET / ((n ? n : 1) * NFEAT), MIN_REPS, MAX_REPS);
        uint64_t t0 = ns_now();
        for (size_t r = 0; r < scan_reps; r++) {
            float ctx[NFEAT];
            rand_ctx(ctx, &s);
            sinku += (uint32_t)cinm_address_nn(m, ctx, FLT_MAX, nullptr);
        }
        uint64_t t1 = ns_now();
        double scan_mean = (double)(t1 - t0) / (double)scan_reps;

        /* index: cinm_nn_index_find on the same query distribution (O(log n) -> many reps) */
        s = 0x1234u;
        uint64_t u0 = ns_now();
        for (size_t r = 0; r < NN_INDEX_REPS; r++) {
            float ctx[NFEAT];
            rand_ctx(ctx, &s);
            sinku += (uint32_t)cinm_nn_index_find(&idx, ctx, FLT_MAX);
        }
        uint64_t u1 = ns_now();
        double index_mean = (double)(u1 - u0) / (double)NN_INDEX_REPS;

        cinm_nn_index_free(&idx);

        printf("n=%zu nn_address_mean_ns=%.1f nn_index_mean_ns=%.2f speedup=%.1fx "
               "nn_index_build_ns=%.0f nn_index_build_ns_per_cell=%.2f\n",
               n, scan_mean, index_mean,
               index_mean > 0.0 ? scan_mean / index_mean : 0.0,
               build_ns, n ? build_ns / (double)n : 0.0);
        fflush(stdout);
    }

    free(m);
    return 0;
}
