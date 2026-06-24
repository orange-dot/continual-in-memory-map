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
- `drum_e2e_check.c` - gate run-drum-e2e (L->M->O end to end): the file -> adapter -> taste-loop
  seam composes deterministically on the committed fixture, and context-addressed taste beats a
  context-blind baseline on a held-out slate at scale.
- `drum_gate_check.c` - gate run-drum-gate (vertical Q, M1 decision gate): hard gates (bounds,
  capacity, rollback, anti-hack) + GOLDEN_SET held-out (ideal>bad, catch trials, revision flips)
  + ANTI_PATTERNS critic; writes a doc-18 named-run bundle under `runs/cim-drum-m1-gate/`.
- `cinm_groove.h` / `cinm_groove.c` - MAP-Elites groove archive (vertical N, M2): a 2-D behavior
  grid over a phi genome, one elite per niche, parent-niche lineage, a quarantine ring of rejects.
  Neutral — the caller supplies the behavior descriptor and the taste fitness; the archive never
  calls the kernel (lifts the `cinm_selfadapt` shape from operator space to groove space).
- `groove_check.c` - gate run-groove-archive (vertical N): the M1 taste model is the selection
  pressure; niches stay populated (no collapse), population is bounded, selection beats the random
  seeds, lineage is reconstructable, rejects are quarantined with a reason, run is deterministic.
- `cinm_drum_render.h` / `cinm_drum_render.c` - the drum-aware render seam (vertical Q, M2): the
  only place a groove's phi is lowered to drum voices. phi -> type-0 Standard MIDI File on the GM
  drum channel + a `*.midi-loss-map` sidecar for the lossy continuous->grid lowering. No runtime
  authority — bytes/files only (doc 19).
- `drum_render_check.c` - gate run-drum-render (vertical Q, M2 render): renders the fittest
  taste-adapted elite + a generic baseline; re-parses the SMF, checks byte-determinism and loss-map
  presence, and proves the adapted groove out-scores the baseline under the taste model (margin
  proxy; human blind-A/B deferred D017). Writes a doc-18 bundle under `runs/cim-drum-m2-render/`.
- `testdata/drum_events.sample` - committed real-schema fixture for the adapter gates.
- `testdata/drum_events.learn` - committed synthetic-in-real-schema scale fixture for run-drum-e2e.
- `testdata/golden_set.sample` - committed transcription (snapshot) of the drummer GOLDEN_SET /
  ANTI_PATTERNS authority docs into the gesture-phi schema, for run-drum-gate.
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
make run-drum-e2e       # L->M->O end to end: seam composes + learns on a held-out slate
make run-drum-gate      # M1 decision gate: hard gates + GOLDEN_SET held-out + ANTI_PATTERNS (Q)
make run-groove-archive # N MAP-Elites groove generator: taste-driven, no niche collapse (M2)
make run-drum-render    # Q render: phi -> real GM-drum MIDI + loss map, taste-margin proxy (M2)
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

`make run-drum-e2e` runs the L->M->O seam on real-schema files (`drum_events.sample`
smoke + `drum_events.learn` scale) and should report PASS on every line:

```text
L->M->O seam composes (deterministic, no drum-symbol leak): PASS (train=6 slate=3 cells=3)
real-schema taste learns through the seam: PASS (cim=0.854 base=0.625)
sparse context activation (cells=4 == contexts=4): PASS
```

`make run-drum-gate` (vertical Q) trains on the transcribed `golden_set.sample`
bootstrap and decides on held-out phase-1; it should ACCEPT and write a doc-18
named-run bundle under `runs/cim-drum-m1-gate/`:

```text
hard gates (bounds/capacity/rollback/anti-hack): PASS
GOLDEN_SET held-out (8/8 ideal>bad, 2/2 revision flips, catch tie/ok): PASS
ANTI_PATTERNS critic (4 checked, 2 deferred-M2): PASS
M1 decision: ACCEPT
run bundle (runs/cim-drum-m1-gate): config.toml events.jsonl metrics.json decisions.jsonl summary.md
```

The two deferred anti-patterns (counterproposal-theater, source-imitation) need
groove generation/dialogue and bind at M2 (step N); the bundle's `summary.md` lists
CHECKED vs DEFERRED so coverage is never silently overstated. The GOLDEN_SET /
ANTI_PATTERNS fixtures are a transcription snapshot of
`pc4-microkit-studio/crates/drum-engine/authority/docs/{eval,goldens}/` and may
drift from that source. Runs under `runs/` are local evidence and gitignored
(doc 18).

`make run-groove-archive` (vertical N) evolves a groove archive with the M1 taste
model as the selection pressure and should report PASS on every line:

```text
niches stay populated (no collapse): PASS (filled=36/36, need>=32)
population bounded by the niche grid: PASS (tried=6064 discarded=5572 filled<=36)
selection beats the random seeds: PASS (elite=0.410 seed=-0.264)
lineage reconstructable: PASS (bred=36, best niche parents=24/1 fit=0.536)
rejects quarantined with a reason: PASS (retained=64 dominated=64 out-of-range=yes)
groove run is deterministic: PASS
```

`make run-drum-render` (vertical Q, M2 render) renders the fittest taste-adapted
elite to a real type-0 SMF on the GM drum channel and should ACCEPT, writing a
doc-18 bundle under `runs/cim-drum-m2-render/`:

```text
type-0 SMF container is valid (re-parsed): PASS (bytes=49 notes=2)
render is byte-deterministic (same phi -> same .mid): PASS
each .mid ships a non-empty *.midi-loss-map: PASS (adapted=648B baseline=649B)
taste shows up in the render (margin proxy, blind-A/B deferred D017): PASS (adapted=0.536 base=-0.264 margin=0.800)
doc-18 bundle written under runs/cim-drum-m2-render/: PASS
render decision: ACCEPT
```

The phi -> drum lowering (`cim.drum_gm_grid.v1`: four GM voices, a density + velocity
gene each, a kick/snare backbone floor) is a modeling act and may drift; the gate
proves the pipeline and a taste-margin proxy, not musical quality (doc 16). Human
blind-A/B, real ADG -> aig-pc4-midi export, and sample-backend audition stay deferred
to D017 (doc 21). Both `.mid` files play in any GM-compatible player/DAW; runs under
`runs/` are gitignored.

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
