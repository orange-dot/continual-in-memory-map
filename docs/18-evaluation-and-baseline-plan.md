# 18. Evaluation And Baseline Plan

This document defines how the milestones (doc 16) are measured. It instantiates the
domain-neutral research program (doc 03) for the drum vertical and follows the
lab's evidence discipline (named runs, explicit baselines, held-out evaluation,
artifacts under `runs/`).

The governing rule from doc 03 stands: prefer honest negative results over
optimistic naming. A drum vertical that loses to a simple baseline is a result, not
a failure to hide.

## Baseline And Bootstrap: E-GMD

The E-GMD / GMD drum corpus (Magenta) is the baseline and the cold-start source. It
is reachable through the existing tooling:

```text
workspace/systems/aig-engine/tools/aig_egmd_corpus.py     metadata + MIDI helper
workspace/systems/aig-engine/tools/aig_external_drum_corpus.py   CC0 audition material
```

Roles E-GMD plays:

```text
baseline      a "generic drummer" the taste model must beat
bootstrap     seed grooves for the MAP-Elites initial population (M2)
audition      real sample material for the M2 render path
```

The baseline is a *simple online learner* on the same feedback stream, not a large
neural model (doc 03, doc 09). Beating a transformer is not the claim; beating a
fair simple baseline is.

## Cold-Start To Warm-Start Arc

The ranker already supports the whole arc; the vertical reuses it (D015).

```text
1. synthetic   aig_ranker synthetic mode -> deterministic preferences
               (policy: variant / baseline / trained_slate)
               -> bootstrap CIM taste cells with zero human time
2. review      aig_ranker review mode -> real human A/B picks (play A, play B)
               -> CIM adapts continually, no retraining
3. replay      deterministic re-run from the JSONL + seed -> confirm the decision
```

Synthetic preferences make M1 runnable before any human listening; real review
turns it into a genuine taste model.

## Held-Out Discipline

The ranker's phases are the held-out boundary (doc 21):

```text
train       feedback the candidate may adapt on
slate_eval  held-out feedback the gate scores on; the candidate must not touch it
```

The candidate may change working state. It must not change the evaluator. Catch
trials (`identical_aa` -> tie, `degraded` -> expected winner) are evaluator-integrity
probes: a candidate that fails a catch trial is rejected regardless of its train
score. This is CIM's anti-hack rule (doc 10) in ranker terms.

## Metrics (doc 03, instantiated for drums)

### Learning

```text
pairwise win-rate        fraction of held-out A/B trials the model predicts right
regret                   cumulative loss vs the best fixed choice per context
time-to-adapt            trials until win-rate recovers after a context switch
retained performance     win-rate on an old context after learning a new one
```

### State

```text
active cells per event   sparsity check (|A_t| << |M|)
cells allocated/merged/split
average confidence, conflict rate
update magnitude, memory footprint (bounded; M1 gate 8)
```

### Explainability

```text
outputs with active-cell explanation (target: 100%)
top contributing cells per decision
provenance trace length (which picks shaped a groove)
```

### Systems (doc 12)

```text
address-scan / score / update / snapshot-restore / replay timing
(existing: make run-memory)
scale sweep to 1M cells, tail percentiles  (make run-scale; run cim-sys-scale-v1)
exact-key index vs scan + byte-exact gate  (make run-scale-index-bench /
                                            run-scale-index-check; run cim-sys-scale-v2)
NN metric index vs scan + byte-exact gate   (make run-scale-nn-index-bench /
                                            run-scale-nn-index-check; run cim-sys-scale-v3)
```

## The Forgetting Probe (drum-shaped)

The doc 03 catastrophic-forgetting probe, in drum terms:

```text
1. learn taste A   e.g. fill-heavy, busy hats in a chorus context
2. learn taste B   e.g. sparse, low-anchor in a verse context (overlapping cues)
3. revisit taste A revisit the chorus context
measure            how much A degraded, and whether cell splitting / confidence
                   decay protected it
```

This is the M1 gate-3 test. It is the most revealing single experiment for the
substrate claim.

### Consolidation sub-probe (D018 / doc 22 R3)

Genuine lossy consolidation (R3) adds three checks the probe must pass. These are
part of the `run-consolidate` **acceptance spec — hard numbers in `config.toml`,
not report lines**:

```text
a. active retention   active contexts retained after consolidation (no
                      catastrophic loss); consolidation evicts only dead/weak cells
b. revival cost       a context evicted as "dead" then revisited must re-learn to
                      within R of its pre-eviction win-rate inside K trials;
                      exceeding the (R, K) bound fails the gate
c. frozen stability   promoted (frozen) cells do not drift: their win-rate on the
                      contexts they own is unchanged across consolidations
```

Revival cost is the honest price of forgetting a dead context: re-learning it cold
is acceptable, but the cost is measured and bounded, never hidden. Schema/merge
consolidation (R3.5) is out of scope until addressing supports prototypes (doc 06,
doc 22 boundaries).

## Named-Run Layout

Every evaluation is a named run with an explicit baseline and retained artifacts,
following the `experiment-evidence-operator` discipline. Runs live under `runs/`
(local, never committed — same convention aig-engine already uses for
`runs/aig-egmd-v1.0.0/...`).

```text
runs/cim-drum-<milestone>-<name>/
  config.toml          task, seed, baseline id, windows, metric thresholds
  events.jsonl         the preference stream used (provenance)
  metrics.json         win-rate, regret, retention, forgetting, footprint
  decisions.jsonl      Loop 2 commit/rollback/quarantine ledger
  summary.md           what was measured, vs which baseline, pass/fail
```

A run that bounds coverage (sampling, top-N, no-retry) must say so in `summary.md`.
Silent truncation reads as "covered everything" when it did not.

## What Counts As Success

```text
M1   the M1 acceptance gate (doc 16) on at least one drum-shaped task,
     vs a simple online baseline, with the forgetting probe passing
M2   the M2 acceptance gate (doc 16): blind A/B prefers taste-adapted grooves
     over the generic E-GMD baseline at the margin set in config.toml
```

## What Counts As An Honest Negative Result (doc 03)

Worth recording, not hiding:

```text
- a simple online logistic baseline beats CIM on the drum task
- addressing (law-axis -> key) dominates every other design choice
- consolidation harms more than the forgetting it prevents
- HDC groove encoding is weaker than the scalar phi path
- explanation overhead is too high to keep 100% coverage
```

> **"Addressing dominates" — CONFIRMED, then ADDRESSED.** `cim-sys-scale-v1`
> confirmed it (`crossover_n = 256`: the linear scan dominates scoring from the
> smallest map). That honest negative result drove Phase 3a: `cim-sys-scale-v2`
> indexed the exact-key scan (`cinm_index`, byte-exact with the scan), making
> `cinm_address` ~O(1) and removing the dominance for exact-key addressing. The
> nearest-neighbour scan is now indexed too (`cinm_nn_index`, a KD-tree,
> `cim-sys-scale-v3`), but at NFEAT=8 the exact tree is backtracking-bound, so it only
> *reduces* NN cost (~8.3× at 1M, sub-1× below ~2k cells) instead of removing the
> dominance the way the hash did for exact keys. The record stays — it was true, was
> measured, and is now resolved for exact keys and quantified for NN (a measured
> constant-factor, not a flatten).
