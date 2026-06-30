# CINM C Kernel - First Exploration Substrate

Pure C, no external libraries. This directory is the first executable substrate
for D011/D012: a small continual preference-learning map whose core is an
adaptive cell — margin, a margin/confidence-gated bounded update, plasticity, and
decay. An in-RAM replayable event log rides alongside as a sidecar evidence lane.
Strictly in-memory — the hot path lives in CPU cache and RAM, and there is no
disk tier (decision D013).

## Files

- `cinm.h` / `cinm.c` - SoA synaptic map and adaptive cell core: exact-key and
  continuous nearest-neighbour/prototype addressing (D019), score, margin, bounded
  pairwise and margin/confidence-gated adaptive update, decay, explain, whole-map
  snapshot/restore, speculative transaction, lossy consolidation (evict/freeze +
  R3.5 prototype merge), and member-wise equality.
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
- `nn_address_check.c` - nearest-neighbour addressing + R3.5 merge gate (D019, neutral
  doc-03 contextual-preference task): determinism, no over-fragmentation (NN folds
  jittered contexts into a few prototypes where exact keys fragment to MAX_CELLS),
  novelty-as-radius, radius-0 reproduces exact-key learning, merge folds duplicates
  faithfully + lossily + deterministically, bounds.
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
- `cinm_index.h` / `cinm_index.c` - Phase 3a exact-key acceleration index: an
  open-addressing hash mapping key -> cell, built over a map's live cells. Auxiliary
  (heap, like `cinm_log`/`cinm_ledger`); the kernel `cinm.c` is untouched and the
  linear scan stays the byte-exact reference. Find is ~O(1); lowest index wins on a
  duplicate key, matching `cinm_address` (`cinm.c:33`).
- `scale_index_check.c` - gate run-scale-index-check: proves the index selects the
  identical cell as the scan (hit-equivalence, absent-key miss, duplicate-key
  tie-break, rebuild-after-evict), at `MAX_CELLS` 256 and 1M.
- `scale_index_bench.c` - run-scale-index-bench: index `find` vs scan `cinm_address`
  across the doc-12 regimes to 1M; reports speedup and the O(count) build cost
  separately. Writes nothing; see the hand-authored `runs/cim-sys-scale-v2/` bundle.
- `cinm_nn_index.h` / `cinm_nn_index.c` - Phase 3b NN (prototype) acceleration index: a
  static KD-tree over live cells' `proto[NFEAT]`, built once. Auxiliary (heap), kernel
  untouched, byte-exact reference. Find reproduces `cinm_address_nn`'s cell — minimum
  squared-L2 within radius2, HIGHEST index on a distance tie (`cinm.c:64`, the opposite
  of exact-key's first-match). Libm-free; the per-axis splitting-plane prune is a
  provably safe float lower bound (no epsilon slack needed).
- `scale_nn_index_check.c` - gate run-scale-nn-index-check: proves the KD-tree selects
  the identical cell as the scan (hit-equivalence, beyond-radius miss, distance-tie
  highest index, exact-radius boundary, rebuild-after-evict), at `MAX_CELLS` 256 and 100k.
- `scale_nn_index_bench.c` - run-scale-nn-index-bench: KD `find` vs scan `cinm_address_nn`
  across the doc-12 regimes to 1M; reports speedup and the O(count·log count) build cost
  separately. Writes nothing; see the hand-authored `runs/cim-sys-scale-v3/` bundle.
- `split_check.c` - gate run-split (opt-in `-DCINM_ENABLE_SPLIT`): proves D019 cell-splitting,
  the inverse of merge. When one NN address carries two address-separable schemas with
  opposite preferences a single cell under-fits; the deferred split pass (Pass 0.5) forks the
  dissenting sub-population into a child and the pair recovers win-rate. 7 assertions incl. the
  aliased *negative control* (split correctly declines) and the `split_locked` anti-oscillation.
- `split_forget_probe.c` - run-split-forget (opt-in `-DCINM_ENABLE_SPLIT`): the doc-18
  forgetting probe (learn A → interleave B → revive under A+B), split OFF vs ON. A measurement,
  not a gate. See the hand-authored `runs/cim-learnq-split-v1/` bundle.

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
make run-nn-address  # NN/prototype addressing + R3.5 merge: clustering, novelty, faithful merge (D019)
make run-split        # D019 cell-splitting (inverse of merge): under-fit recovery, separability null (-DCINM_ENABLE_SPLIT)
make run-split-forget # doc-18 forgetting probe: split OFF vs ON protects an old taste (-DCINM_ENABLE_SPLIT)
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
make run-scale       # A: scaling sweep of the real kernel to 1M cells + tail percentiles
make run-scale-native # ... at -O3 -march=native
make run-scale-fast   # ... at -O3 -march=native -ffast-math (doc-11 rule #8 sweep)
make vec-report-scale # auto-vectorization report for the scale build
make run-scale-index-check # Phase 3a: exact-key index == scan, byte-exact (incl. dup-key tie-break)
make run-scale-index-bench # Phase 3a: index find vs scan address to 1M (the speedup)
make run-scale-nn-index-check # Phase 3b: NN KD-tree == scan, byte-exact (incl. distance-tie highest index)
make run-scale-nn-index-bench # Phase 3b: KD find vs scan cinm_address_nn to 1M (the modest speedup)
```

Expected results:

- `run-scale-index-check` prints four `PASS` lines and exits 0 — the `cinm_index`
  hash selects the identical cell as `cinm_address` (lowest index on a duplicate
  key, `cinm.c:33`), so `cinm_equal` stays byte-exact. The linear scan in `cinm.c`
  is untouched; the index is a separate module (`cinm_index.c`).
- `run-scale-index-bench` shows the address curve flattening: linear scan
  ~130 ns → ~443 µs from 256 → 1M cells, vs index `find` ~6 → ~47 ns (≈ 9 000× at
  1M); `index_build_ns_per_cell` is the O(count) build cost, reported separately.
  See `runs/cim-sys-scale-v2/`.
- `run-scale-nn-index-check` prints five `PASS` lines and exits 0 — the `cinm_nn_index`
  KD-tree selects the identical cell as `cinm_address_nn`, the distance-tie (highest
  index) and the inclusive radius boundary included, so `cinm_equal` stays byte-exact.
- `run-scale-nn-index-bench` shows a *modest, growing* speedup, not a flatten: NN scan
  ~1.2 µs → ~6.2 ms from 256 → 1M, vs KD `find` ~2.9 µs → ~0.74 ms (sub-1× below ~2k
  cells, ~8.3× at 1M). At NFEAT=8 an exact metric tree is backtracking-bound — the
  exact-key hash's ~O(1) does not transfer. `nn_index_build_ns_per_cell` (the
  O(count·log count) build, ~9× the hash's) is reported separately. See
  `runs/cim-sys-scale-v3/`.
- `run-split` prints the conflict-rate + `dir2` diagnostics and seven `PASS` lines, exits 0.
  A single torn cell sits at chance (`wr_off=0.505`); splitting recovers it to `0.960`
  (1.90×). The aliased fixture is *suppressed* (`split_suppressed=1`, stays 0.480) — splitting
  declines when the sub-populations share an address. Opt-in `-DCINM_ENABLE_SPLIT`; the default
  build is byte-exact (`sizeof(cinm_map)` unchanged, the full default gate suite green).
- `run-split-forget` prints the OFF-vs-ON table: taste A retained `0.322` (split off, forgotten)
  vs `0.960` (split on) under continued interference, revival `1200` (never) vs `100` events.
  *Immediate* retention is equal (`0.760`) — splitting isolates the interferer, it does not
  restore. See `runs/cim-learnq-split-v1/`.
- **The 1/3 rate threshold is empirical.** Symmetric opposite schemas (`wtB = −wtA`) sit at
  ~1/2 contradiction (`winrate_B = 1 − winrate_A`), so a 1/2 gate sits on the structural ceiling
  and never reliably fires (measured 0.490) — `cinm.c` uses `3·conflict ≥ evidence`.

All targets build with:

```sh
cc -O2 -Wall -Wextra -Wpedantic -std=c23
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

`make run-nn-address` exercises continuous NN/prototype addressing and R3.5 merge
consolidation (D019) on a neutral contextual-preference task (contexts that repeat
with variation, doc 03) and should report PASS on every line:

```text
NN addressing deterministic: PASS
clusters, no over-fragmentation: PASS (nn cells=4==C, exact-key cells=256)
novelty is a radius decision (allocate vs reuse): PASS
radius-0 NN reproduces exact-key learning: PASS
merge folds near-duplicate prototypes (faithful): PASS (merged=1 count 2->1 wr 0.910->0.957)
merge is lossy + deterministic: PASS
bounds (count<=MAX_CELLS, protos finite): PASS
```

The headline contrast — 4 prototype cells under NN vs MAX_CELLS=256 under exact keys
on the same jittered stream — is the schema compression exact-symbolic keys cannot do
(doc 22, R3.5); the kernel stays libm-free (squared-L2 distance, no `sqrt`). The
novelty radius is the open tuning knob (doc 06); the doc-03 "addressing dominates"
line is the pre-registered negative-result watch.

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

`make run-scale` (decision A — systems/scale) sweeps the real kernel from 256 to
1,048,576 cells on a heap-allocated map (`-DCINM_MAX_CELLS`), emitting one
machine-parseable line per size plus a crossover line. It is a measurement, not a
PASS/FAIL gate. The headline on the reference host (i7-4600U) is:

```text
crossover_n=256          # retrieval dominates from the smallest map onward
n=1048576 address_mean_ns=403514 address_p50=823391 address_p99=4269369 ...
          nn_mean_ns=9287754 ... score_ns_per_cell=5.794
```

Findings (full bundle in `runs/cim-sys-scale-v1/`, gitignored):

- **Addressing is the scaling wall.** `cinm_address` is O(count) linear scan,
  memory-bound; `cinm_address_nn` ~8× worse (O(count·NFEAT)). `score_ns_per_cell`
  stays flat ~4–6 ns across all cache regimes — arithmetic is never the bottleneck.
- **`crossover_n=256`** confirms the doc-18 pre-registered watch "addressing
  dominates every other design choice."
- **Compiler flags don't move the bottleneck:** `-ffast-math` ~halves the float
  paths (NN/score/merge) but `cinm_address` is flat across `-O2`/native/fast.
- **Auto-vec already covers the one float reduction** (`cinm_score` dot loop,
  `cinm.c:84`); hand SIMD is not justified. The justified lever is an index over
  the scan (the conditional NN-index follow-on).

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
addressing       = exact-symbolic keys or continuous NN/prototype (D019)
in-RAM event log = evidence + within-epoch replay (recovery, not source of truth)
decision ledger  = append-only receipts of why the map changed (audit)
snapshot         = cheap in-RAM map copy
rollback         = cheap in-memory reversibility
consolidation    = lossy forgetting past the epoch boundary (evict/freeze + R3.5 prototype merge); undo is windowed
```

Durable cross-restart persistence (append-only SSD log, snapshots, manifests,
mmap HDC banks, io_uring/SPDK) is deferred — parked under `docs/future/` as a
later hardware mapping, not part of the current RAM-only scope (decision D013).
