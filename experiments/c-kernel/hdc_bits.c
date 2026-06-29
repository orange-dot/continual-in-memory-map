/* hdc_bits.c — bit-packed HDC baseline: XOR bind and popcount similarity. */
#include <stdio.h>
#include <stdint.h>

enum { HDC_WORDS = 16, HDC_BITS = HDC_WORDS * 64 };

static unsigned popcount_scalar(uint64_t x) {
    unsigned n = 0;
    while (x) {
        n += (unsigned)(x & 1u);
        x >>= 1;
    }
    return n;
}

static unsigned popcount_fast(uint64_t x) {
#if defined(__GNUC__) || defined(__clang__)
    return (unsigned)__builtin_popcountll(x);
#else
    return popcount_scalar(x);
#endif
}

static void bind_xor(uint64_t out[static HDC_WORDS],
                     const uint64_t a[static HDC_WORDS],
                     const uint64_t b[static HDC_WORDS]) {
    for (int i = 0; i < HDC_WORDS; i++)
        out[i] = a[i] ^ b[i];
}

static unsigned distance_scalar(const uint64_t a[static HDC_WORDS],
                                const uint64_t b[static HDC_WORDS]) {
    unsigned d = 0;
    for (int i = 0; i < HDC_WORDS; i++)
        d += popcount_scalar(a[i] ^ b[i]);
    return d;
}

static unsigned distance_fast(const uint64_t a[static HDC_WORDS],
                              const uint64_t b[static HDC_WORDS]) {
    unsigned d = 0;
    for (int i = 0; i < HDC_WORDS; i++)
        d += popcount_fast(a[i] ^ b[i]);
    return d;
}

static void make_vec(uint64_t v[static HDC_WORDS], uint64_t seed) {
    uint64_t x = seed;
    for (int i = 0; i < HDC_WORDS; i++) {
        x += 0x9E3779B97F4A7C15ULL;
        uint64_t z = x;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        v[i] = z ^ (z >> 31);
    }
}

int main(void) {
    uint64_t a[HDC_WORDS], b[HDC_WORDS], c[HDC_WORDS], bound[HDC_WORDS];
    make_vec(a, 1);
    make_vec(b, 2);
    make_vec(c, 3);
    bind_xor(bound, a, b);
    unsigned ds = distance_scalar(bound, c);
    unsigned df = distance_fast(bound, c);
    unsigned sim = HDC_BITS - df;
    bool ok = ds == df && df <= HDC_BITS && sim <= HDC_BITS;
    printf("hdc_bits distance_scalar=%u distance_fast=%u similarity=%u -> %s\n",
           ds, df, sim, ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
