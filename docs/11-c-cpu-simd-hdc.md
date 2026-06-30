# C CPU/SIMD Notes For CINM And HDC

Reviewed: 2026-06-21

This document records the C-level CPU/RAM angle for CINM and HDC experiments on
the current local machine.

It is based on the local hardware scan produced by:

```text
/home/dev/scan-cpu-ram.sh
```

Report path from the first run:

```text
/home/dev/cpu-ram-report-20260621-013435.txt
```

The goal is not to commit the project to hand-written SIMD immediately. The
goal is to make the C substrate aware of what this CPU is good at, and to shape
the first kernels so the compiler and later hand-vectorized code can exploit it.

## Local Hardware Summary

> **Two machines, two roles (added 2026-06-29).** The Lenovo P16v summarized below
> ("Machine:" and the CPU/cache/RAM summaries) is the **development workstation**.
> Every benchmark number in these docs — `cim-sys-scale-v1` and the Faza 3a index
> work — comes from a separate **measurement / execution host**, the i7-4600U
> documented immediately below, never from the workstation. Any cache-regime or
> latency figure in these docs is the measurement host's.

### Current measurement host (2026-06-29)

The host that actually executes the C-kernel benchmarks in this lab —
`cim-sys-scale-v1` and the Faza 3a index work — is **not** the P16v workstation.
Measured directly on it (`lscpu`, `/sys/.../cache`, `/proc/meminfo`; `cc`/`gcc`
16.1.1):

```text
CPU:    Intel Core i7-4600U (Haswell; family 6, model 69, stepping 1)
        2 cores / 4 threads (HT); 2.10 GHz base, 3.30 GHz turbo, 0.80 GHz min
        bare-metal (systemd-detect-virt: none); 1 NUMA node
Cache:  L1d 32 KiB/core, L1i 32 KiB/core, L2 256 KiB/core, L3 4 MiB shared; 64 B line
ISA:    SSE..SSE4.2, AVX, AVX2, FMA, F16C, BMI1, BMI2, POPCNT, AES, PCLMULQDQ, MOVBE, RDRAND
        NO AVX-512, NO AVX-VNNI, NO VAES, NO VPCLMULQDQ
RAM:    7.6 GiB total (MemTotal 8,014,140 kB); swap 7.6 GiB
        (DIMM type / MT-s / part number unavailable — dmidecode needs root here)
```

Consequences for these docs:

- **This host is behind every benchmark number here.** It is the same i7-4600U
  recorded in `runs/cim-sys-scale-v1/config.toml`, so v1 and the Faza 3a numbers
  (`cinm_address` ~374 µs vs index ~33 ns at 1M) are self-consistent. The earlier
  provenance mismatch was the workstation summary, not the data.
- **Memory is 8 GiB, not 64 GiB.** The 1M-cell map (~90 MB) still fits, so the scale
  sweep is valid — but the "64 GiB headroom for large maps / archives / logs" framing
  is the workstation's, not where the experiments run.
- **ISA baseline is AVX2 + FMA only** — no AVX-512, and (unlike the workstation) no
  AVX-VNNI / VAES / VPCLMULQDQ. A VNNI quantized path cannot be validated on this
  host; the scalar + auto-vectorized AVX2 baseline (C design rules #1/#2) is all it
  runs.
- **Cell regimes at the 90 B cell:** L1d ~364, L2 ~2913, L3 ~46603 cells, then DRAM —
  identical to `cim-sys-scale-v1`.

Machine:

```text
Lenovo ThinkPad P16v Gen 2
CPU: Intel Core Ultra 7 165H
RAM: 64 GiB DDR5 SODIMM, 5600 MT/s
```

CPU summary:

```text
Architecture: x86_64
Logical CPUs: 22
Physical core shape: 16 cores total
  - 6 performance cores with SMT = 12 threads
  - 8 efficiency cores = 8 threads
  - 2 low-power efficiency cores = 2 threads
Max turbo: up to 5.0 GHz
Min frequency: 400 MHz
NUMA nodes: 1
Virtualization: VT-x
```

Cache summary:

```text
L1d: 544 KiB total
L1i: 896 KiB total
L2: 18 MiB total
L3: 24 MiB total
Cache line size: 64 bytes
```

RAM summary:

```text
Physical RAM: 64 GiB
OS-usable RAM: about 62 GiB
Modules: 2 x 32 GiB
Type: DDR5 SODIMM
Speed: 5600 MT/s
Manufacturer: Micron
Part number: MTC16C2085S1SC56BD1
Rank: 2
Voltage: 1.1 V
ECC: none
Slots populated: 2 / 2
DMI maximum capacity: 64 GiB
```

Important CPU instruction families visible in the scan:

```text
AVX2
FMA
AVX-VNNI
POPCNT
BMI1 / BMI2
VAES
VPCLMULQDQ
```

There is no AVX-512 in the reported flags. The right baseline is therefore
ordinary scalar C, compiler auto-vectorization, and optional AVX2/VNNI-specific
paths later.

> **Measured (cim-sys-scale-v1, decision A; rule #8 below).** `vec-report-scale`
> on `-O3 -march=native`: the one float reduction on the hot path — the
> `cinm_score` dot loop (`cinm.c:84`) — is **already auto-vectorized** (32-byte
> vectors, unroll 8). The addressing scans (`cinm.c:32`, `:61`) do not vectorize
> (early-exit / branchy control flow) and are memory-bound regardless. The
> `-O2` / `-O3 -march=native` / `-ffast-math` sweep confirms it: `-ffast-math`
> ~halves the float paths (NN, score, merge) by reassociating, but leaves
> `cinm_address` flat. **Conclusion: hand-written SIMD is not justified now** —
> the compiler already covers the only arithmetic-bound loop; the scaling lever
> is an index over the scan, not vectorization. Recorded as a measured negative
> result (doc 18). See `experiments/c-kernel/runs/cim-sys-scale-v1/`.

> **Lever realized (cim-sys-scale-v2, Phase 3a).** The index — not SIMD — was
> built: `cinm_index.c` (an open-addressing hash) added *alongside* the untouched
> linear scan, making `cinm_address` ~O(1) (~47 ns vs 443 µs scanned at 1M) and
> byte-exact with the scan (`scale_index_check.c` green; the scalar scan stays the
> reference, rule #1). Hand SIMD stayed unbuilt, as the negative result predicted.
> The nearest-neighbour scan got the same treatment in Phase 3b (`cinm_nn_index`, a
> static KD-tree, `cim-sys-scale-v3`) — also byte-exact, also libm-free — but the
> lever does *not* transfer: at NFEAT=8 an exact metric tree is backtracking-bound, so
> it only reduces NN cost ~8.3× at 1M (sub-1× below ~2k cells), not the ~O(1) the
> exact-key hash gave. The exact-key/NN asymmetry is now measured. See
> `experiments/c-kernel/runs/cim-sys-scale-v2/` and `.../cim-sys-scale-v3/`.

## Why This Matters For CINM

CINM will repeatedly do a few simple operations over arrays:

```text
address candidate cells
scan memory cells
compute similarity
compute dot products
update local weights
compare candidate policies against a baseline
snapshot and rollback small state
```

These are not exotic operations. They are regular, memory-heavy loops. That is
exactly where C plus a modern CPU can be strong.

For CINM, CPU+RAM is not merely a fallback before CUDA. It is the best first
development substrate because:

- the system is sparse and local
- rollback/snapshot is simple in host memory
- experiments need debuggability and deterministic replay
- memory layout is part of the research question
- HDC has both bit-packed and float-vector forms that map well to CPU kernels
- the machine has enough RAM for large maps, archives, logs, and test vectors

CUDA can still become useful later for batched similarity search or large
offline evaluation. It should not define the first runtime contract.

## Two HDC Lines

There are two useful HDC representations to keep separate.

### Float HDC Line

Use arrays of `float`.

Useful for:

- theory clarity
- local preference weights
- plasticity
- confidence
- margin-scaled updates
- gradient-like pairwise updates
- readable diagnostics

Typical operations:

```text
dot product:       score = sum(w[i] * phi[i])
update:            w[i] += eta * reward * dphi[i]
clamp:             w[i] = clamp(w[i], -W_MAX, W_MAX)
similarity scan:   best = argmax(dot(cell[j].w, phi))
```

CPU mapping:

```text
AVX2 handles 8 float values per 256-bit vector.
FMA fuses multiply + add for dot products.
```

The existing C kernel currently starts here:

```text
cinm_cell.w[NFEAT]
cinm_score()
cinm_update()
```

This is the right clarity-first path.

### Bit-Packed HDC Line

Use arrays of `uint64_t`.

Useful for:

- very high dimensional HDC vectors
- fast binding
- fast similarity
- memory-dense archives
- large prototype stores

Typical operations:

```text
bind:       hv_out[i] = hv_a[i] ^ hv_b[i]
similarity: distance += popcount(hv_a[i] ^ hv_b[i])
bundle:     majority vote, often via counters or bit-sliced accumulators
permute:    rotate or indexed shuffle
```

CPU mapping:

```text
XOR is extremely cheap.
POPCNT counts set bits in hardware.
BMI instructions can help bit manipulation.
```

Minimal C shape:

```c
#include <stdint.h>

int hamming_u64(const uint64_t *restrict a,
                const uint64_t *restrict b,
                int words) {
    int d = 0;
    for (int i = 0; i < words; i++)
        d += __builtin_popcountll(a[i] ^ b[i]);
    return d;
}
```

This is already a strong first implementation. The compiler should lower
`__builtin_popcountll` to the hardware `popcnt` instruction on this CPU when
compiled for the native target.

## Instruction Families

### AVX2

AVX2 provides 256-bit vector operations.

For `float`:

```text
256 bits / 32 bits = 8 floats per vector
```

A scalar loop:

```c
float dot_f32(const float *restrict a,
              const float *restrict b,
              int n) {
    float s = 0.0f;
    for (int i = 0; i < n; i++)
        s += a[i] * b[i];
    return s;
}
```

can become vector work internally:

```text
load 8 floats from a
load 8 floats from b
multiply
accumulate
repeat
horizontal reduce
```

Relevant CINM/HDC operations:

- `cinm_score`
- scanning many cells for best score
- applying local weight updates
- clamp/threshold passes
- evaluating many candidate policies

### FMA

FMA means fused multiply-add.

This expression:

```c
s += a[i] * b[i];
```

can map to:

```text
s = fma(a[i], b[i], s)
```

The CPU performs multiply and add as one fused operation. This matters for
dot-product-heavy float HDC:

```text
score = dot(w, phi)
```

In CINM terms, FMA helps whenever scoring is:

```text
memory contribution = sum(cell weights * candidate features)
```

### POPCNT

POPCNT counts the number of set bits in an integer.

For bit-packed HDC, similarity is often:

```text
diff = a XOR b
distance = popcount(diff)
```

One `uint64_t` word represents 64 dimensions. A 8192-bit hypervector uses:

```text
8192 / 64 = 128 words
```

A full Hamming distance over 8192 dimensions is therefore 128 XORs and 128
popcounts. That is small enough to be extremely fast on a modern CPU.

Minimal similarity:

```c
float similarity_binary(const uint64_t *restrict a,
                        const uint64_t *restrict b,
                        int words) {
    int dist = hamming_u64(a, b, words);
    int bits = words * 64;
    return 1.0f - (2.0f * (float)dist / (float)bits);
}
```

This maps Hamming distance to approximately:

```text
+1.0 = identical
 0.0 = random/unrelated
-1.0 = opposite
```

### AVX-VNNI

AVX-VNNI helps integer dot-product style workloads.

It is relevant if CINM later uses quantized vectors:

```text
int8 weights
int8 features
int32 accumulators
```

This is not the first kernel. It becomes interesting for:

- quantized prototypes
- small neural/HDC hybrids
- dense scoring of many candidates
- compressed preference vectors

First implementation should not depend on VNNI. Keep the data model simple.

### VAES And VPCLMULQDQ

These are less central for first CINM work.

Potential later uses:

- fast deterministic randomization or hashing
- checksums / fingerprints
- cryptographic audit variants
- block-level transforms

They should not affect the first HDC kernels.

## Compiler Strategy

Do not begin with hand-written intrinsics.

Start with clean C loops that a compiler can understand:

- contiguous arrays
- known lengths where possible
- `restrict` for non-aliasing
- simple loop bodies
- no hidden function calls inside hot loops
- no mixed pointer aliasing

Useful compile flags for experiments:

```bash
cc -O3 -march=native -Wall -Wextra -std=c11
```

For vectorization diagnostics:

```bash
cc -O3 -march=native -fopt-info-vec-optimized -fopt-info-vec-missed
```

For float experiments where exact IEEE behavior is not required:

```bash
cc -O3 -march=native -ffast-math
```

Use `-ffast-math` carefully. It can change NaN/Inf handling and reassociation
semantics. For reproducible scientific logs, compare both modes.

Good first rule:

```text
default correctness build: -O2 -Wall -Wextra -std=c11
performance probe build:  -O3 -march=native -fopt-info-vec-optimized
aggressive float probe:   -O3 -march=native -ffast-math
```

## C Loop Patterns

### Float Dot Product

```c
float dot_f32(const float *restrict a,
              const float *restrict b,
              int n) {
    float s = 0.0f;
    for (int i = 0; i < n; i++)
        s += a[i] * b[i];
    return s;
}
```

Notes:

- `restrict` tells the compiler `a` and `b` do not overlap.
- Contiguous `float` arrays help vector loads.
- For best auto-vectorization, keep the loop simple.
- If `n` is fixed and known at compile time, the compiler may unroll more.

### Float Update With Clamp

```c
static inline float clamp_f32(float x, float lo, float hi) {
    return x < lo ? lo : (x > hi ? hi : x);
}

void update_f32(float *restrict w,
                const float *restrict dphi,
                int n,
                float step,
                float wmax) {
    for (int i = 0; i < n; i++) {
        float x = w[i] + step * dphi[i];
        w[i] = clamp_f32(x, -wmax, wmax);
    }
}
```

This maps to CINM:

```text
step = ETA * reward * plasticity
w = clamp(w + step * dphi)
```

### Bit Hamming Distance

```c
#include <stdint.h>

int hamming_u64(const uint64_t *restrict a,
                const uint64_t *restrict b,
                int words) {
    int d = 0;
    for (int i = 0; i < words; i++)
        d += __builtin_popcountll(a[i] ^ b[i]);
    return d;
}
```

This is the core of bit-packed HDC lookup.

### Bit Binding

```c
void bind_xor_u64(uint64_t *restrict out,
                  const uint64_t *restrict a,
                  const uint64_t *restrict b,
                  int words) {
    for (int i = 0; i < words; i++)
        out[i] = a[i] ^ b[i];
}
```

### Bit Permutation By Rotate

```c
static inline uint64_t rotl64(uint64_t x, unsigned r) {
    return (x << r) | (x >> (64u - r));
}

void permute_rot_u64(uint64_t *restrict out,
                     const uint64_t *restrict in,
                     int words,
                     unsigned r) {
    for (int i = 0; i < words; i++)
        out[i] = rotl64(in[i], r);
}
```

This is not a universal HDC permutation, but it is a useful first reversible
operation for experiments.

## Memory Layout Guidance

Memory layout matters more than clever instructions at first.

### For Float Cells

Current conceptual shape:

```c
typedef struct {
    uint32_t key;
    float w[NFEAT];
    float plast;
    float conf;
    uint32_t evidence;
    uint32_t born;
    uint32_t last_touched;
    bool in_use;
} cinm_cell;
```

This is good for readability.

For large scans, a structure-of-arrays layout may become faster:

```text
keys[]
weights[cell][feature]
plasticity[]
confidence[]
evidence[]
```

Tradeoff:

```text
array-of-structs:
  easier to explain and rollback one cell

structure-of-arrays:
  better for scanning many weights/features
```

First C kernel should stay clarity-first. A later benchmark can compare both.

### Alignment

Cache line size is 64 bytes.

Useful alignment ideas:

```text
align large vectors to 32 or 64 bytes
keep hot arrays contiguous
avoid scattered heap allocation per cell
batch scans over contiguous memory
```

Possible C allocation later:

```c
void *p = NULL;
if (posix_memalign(&p, 64, bytes) != 0)
    p = NULL;
```

Do not use this everywhere immediately. Use it only when benchmarks show a
layout-sensitive hot path.

## CINM Hot Paths

Expected hot paths:

```text
1. score one candidate against active cells
2. find top-k similar cells
3. update touched cells
4. evaluate candidate adaptation over held-out trials
5. snapshot/restore map state
6. archive search by niche or score
```

Likely CPU bottlenecks:

```text
large memory scans
cache misses
branchy cell filtering
dot products over many cells
popcount scans over many hypervectors
copy cost for whole-map rollback
```

For the first kernel, the biggest win is not intrinsics. It is making the data
easy to scan and replay.

## Rollback And SIMD

Rollback interacts with memory layout.

Whole-map snapshot:

```text
simple
safe
debuggable
probably fine for MAX_CELLS=256
```

Delta log:

```text
faster for large maps
more complex
requires exact touched-cell tracking
```

For early C:

```text
copy whole cinm_map
run candidate
commit or restore
```

For later C:

```text
log touched cell indices
copy old cell states
restore only touched cells
```

SIMD does not change the rule. It only makes scoring and updates faster. The
transaction boundary must remain visible and testable.

## Suggested Build Targets

The current `experiments/c-kernel/Makefile` can eventually grow separate
targets:

```make
CFLAGS_DEBUG = -O0 -g -Wall -Wextra -std=c11
CFLAGS_BASE  = -O2 -Wall -Wextra -std=c11
CFLAGS_NATIVE = -O3 -march=native -Wall -Wextra -std=c11
CFLAGS_VEC_REPORT = -O3 -march=native -fopt-info-vec-optimized -fopt-info-vec-missed -Wall -Wextra -std=c11
```

Potential commands:

```bash
make run
make run-native
make vec-report
make bench
```

Do not add this until the minimal update rule and rollback shape exist.

## Verification Commands For C Kernels

Check vectorization:

```bash
cc -O3 -march=native -fopt-info-vec-optimized -c cinm.c
```

Inspect generated assembly:

```bash
cc -O3 -march=native -S cinm.c
```

Look for useful instructions:

```bash
rg "vfmadd|vmulps|vaddps|vmaxps|vminps|popcnt|vpopcnt|vpdp" cinm.s
```

For bit kernels:

```bash
cc -O3 -march=native -S hdc_bits.c
rg "popcnt|xor" hdc_bits.s
```

For timing:

```bash
perf stat ./preference
```

If `perf` requires permissions, use simple internal timing first.

## C Design Rules For CINM

1. Keep the scalar reference path.
2. Add SIMD only after scalar behavior is fixed and tested.
3. Use `restrict` on hot pointer arguments when aliasing is not allowed.
4. Keep hot loops simple enough for auto-vectorization.
5. Prefer contiguous arrays over pointer-heavy structures.
6. Keep rollback boundaries visible.
7. Do not let performance flags change experiment meaning silently.
8. Compare `-O2`, `-O3 -march=native`, and `-O3 -march=native -ffast-math`.
9. Treat bit-HDC and float-HDC as separate backends with shared tests.
10. Benchmark similarity search and rollback separately.

## First Practical Plan

For the current C kernel:

```text
Phase 1:
  implement bounded pairwise float update
  keep scalar C only
  confirm learning rises above chance

Phase 2:
  add whole-map snapshot/restore
  add candidate adaptation transaction wrapper
  keep scalar C only

Phase 3:
  add tiny benchmark timing for score/update/rollback
  compile with -O2 and -O3 -march=native
  inspect compiler vectorization reports

Phase 4:
  add bit-packed HDC experiment file
  implement XOR bind and POPCNT similarity
  compare float scoring vs bit similarity

Phase 5:
  decide whether hand-written AVX2 intrinsics are justified
```

The likely answer for a while will be: not yet. Clean scalar C plus
`-O3 -march=native` should be enough until the data layout and experiment shape
stabilize.

## Why This Fits The Current Project Direction

The current project term is:

```text
interactive continual preference learning
  with reversible self-adaptation
```

This means the C kernel needs to be:

- immediate
- local
- inspectable
- rollback-friendly
- memory-layout-conscious
- cheap enough to run many small candidate adaptations

CPU SIMD helps exactly there.

The machine has enough RAM for large local memory maps and archive experiments.
The CPU has enough vector capability for float scoring and bit-packed HDC. This
makes CPU+RAM the primary experimental substrate, with CUDA left as an optional
batch accelerator rather than the conceptual core.

