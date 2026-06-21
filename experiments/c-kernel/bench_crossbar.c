/* bench_crossbar.c — sparse vs dense (crossbar) scoring microbenchmark.
 *
 * Standalone, pure C, no libraries. A crossbar scores every cell in one analog
 * MVM (O(1) in hardware); here that is emulated as a dense pass and contrasted
 * with the sparse path (address one cell, score it) across map sizes N, in two
 * regimes: exact-key and similarity/NN.  Build: make run-bench
 */
#include "cinm.h"        /* NFEAT */
#include <stdio.h>
#include <stdint.h>
#include <time.h>

enum { MAXN = 1 << 16 };        /* largest map size in the sweep         */
enum { NQP  = 256 };            /* precomputed query pool (power of two)  */
enum { TARGET_OPS = 1 << 28 };  /* work per measurement -> stable clock() */

static uint32_t keys[MAXN];
static float    W[MAXN][NFEAT];
static float    scores[MAXN];
static float    qphi[NQP][NFEAT];
static uint32_t qtgt[NQP];

/* splitmix64 — seeded PRNG, no libraries. */
static uint64_t rng = 0x243F6A8885A308D3ULL;
static uint64_t nx(void) {
    uint64_t z = (rng += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}
static float rnd01(void) { return (float)(nx() >> 40) * (1.0f / 16777216.0f); }

static float dotf(const float a[static NFEAT], const float b[static NFEAT]) {
    float s = 0.0f;
    for (int k = 0; k < NFEAT; k++) s += a[k] * b[k];
    return s;
}

/* Dense crossbar model: score every cell in one pass (the analog MVM, emulated). */
static void crossbar_mvm(size_t n, const float phi[static NFEAT], float *out) {
    for (size_t i = 0; i < n; i++) out[i] = dotf(W[i], phi);
}

/* Sparse, ideal addressing: O(1) direct index, score one cell. */
static float sparse_direct(uint32_t idx, const float phi[static NFEAT]) {
    return dotf(W[idx], phi);
}

/* Sparse, current-kernel addressing: O(n) linear key scan, score one cell. */
static float sparse_scan(size_t n, uint32_t key, const float phi[static NFEAT]) {
    for (size_t i = 0; i < n; i++)
        if (keys[i] == key) return dotf(W[i], phi);
    return 0.0f;
}

/* Similarity addressing: best cell by score — must touch all cells (== dense). */
static float nn_max(size_t n, const float phi[static NFEAT]) {
    float best = -1e30f;
    for (size_t i = 0; i < n; i++) {
        float s = dotf(W[i], phi);
        if (s > best) best = s;
    }
    return best;
}

static double ns_per_query(clock_t dt, long q) {
    return (double)dt / (double)CLOCKS_PER_SEC / (double)q * 1e9;
}

int main(void) {
    static const size_t sweep[] = {16, 64, 256, 1024, 4096, 16384, 65536};
    const size_t nsweep = sizeof sweep / sizeof sweep[0];

    for (size_t i = 0; i < MAXN; i++) {
        keys[i] = (uint32_t)i;
        for (int k = 0; k < NFEAT; k++) W[i][k] = rnd01() * 2.0f - 1.0f;
    }
    for (int j = 0; j < NQP; j++) {
        for (int k = 0; k < NFEAT; k++) qphi[j][k] = rnd01();
        qtgt[j] = (uint32_t)(nx() % MAXN);
    }

    /* Agreement: dense score at the addressed column == the sparse score. */
    {
        const float *phi = qphi[0];
        uint32_t t = qtgt[0] % 1024u;
        crossbar_mvm(1024, phi, scores);
        float diff = scores[t] - sparse_direct(t, phi);
        if (diff < 0.0f) diff = -diff;
        printf("agreement (N=1024): diff=%.2e -> %s\n\n",
               (double)diff, diff < 1e-4f ? "PASS" : "FAIL");
    }

    printf("%-8s %11s %11s %11s %11s %10s\n",
           "N", "direct ns", "scan ns", "dense ns", "nn ns", "dense/dir");

    volatile float sink = 0.0f;
    for (size_t s = 0; s < nsweep; s++) {
        size_t n = sweep[s];
        uint32_t mask = (uint32_t)(n - 1);   /* sweep values are powers of two */
        clock_t t0, t1;
        long q;
        float acc;
        double d_direct, d_scan, d_dense, d_nn;

        q = TARGET_OPS / (long)NFEAT;
        acc = 0.0f; t0 = clock();
        for (long i = 0; i < q; i++) {
            int j = (int)(i & (NQP - 1));
            acc += sparse_direct(qtgt[j] & mask, qphi[j]);
        }
        t1 = clock(); sink += acc;
        d_direct = ns_per_query(t1 - t0, q);

        q = TARGET_OPS / (long)n; if (q < 1) q = 1;
        acc = 0.0f; t0 = clock();
        for (long i = 0; i < q; i++) {
            int j = (int)(i & (NQP - 1));
            acc += sparse_scan(n, qtgt[j] & mask, qphi[j]);
        }
        t1 = clock(); sink += acc;
        d_scan = ns_per_query(t1 - t0, q);

        q = TARGET_OPS / ((long)n * NFEAT); if (q < 1) q = 1;
        acc = 0.0f; t0 = clock();
        for (long i = 0; i < q; i++) {
            int j = (int)(i & (NQP - 1));
            crossbar_mvm(n, qphi[j], scores);
            acc += scores[qtgt[j] & mask];
        }
        t1 = clock(); sink += acc;
        d_dense = ns_per_query(t1 - t0, q);

        q = TARGET_OPS / ((long)n * NFEAT); if (q < 1) q = 1;
        acc = 0.0f; t0 = clock();
        for (long i = 0; i < q; i++) {
            int j = (int)(i & (NQP - 1));
            acc += nn_max(n, qphi[j]);
        }
        t1 = clock(); sink += acc;
        d_nn = ns_per_query(t1 - t0, q);

        printf("%-8zu %11.2f %11.2f %11.2f %11.2f %10.1f\n",
               n, d_direct, d_scan, d_dense, d_nn, d_dense / d_direct);
    }
    printf("\n(checksum %.3f)\n", (double)sink);
    return 0;
}
