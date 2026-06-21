# 13. Implementation Backlog G-K

This document records the next implementation set after the first C kernel
became executable and replayable. It is still research infrastructure, not a
production claim.

## G. Evidence-Grade Kernel

Goal: make the in-RAM event log a small evidence machine, not just a happy-path
demo. (Per D013 this is RAM-only; the disk-grade variant — record framing,
checksums, fdatasync, manifests — is deferred, see `docs/future/`.)

Implemented surface:

- in-RAM append-only event log (`cinm_log`): growable `cinm_event` array
- layout-independent event record `{seq, type, key, reward, dphi[NFEAT]}`
- checked replay report: applied count, last seq, bad sequence, unsupported
  type, and capacity overflow
- strict replay: sequence gaps, duplicate sequences, unsupported event types, and
  full-map drops fail recovery instead of silently producing a partial map
- "snapshot" = in-RAM `cinm_snapshot` (whole-map copy) + the log position
- recovery gate compares full semantic map state, including logical clock and
  evidence/bookkeeping, via `cinm_equal`

Invariant:

```text
in-RAM event log = replayable truth (within a process)
snapshot = cheap in-RAM map copy
```

Gate:

```sh
make run-log-invariants
make run-replay
```

## H. Learning-Rule Experiments

Goal: keep the core kernel scalar and simple while testing learning rules in
separate harnesses.

Implemented experiments:

- constant pairwise update
- sigma(-margin) update
- confidence/plasticity dampening
- negative reward reversal
- decay retention
- conflict/quarantine signal

Invariant:

```text
learning-rule experiments must not change the meaning of cinm_update
```

Gate:

```sh
make run-learning
```

## I. Memory Hierarchy Timing

Goal: start measuring the layered memory model from doc 12 (cache + RAM only).

Implemented surface:

- timing lines for address scan, score, update, snapshot/restore, in-RAM replay

Deferred (D013, `docs/future/`): the SSD HDC bank surface
(`cinm_store_map_hdc_bank` / `unmap` / `prefetch_range` / `drop_range`, bank-name
validation) — a future read-mostly external-memory mapping, not in current scope.

Invariant:

```text
RAM map is the active projection
the event log is the source of truth (in RAM)
```

Gate:

```sh
make run-memory
```

## J. SIMD/HDC Lane

Goal: add a portable HDC baseline and expose compiler vectorization without
handwritten intrinsics.

Implemented surface:

- 1024-bit HDC vectors as `uint64_t[16]`
- XOR bind
- scalar popcount distance
- compiler builtin popcount distance
- native build target
- compiler vectorization report target

Invariant:

```text
optimized HDC path must exactly match scalar HDC path
native build must not change task behavior
```

Gate:

```sh
make run-hdc
make native
make vec-report
```

## K. Documentation And Claim Boundaries

Goal: keep the project honest as it gets more capable.

Claim boundaries:

- this is a strictly in-memory PoC (no durable store; SSD deferred, D013)
- this is not an application or music-domain implementation
- this does not claim biological fidelity
- this does not claim to replace large-scale backpropagation
- CUDA remains outside this implementation set

Full local gate:

```sh
make run
make run-cmp
make run-learning
make run-txn
make run-replay
make run-log-invariants
make run-memory
make run-hdc
make native
make vec-report
make run-bench
```

The important conceptual result is narrower than the code volume:

```text
interactive continual preference learning
  with reversible self-adaptation
  over a replayable in-memory substrate
```
