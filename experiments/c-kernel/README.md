# CINM C Kernel - First Exploration Substrate

Pure C, no external libraries. This directory is the first executable substrate
for D011/D012: a small continual preference-learning map whose core is an
adaptive cell — margin, a margin/confidence-gated bounded update, plasticity, and
decay. An in-RAM replayable event log rides alongside as a sidecar evidence lane.
Strictly in-memory — the hot path lives in CPU cache and RAM, and there is no
disk tier (decision D013).

## Files

- `cinm.h` / `cinm.c` - SoA synaptic map and adaptive cell core: address, score,
  margin, bounded pairwise and margin/confidence-gated adaptive update, decay,
  explain, whole-map snapshot/restore, speculative transaction, and member-wise
  equality.
- `core_check.c` - core adaptive-cell gates: clamp pressure, margin direction,
  maturity, conflict, and decay retention.
- `task_preference.c` - contextual preference-memory toy task.
- `cmp_update.c` - A/B comparison between constant-step update and
  sigma(-margin) update.
- `learning_experiment.c` - learning-rule harness for sigma, confidence,
  negative reward, decay, and conflict signals.
- `txn_check.c` - byte-exact rollback check for rejected in-memory adaptation.
- `cinm_log.h` / `cinm_log.c` - in-RAM event log: append-only learning events
  plus strict logical replay (sequence, type, capacity).
- `replay_check.c` - recovery proof harness: in-RAM snapshot + event replay, and
  event-log-only replay.
- `log_invariants_check.c` - replay invariant guards: sequence gap, duplicate
  sequence, unsupported type, and capacity overflow.
- `memory_bench.c` - timing lines for address, score, update, snapshot/restore,
  and in-RAM replay.
- `hdc_bits.c` - 1024-bit HDC XOR + popcount baseline.
- `bench_crossbar.c` - preserved sparse-vs-dense microbenchmark.

## Build And Run

```sh
make run-core-check     # core adaptive-cell gates (clamp, margin, maturity, conflict, decay)
make run          # contextual preference task
make run-cmp      # constant-step vs sigma(-margin) update
make run-learning # learning-rule experiment set
make run-txn      # reversible in-memory transaction check
make run-replay   # in-RAM snapshot + event replay recovery check
make run-log-invariants # event-log replay invariant guards
make run-memory   # address/score/update/snapshot/replay timing lines
make run-hdc      # bit-HDC XOR + popcount agreement
make native       # -O3 -march=native behavior check
make vec-report   # compiler vectorization report for cinm.c
make run-bench    # sparse vs dense crossbar microbenchmark
```

All targets build with:

```sh
cc -O2 -Wall -Wextra -Wpedantic -std=c11
```

The core kernel — including the adaptive update — remains libm-free; only the
comparison/experiment harnesses `cmp_update.c` and `learning_experiment.c` link
`-lm` for `expf` and `sqrt`.

## Current Expected Results

`make run` should learn well above chance on all contexts. The current
deterministic run reaches rolling win-rate around 0.86-0.93 and per-context
evaluation around 0.82-0.90.

`make run-cmp` should show that sigma(-margin) keeps the update alive while
reducing late churn and clamp pressure. Current reference output:

```text
win ctx0                    0.883      0.986
win ctx1                    0.873      0.964
win ctx2                    0.874      0.988
steady |dw| (last 20%)     0.0898     0.0170
mean |w|                    3.677      2.462
max  |w|                    4.000      4.000
clamp fraction              0.583      0.167
```

`make run-learning` should report sigma and confidence variants outperforming
constant-step clamp behavior, plus PASS lines for negative reward, decay
retention, and conflict signal.

`make run-txn` should report:

```text
committed=false  identical-after-rollback=yes -> PASS
```

`make run-replay` should report:

```text
snapshot+replay recovers state: PASS
event-log alone recovers state: PASS
```

`make run-log-invariants` should report PASS for clean replay, sequence gap
rejection, duplicate sequence rejection, unsupported type rejection, and
capacity overflow rejection.

`make run-hdc` should report PASS.

## Core Cell

The core is one adaptive cell per context (an index into the SoA map):

- `cinm_margin` / `cinm_score` — alignment of a cell's weights with a direction.
- `cinm_update_pairwise` — the bounded constant step (legacy behavior), now also
  reporting margins, clamp count, and a conflict flag; `cinm_update` wraps it.
- `cinm_update_adaptive` — scales the step by margin alignment and cell maturity,
  raises confidence on agreement and lowers it on conflict, and derives
  plasticity from confidence (floored). Libm-free: the gate is only comparisons.
- `cinm_decay` — multiplies in-use weights by a factor in [0,1]; provenance intact.

`make run-core-check` asserts the relationships: adaptive clamps less than
pairwise, negative reward lowers the margin, mature cells move less than fresh,
conflict fires on a reversed stream, and decay keeps a strong preference. The
abstract proof lane mirrors these (see `../../docs/FORMAL_PROOF.md`).

## Event Log Model (sidecar)

The replayable source of truth is the layout-independent event:

```text
{ seq, type, key, reward, dphi[NFEAT] }
```

The map in RAM is a derivative working projection. Recovery is:

```text
take an in-RAM snapshot (whole-map copy) + remember the log position
replay events at or after the snapshot base sequence
```

The event log is an in-RAM append-only array (`cinm_log`). Replay is strict on
the invariants that still matter without a disk: sequence gaps, duplicate
sequences, unsupported event types, and capacity overflow are hard failures.
There is no on-disk format, checksum, or durability layer.

`cinm_init` zeroes the whole map so unused cells are deterministic and replay is
reproducible on the current build.

## Boundaries

This is a strictly in-memory PoC. It proves the local research invariant:

```text
in-RAM event log = replayable truth (within a session)
snapshot         = cheap in-RAM map copy
rollback         = cheap in-memory reversibility
```

Durable cross-restart persistence (append-only SSD log, snapshots, manifests,
mmap HDC banks, io_uring/SPDK) is deferred — parked under `docs/future/` as a
later hardware mapping, not part of the current RAM-only scope (decision D013).
