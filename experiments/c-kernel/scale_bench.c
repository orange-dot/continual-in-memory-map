/* scale_bench.c — addressing/scoring/consolidation cost as the map grows across
 * the doc-12 cache regimes (L1 ~819, L2 ~35k, L3 ~420k, then DRAM). This is the
 * measurement that decision D011 deferred "until the model stabilizes" — that
 * condition is now met (R0-R5 green). It answers the doc-06 systems questions
 * ("how large can the map grow before retrieval dominates") and the doc-14 tail
 * model (report p50..p99.99, not just the mean).
 *
 * Build with a large capacity, e.g.:
 *   cc -O2 -std=c23 -DCINM_MAX_CELLS=1048576 -o scale scale_bench.c cinm.c
 * The map is HEAP-allocated (one cinm_map is ~90 MB at 1M cells) because the
 * kernel's by-value snapshot/undo pattern cannot put a map this large on the
 * stack. The kernel itself is untouched: its API already takes cinm_map*.
 *
 * Snapshot/undo are deliberately NOT swept here: they are O(struct size), so they
 * do not scale with live count — their non-scaling is a separate, known fact, not
 * a curve. Output is machine-parseable key=value lines (memory_bench.c format). */
#define _POSIX_C_SOURCE 200809L
#include "cinm.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <time.h>

/* Sweep points across the doc-12 cache regimes; clamped to MAX_CELLS at build. */
static const size_t SWEEP[] = { 256, 819, 4096, 35000, 131072, 420000, 1048576 };
enum { NSWEEP = (int)(sizeof SWEEP / sizeof SWEEP[0]) };

enum {
    MERGE_CAP    = 8192,     /* consolidate merge is O(N^2): cap + say so (doc 18) */
    MIN_SAMPLES  = 256,      /* per-call distribution floor (tail resolution)      */
    MAX_SAMPLES  = 8192,     /* ... and ceiling (runtime bound)                    */
    MIN_REPS     = 64,       /* amortized-mean loop floor                          */
    MAX_REPS     = 20000,    /* ... and ceiling                                    */
};
static const uint64_t SAMPLE_BUDGET = 200000000ULL; /* ~ comparisons across a sample set */
static const uint64_t WORK_BUDGET   = 100000000ULL; /* ~ comparisons across a mean loop   */

static volatile float    sinkf;
static volatile uint32_t sinku;

static uint64_t ns_now(void) {           /* same idiom as memory_bench.c */
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static uint32_t lcg(uint32_t *s) { *s = *s * 1664525u + 1013904223u; return *s; }

static size_t clampz(uint64_t x, size_t lo, size_t hi) {
    return x < lo ? lo : (x > hi ? hi : (size_t)x);
}

static int cmp_u64(const void *a, const void *b) {
    uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;
    return (x > y) - (x < y);
}

/* Direct fill: set up N live cells without paying cinm_address's O(N^2) insert
 * scan — we are measuring the kernel ops, not the fill. Mirrors alloc_cell. */
static void fill_direct(cinm_map *m, size_t n) {
    cinm_init(m);
    for (size_t i = 0; i < n; i++) {
        m->key[i] = (uint32_t)i;
        for (int k = 0; k < NFEAT; k++) {
            m->proto[i][k] = (float)((i * 31u + (uint32_t)k) % 97u) * 0.1f;
            m->w[i][k]     = (float)(((i + (uint32_t)k) & 7u)) * 0.25f - 1.0f;
        }
        m->plast[i] = 1.0f; m->conf[i] = 0.0f;
        m->evidence[i] = 1; m->born[i] = 0; m->last_touched[i] = 0;
        m->in_use[i] = true; m->frozen[i] = false;
    }
    m->count = n;
    m->t = (uint32_t)n;
}

static uint64_t percentile(const uint64_t *sorted, size_t n, double p) {
    if (n == 0) return 0;
    size_t idx = (size_t)(p * (double)(n - 1) + 0.5);
    return sorted[idx];
}

/* exact-key addressing: query an existing key (a hit) -> linear scan to it. */
static void bench_address(cinm_map *m, size_t n, uint64_t *buf, size_t *out_samples,
                          double *out_mean_ns) {
    uint32_t s = 0x1234u;
    size_t reps = clampz(WORK_BUDGET / (n ? n : 1), MIN_REPS, MAX_REPS);
    uint64_t t0 = ns_now();
    for (size_t r = 0; r < reps; r++)
        sinku += (uint32_t)cinm_address(m, lcg(&s) % (uint32_t)n, nullptr);
    uint64_t t1 = ns_now();
    *out_mean_ns = (double)(t1 - t0) / (double)reps;

    size_t samples = clampz(SAMPLE_BUDGET / (n ? n : 1), MIN_SAMPLES, MAX_SAMPLES);
    for (size_t i = 0; i < samples; i++) {
        uint32_t key = lcg(&s) % (uint32_t)n;
        uint64_t a = ns_now();
        sinku += (uint32_t)cinm_address(m, key, nullptr);
        uint64_t b = ns_now();
        buf[i] = b - a;
    }
    qsort(buf, samples, sizeof *buf, cmp_u64);
    *out_samples = samples;
}

/* nearest-neighbour addressing: huge radius -> always a hit (read-only, no alloc),
 * always a full O(n*NFEAT) prototype scan. */
static void bench_address_nn(cinm_map *m, size_t n, uint64_t *buf, size_t *out_samples,
                             double *out_mean_ns) {
    float ctx[NFEAT];
    for (int k = 0; k < NFEAT; k++) ctx[k] = (float)(k % 5) * 0.1f;
    size_t reps = clampz(WORK_BUDGET / ((n ? n : 1) * NFEAT), MIN_REPS, MAX_REPS);
    uint64_t t0 = ns_now();
    for (size_t r = 0; r < reps; r++)
        sinku += (uint32_t)cinm_address_nn(m, ctx, FLT_MAX, nullptr);
    uint64_t t1 = ns_now();
    *out_mean_ns = (double)(t1 - t0) / (double)reps;

    size_t samples = clampz(SAMPLE_BUDGET / ((n ? n : 1) * NFEAT), MIN_SAMPLES, MAX_SAMPLES);
    for (size_t i = 0; i < samples; i++) {
        uint64_t a = ns_now();
        sinku += (uint32_t)cinm_address_nn(m, ctx, FLT_MAX, nullptr);
        uint64_t b = ns_now();
        buf[i] = b - a;
    }
    qsort(buf, samples, sizeof *buf, cmp_u64);
    *out_samples = samples;
}

/* score over every live cell: O(NFEAT) per call, N-independent in work but
 * exposes cache behaviour on w[] as the map outgrows L1/L2/L3. */
static double bench_score_ns_per_cell(cinm_map *m, size_t n) {
    float phi[NFEAT];
    for (int k = 0; k < NFEAT; k++) phi[k] = (float)(k + 1) * 0.02f;
    size_t reps = clampz(WORK_BUDGET / (n ? n : 1), MIN_REPS, MAX_REPS);
    uint64_t t0 = ns_now();
    for (size_t r = 0; r < reps; r++)
        for (size_t i = 0; i < n; i++)
            sinkf += cinm_score(m, i, phi);
    uint64_t t1 = ns_now();
    return (double)(t1 - t0) / (double)(reps * n);
}

/* consolidate merge pass: O(n^2) prototype comparisons, capped (doc 18). Mutates,
 * so it runs on its own fresh fill. Reports total ns and ns per cell-pair. */
static void bench_consolidate(cinm_map *m, size_t n, double *out_ns, double *out_ns_per_pair) {
    fill_direct(m, n);
    cinm_consolidate_policy p = {
        .evict_floor = -1.0f, .evict_conf_max = -1.0f, .evict_idle_age = UINT32_MAX,
        .promote_conf = 2.0f, .promote_evidence = UINT32_MAX,
        .merge_radius2 = 0.5f,
    };
    uint64_t t0 = ns_now();
    cinm_consolidate_result r = cinm_consolidate(m, &p, (uint32_t)n, (uint32_t)n);
    uint64_t t1 = ns_now();
    (void)r;
    *out_ns = (double)(t1 - t0);
    double pairs = (double)n * (double)n / 2.0;
    *out_ns_per_pair = pairs > 0.0 ? (double)(t1 - t0) / pairs : 0.0;
}

int main(void) {
    cinm_map *m = malloc(sizeof *m);
    cinm_map *mc = malloc(sizeof *mc);
    uint64_t *buf = malloc((size_t)MAX_SAMPLES * sizeof *buf);
    if (!m || !mc || !buf) { fprintf(stderr, "alloc failed\n"); return 1; }

    printf("build_max_cells=%d nfeat=%d cell_bytes~=%zu\n",
           MAX_CELLS, NFEAT, sizeof *m / (size_t)MAX_CELLS);

    double addr_mean[NSWEEP], score_pc[NSWEEP];
    for (int si = 0; si < NSWEEP; si++) {
        size_t n = SWEEP[si];
        if (n > (size_t)MAX_CELLS) continue;

        fill_direct(m, n);

        double a_mean, nn_mean; size_t a_samp, nn_samp;
        bench_address(m, n, buf, &a_samp, &a_mean);
        uint64_t a_p50 = percentile(buf, a_samp, 0.50),  a_p95 = percentile(buf, a_samp, 0.95),
                 a_p99 = percentile(buf, a_samp, 0.99),  a_p999 = percentile(buf, a_samp, 0.999),
                 a_p9999 = percentile(buf, a_samp, 0.9999), a_max = percentile(buf, a_samp, 1.0);

        bench_address_nn(m, n, buf, &nn_samp, &nn_mean);
        uint64_t nn_p50 = percentile(buf, nn_samp, 0.50), nn_p99 = percentile(buf, nn_samp, 0.99),
                 nn_p999 = percentile(buf, nn_samp, 0.999), nn_max = percentile(buf, nn_samp, 1.0);

        double sc = bench_score_ns_per_cell(m, n);

        addr_mean[si] = a_mean; score_pc[si] = sc;

        printf("n=%zu "
               "address_mean_ns=%.1f address_p50=%llu address_p95=%llu address_p99=%llu "
               "address_p999=%llu address_p9999=%llu address_max=%llu address_samples=%zu "
               "nn_mean_ns=%.1f nn_p50=%llu nn_p99=%llu nn_p999=%llu nn_max=%llu nn_samples=%zu "
               "score_ns_per_cell=%.3f\n",
               n,
               a_mean, (unsigned long long)a_p50, (unsigned long long)a_p95,
               (unsigned long long)a_p99, (unsigned long long)a_p999,
               (unsigned long long)a_p9999, (unsigned long long)a_max, a_samp,
               nn_mean, (unsigned long long)nn_p50, (unsigned long long)nn_p99,
               (unsigned long long)nn_p999, (unsigned long long)nn_max, nn_samp,
               sc);
        fflush(stdout);
    }

    /* consolidate merge, capped */
    for (int si = 0; si < NSWEEP; si++) {
        size_t n = SWEEP[si];
        if (n > (size_t)MAX_CELLS || n > MERGE_CAP) continue;
        double cns, cpp;
        bench_consolidate(mc, n, &cns, &cpp);
        printf("consolidate_n=%zu merge_total_ns=%.0f merge_ns_per_pair=%.3f\n", n, cns, cpp);
        fflush(stdout);
    }

    /* crossover: first swept n where one exact-key lookup costs >= the per-cell
     * score+update work it gates (retrieval dominates, doc 06/14). Update ~= score. */
    long crossover = -1;
    for (int si = 0; si < NSWEEP; si++) {
        size_t n = SWEEP[si];
        if (n > (size_t)MAX_CELLS) continue;
        if (addr_mean[si] >= 2.0 * score_pc[si]) { crossover = (long)n; break; }
    }
    if (crossover < 0) printf("crossover_n=none (retrieval never dominated across the sweep)\n");
    else               printf("crossover_n=%ld\n", crossover);

    free(buf); free(mc); free(m);
    return 0;
}
