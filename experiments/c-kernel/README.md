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
  plus strict logical replay (sequence, type, capacity, bounded range).
- `cinm_ledger.h` / `cinm_ledger.c` - in-RAM decision ledger: append-only receipts
  recording why the map changed (commit/rollback/consolidation) — D018, R2.
- `cinm_undo.h` / `cinm_undo.c` - bounded undo window: snapshot ring + log replay
  for "undo last N" within a retention horizon — D018, R4.
- `replay_check.c` - recovery proof harness: snapshot + within-epoch replay, and the
  epoch as explicit map state.
- `log_invariants_check.c` - replay invariant guards: sequence gap, duplicate
  sequence, unsupported type, and capacity overflow.
- `ledger_check.c` - decision-ledger gate: append-only, ordered, explains forgetting.
- `consolidate_check.c` - lossy-consolidation gate: evict/freeze/epoch, revival cost,
  and full-replay-diverges vs within-epoch-matches.
- `undo_check.c` - bounded-undo gate: within-horizon byte-exact, beyond-horizon and
  across-consolidation refused.
- `cinm_selfadapt.h` / `cinm_selfadapt.c` - operator self-adaptation archive
  (MAP-Elites shape): value-bucket niches, elite-per-niche, discarded lineage — vertical P.
- `taste_check.c` - drum-shaped taste-loop gate (vertical O): context-addressing beats a
  context-blind baseline, old-context retention, byte-exact undo.
- `selfadapt_check.c` - Godel-Darwin self-adaptation gate (vertical P): self-tunes the decay
  rate on a drifting taste, held-out anti-hack, ledger receipts, reversible.
- `cinm_drum_adapter.h` / `cinm_drum_adapter.c` - the drum-aware ingestion seam (vertical L+M):
  flat `drum_events` (real aig_ranker schema) -> cinm_events, train/slate split, dedup by
  record_id, winner+confidence -> reward/dphi. Library-free line reader.
- `drum_adapter_check.c` / `drum_features_check.c` - gates: golden cinm_event sequence, phase
  split, dedup, no drum-symbol leak (L); phi/dphi determinism + sparse activation (M).
- `testdata/drum_events.sample` - committed real-schema fixture for the adapter gates.
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
make run-ledger   # decision-ledger append-only receipts (D018)
make run-consolidate # lossy consolidation: evict/freeze, revival cost, lossy vs within-epoch (D018)
make run-undo     # bounded undo: within-horizon exact, beyond/across-epoch refused (D018)
make run-taste-loop  # drum taste loop: context-addressing beats blind baseline (vertical O)
make run-self-adapt  # Godel-Darwin: self-tunes decay on drift, held-out gate (vertical P)
make run-drum-adapter   # real-schema drum_events -> cinm_events: golden vector, phase, dedup (L)
make run-drum-features  # phi/dphi determinism + sparse activation (M)
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
snapshot+replay recovers state: PASS (base_seq=2001)
within-epoch replay recovers state: PASS (epoch=0 base_seq=0)
epoch advance is observable: PASS
```

`make run-ledger`, `make run-consolidate`, and `make run-undo` exercise the D018
state-primacy surface and should report PASS on every line:

```text
ledger append-only and ordered: PASS
consolidation receipt explains the forgetting: PASS
evict/freeze/count/epoch: PASS (evicted=2 promoted=1 count 4->2 epoch=1)
revival to >= 85% of pre-eviction within K=100: PASS
lossy + within-epoch replay: PASS (full replay diverges=yes, within-epoch matches=yes)
undo within horizon is byte-exact: PASS
undo beyond horizon refused: PASS
undo across consolidation refused: PASS
```

`make run-taste-loop` and `make run-self-adapt` exercise the drum vertical (O, P) on
synthetic drum-shaped data and should report PASS on every line:

```text
context-addressed taste beats blind baseline: PASS (cim=0.920 base=0.565)
old-context retention after a new context: PASS (cim 0.905->0.910, base 0.912->0.597)
undo N taste picks is byte-exact (R4): PASS
self-adaptation beats no-forgetting baseline: PASS (best_decay=0.930 best=0.871 base=0.779)
every proposal leaves a ledger receipt: PASS (commits=17 rollbacks=31 len=48)
held-out rejects extreme operators: PASS
rejected operator is byte-exactly reversible: PASS
```

`make run-drum-adapter` and `make run-drum-features` ingest the real-schema fixture
(`testdata/drum_events.sample`) and should report PASS on every line:

```text
golden cinm_event sequence: PASS (train=6 slate=3 dup=1 tie/neither=2)
train/slate phase split + per-set seq: PASS
duplicate record_id not double-applied: PASS
kernel sees only neutral cinm_events: PASS
phi/dphi deterministic across runs: PASS
sparse activation (cells=3 << MAX_CELLS=256): PASS
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

The layout-independent event is evidence, replayable within an epoch:

```text
{ seq, type, key, reward, dphi[NFEAT] }
```

The live map is the primary state (D018); the event log is evidence + a
within-epoch undo/recovery trail, not a source of truth the map is rebuilt from.
Recovery is:

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
live map         = primary state (D018)
in-RAM event log = evidence + within-epoch replay (recovery, not source of truth)
decision ledger  = append-only receipts of why the map changed (audit)
snapshot         = cheap in-RAM map copy
rollback         = cheap in-memory reversibility
consolidation    = lossy forgetting past the epoch boundary; undo is windowed
```

Durable cross-restart persistence (append-only SSD log, snapshots, manifests,
mmap HDC banks, io_uring/SPDK) is deferred — parked under `docs/future/` as a
later hardware mapping, not part of the current RAM-only scope (decision D013).
