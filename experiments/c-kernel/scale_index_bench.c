/* scale_index_bench.c — the payoff of Faza 3a: exact-key addressing cost with the
 * cinm_index hash vs the linear scan cinm_address, across the doc-12 cache regimes.
 * Companion to scale_bench.c (which measured the scan-only baseline, cim-sys-scale-v1).
 * The kernel is untouched; this links cinm.c + cinm_index.c.
 *
 * Build with a large capacity, e.g.:
 *   cc -O2 -std=c23 -DCINM_MAX_CELLS=1048576 -o scale-index-bench scale_index_bench.c cinm.c cinm_index.c
 *
 * Honest accounting (doc 18): the index is build-once / query-many, so the fair number for
 * read-heavy addressing is the AMORTIZED per-query cost. The O(count) build cost is reported
 * SEPARATELY (index_build_ns / _per_cell) — never hidden inside the query figure. Output is
 * machine-parseable key=value lines (scale_bench.c / memory_bench.c format). */
#define _POSIX_C_SOURCE 200809L
#include "cinm.h"
#include "cinm_index.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

/* Same sweep points as scale_bench.c; clamped to MAX_CELLS at build. */
static const size_t SWEEP[] = { 256, 819, 4096, 35000, 131072, 420000, 1048576 };
enum { NSWEEP = (int)(sizeof SWEEP / sizeof SWEEP[0]) };

enum { MIN_REPS = 64, MAX_REPS = 20000, INDEX_REPS = 4000000 };
static const uint64_t WORK_BUDGET = 100000000ULL;   /* ~comparisons across the scan loop */

static const uint32_t GOLDEN = 2654435761u;         /* odd => bijection => distinct keys */
static uint32_t key_for(size_t i) { return (uint32_t)i * GOLDEN; }

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

/* N live cells with distinct scrambled keys (direct writes; bench-only, like fill_direct). */
static void fill_keys(cinm_map *m, size_t n) {
    cinm_init(m);
    for (size_t i = 0; i < n; i++) {
        m->key[i]    = key_for(i);
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
        fill_keys(m, n);

        /* index build cost (reported separately) */
        cinm_key_index idx;
        uint64_t b0 = ns_now();
        if (!cinm_key_index_build(&idx, m)) { fprintf(stderr, "index build failed\n"); free(m); return 1; }
        uint64_t b1 = ns_now();
        double build_ns = (double)(b1 - b0);

        /* scan: cinm_address on random existing keys (hits, read-only) */
        uint32_t s = 0x1234u;
        size_t scan_reps = clampz(WORK_BUDGET / (n ? n : 1), MIN_REPS, MAX_REPS);
        uint64_t t0 = ns_now();
        for (size_t r = 0; r < scan_reps; r++)
            sinku += (uint32_t)cinm_address(m, key_for(lcg(&s) % (uint32_t)n), nullptr);
        uint64_t t1 = ns_now();
        double scan_mean = (double)(t1 - t0) / (double)scan_reps;

        /* index: cinm_key_index_find on the same key distribution (O(1) -> many reps) */
        s = 0x1234u;
        uint64_t u0 = ns_now();
        for (size_t r = 0; r < INDEX_REPS; r++)
            sinku += (uint32_t)cinm_key_index_find(&idx, key_for(lcg(&s) % (uint32_t)n));
        uint64_t u1 = ns_now();
        double index_mean = (double)(u1 - u0) / (double)INDEX_REPS;

        cinm_key_index_free(&idx);

        printf("n=%zu address_mean_ns=%.1f index_mean_ns=%.2f speedup=%.1fx "
               "index_build_ns=%.0f index_build_ns_per_cell=%.2f\n",
               n, scan_mean, index_mean,
               index_mean > 0.0 ? scan_mean / index_mean : 0.0,
               build_ns, n ? build_ns / (double)n : 0.0);
        fflush(stdout);
    }

    free(m);
    return 0;
}
