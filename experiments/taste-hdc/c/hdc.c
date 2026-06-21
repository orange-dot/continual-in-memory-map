/* hdc.c — hyperdimensional computing primitives in C, neuromorphic flavor.
 *
 * The Python version used int8 (+1/-1) for clarity. This one is the substrate-
 * honest version: hypervectors are *bit-packed binary*, 1 bit per dimension.
 * That is the representation that maps onto neuromorphic / in-memory hardware:
 *   bind       == XOR            (word-wise, reversible)
 *   similarity == popcount(XOR)  (Hamming distance -> agreement)
 *   bundle     == per-bit majority vote
 * 32x smaller than int8, and every op is a bitwise / popcount instruction.
 *
 * Build:  cc -O2 -Wall -o hdc hdc.c && ./hdc
 */
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#define DIM   8192        /* bits per hypervector (kept a multiple of 64) */
#define WORDS (DIM / 64)  /* 64-bit words per hypervector                 */

typedef struct { uint64_t w[WORDS]; } hv_t;

/* --- splitmix64: tiny seeded PRNG, reproducible ------------------------- */
static uint64_t rng_state;
static uint64_t next_rand(void) {
    uint64_t z = (rng_state += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

/* A fresh random hypervector — the atom of representation. */
static void random_hv(hv_t *out) {
    for (size_t i = 0; i < WORDS; i++)
        out->w[i] = next_rand();
}

/* Bind: elementwise XOR — reversible association (bind with b again to undo). */
static void bind(hv_t *out, const hv_t *a, const hv_t *b) {
    for (size_t i = 0; i < WORDS; i++)
        out->w[i] = a->w[i] ^ b->w[i];
}

/* Similarity in [-1, 1]: agreement = 1 - 2 * Hamming / DIM. */
static double similarity(const hv_t *a, const hv_t *b) {
    int ham = 0;
    for (size_t i = 0; i < WORDS; i++)
        ham += __builtin_popcountll(a->w[i] ^ b->w[i]);
    return 1.0 - 2.0 * (double)ham / (double)DIM;
}

/* Bundle: per-bit majority vote over n hypervectors (superposition). */
static void bundle(hv_t *out, const hv_t *hvs, size_t n) {
    for (size_t bit = 0; bit < DIM; bit++) {
        int count = 0;
        for (size_t i = 0; i < n; i++)
            count += (int)((hvs[i].w[bit >> 6] >> (bit & 63)) & 1u);
        if (2 * count >= (int)n)
            out->w[bit >> 6] |= (uint64_t)1 << (bit & 63);
        else
            out->w[bit >> 6] &= ~((uint64_t)1 << (bit & 63));
    }
}

int main(void) {
    rng_state = 1;

    hv_t a, b, c;
    random_hv(&a); random_hv(&b); random_hv(&c);

    /* Quasi-orthogonality: two random hypervectors share ~nothing. */
    printf("sim(a, b)          = %+.3f   (random -> ~0)\n", similarity(&a, &b));

    /* Bundle stays similar to every member. */
    hv_t set[3] = { a, b, c }, bund;
    bundle(&bund, set, 3);
    printf("sim(bundle, a)     = %+.3f   (member -> high)\n", similarity(&bund, &a));
    printf("sim(bundle, c)     = %+.3f   (member -> high)\n", similarity(&bund, &c));

    /* Bind is dissimilar to its inputs, but exactly reversible. */
    hv_t ab, recovered;
    bind(&ab, &a, &b);
    printf("sim(bind(a,b), a)  = %+.3f   (bound -> ~0)\n", similarity(&ab, &a));
    bind(&recovered, &ab, &b);            /* unbind: XOR with b again */
    printf("sim(unbind, a)     = %+.3f   (reversible -> 1.0)\n", similarity(&recovered, &a));

    return 0;
}
