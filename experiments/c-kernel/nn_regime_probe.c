/* nn_regime_probe.c — closes the v3 NN-lookup question: is the O(count) prototype scan a
 * real problem in the regime NN addressing actually produces? Two cheap measurements:
 *
 *   A) Bounded radius. v3 measured cinm_address_nn at radius2 = FLT_MAX (unbounded global
 *      1-NN, the worst case for the index). The real D019 addressing uses a finite novelty
 *      radius. Time the KD find at FLT_MAX vs a tight radius to show how much the finite
 *      radius prunes.
 *
 *   B) Realistic count. NN addressing exists to COMPRESS: near-identical contexts share a
 *      cell (run-nn-address: 256 contexts -> 4 cells). Feed a clustered context stream
 *      through cinm_address_nn with a novelty radius and watch the live count: it saturates
 *      at the cluster count, NOT the stream length. So the scan stays O(clusters) — a few
 *      microseconds — and the index is moot for the workload NN addressing is designed for.
 *      The 1M-distinct load v3 indexed is the case working NN addressing never reaches.
 *
 * Auxiliary, like the bench: links cinm.c (untouched) + cinm_nn_index.c. */
#define _POSIX_C_SOURCE 200809L
#include "cinm.h"
#include "cinm_nn_index.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <float.h>
#include <time.h>

static volatile uint32_t sinku;

static uint64_t ns_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static uint32_t lcg(uint32_t *s) { *s = *s * 1664525u + 1013904223u; return *s; }
static float coord(uint32_t *s) { return (float)(lcg(s) & 0xFFFFu) * (1.0f / 6553.6f); }   /* [0,10)   */
static float jitter(uint32_t *s) { return ((float)(lcg(s) & 0xFFFFu) / 65535.0f - 0.5f) * 0.2f; } /* [-0.1,0.1] */

static void fill_distinct(cinm_map *m, size_t n) {
    cinm_init(m);
    uint32_t s = 0xBEEFu;
    for (size_t i = 0; i < n; i++) {
        for (int k = 0; k < NFEAT; k++) m->proto[i][k] = coord(&s);
        m->in_use[i] = true;
    }
    m->count = n;
}

int main(void) {
    cinm_map *m = malloc(sizeof *m);
    if (!m) { fprintf(stderr, "alloc failed\n"); return 1; }

    /* ---- A) bounded radius: how much a finite novelty radius prunes (vs v3's FLT_MAX) ---- */
    const size_t AN = 131072;
    if (AN <= (size_t)MAX_CELLS) {
        fill_distinct(m, AN);
        cinm_nn_index idx;
        if (!cinm_nn_index_build(&idx, m)) { fprintf(stderr, "build failed\n"); free(m); return 1; }
        enum { Q = 200000 };
        for (int pass = 0; pass < 2; pass++) {
            float radius2 = pass == 0 ? FLT_MAX : 1.0f;
            uint32_t s = 0x1234u;
            uint64_t t0 = ns_now();
            for (size_t r = 0; r < Q; r++) {
                float ctx[NFEAT];
                for (int k = 0; k < NFEAT; k++) ctx[k] = coord(&s);
                sinku += (uint32_t)cinm_nn_index_find(&idx, ctx, radius2);
            }
            uint64_t t1 = ns_now();
            printf("A radius2=%-9s n=%zu kd_find_mean_ns=%.2f\n",
                   pass == 0 ? "FLT_MAX" : "1.0", AN, (double)(t1 - t0) / (double)Q);
        }
        cinm_nn_index_free(&idx);
    }

    /* ---- B) realistic count: clustered stream through cinm_address_nn (the designed use) ---- */
    const size_t C = 1000;                          /* cluster centers (distinct schemas)     */
    const float  NOVELTY2 = 1.0f;                   /* > jitter^2 (~0.32), << inter-cluster (~133) */
    float *center = malloc(C * (size_t)NFEAT * sizeof *center);
    if (!center) { fprintf(stderr, "alloc failed\n"); free(m); return 1; }
    uint32_t cs = 0xC0DEu;
    for (size_t c = 0; c < C; c++)
        for (int k = 0; k < NFEAT; k++) center[c * NFEAT + k] = coord(&cs);

    cinm_init(m);
    const size_t CHECK[] = { 1000, 5000, 20000, 200000 };
    enum { NCHECK = (int)(sizeof CHECK / sizeof CHECK[0]) };
    uint32_t ss = 0x77u;
    size_t next = 0;
    printf("B clusters=%zu novelty_radius2=%.1f  (count should saturate near %zu, not the stream length)\n",
           C, (double)NOVELTY2, C);
    for (size_t step = 1; step <= CHECK[NCHECK - 1]; step++) {
        size_t c = lcg(&ss) % C;
        float ctx[NFEAT];
        for (int k = 0; k < NFEAT; k++) ctx[k] = center[c * NFEAT + k] + jitter(&ss);
        bool nov = false;
        sinku += (uint32_t)cinm_address_nn(m, ctx, NOVELTY2, &nov);
        if (next < NCHECK && step == CHECK[next]) {
            printf("B stream=%zu live_count=%zu\n", step, m->count);
            next++;
        }
    }

    /* realistic addressing cost at the saturated count (in-cluster queries -> hits, read-only) */
    uint32_t qs = 0x99u;
    enum { HQ = 200000 };
    uint64_t h0 = ns_now();
    for (size_t r = 0; r < HQ; r++) {
        size_t c = lcg(&qs) % C;
        float ctx[NFEAT];
        for (int k = 0; k < NFEAT; k++) ctx[k] = center[c * NFEAT + k] + jitter(&qs);
        sinku += (uint32_t)cinm_address_nn(m, ctx, NOVELTY2, nullptr);
    }
    uint64_t h1 = ns_now();
    printf("B saturated_count=%zu scan_address_mean_ns=%.1f  (vs v3 1M-distinct scan ~6.17e6 ns)\n",
           m->count, (double)(h1 - h0) / (double)HQ);

    free(center);
    free(m);
    return 0;
}
