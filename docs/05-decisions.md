# 05. Decisions

This file records project decisions made during the initial discussion.

## D001: First Project Is Domain-Neutral

Decision:

The first CINM work is theoretical, mathematical, and conceptual. It is not
directly tied to music, ADG, Drum Engine, Magenta, EnCodec, or PC4MS.

Reason:

The substrate should be understood before it is applied to a domain.

Consequence:

Domain examples belong in future bridge notes, not in the core theory.

## D002: Continual Learning Is The Central Problem

Decision:

The core research target is continual learning, not model size, music
generation, or GPU acceleration.

Reason:

The major discomfort with modern AI is that systems often cannot absorb small
lessons directly. They compensate through expensive global retraining.

Consequence:

Every proposed mechanism should be judged by whether it supports small,
bounded, local, immediate updates.

## D003: In-Memory Is A Design Principle, Not Just Storage

Decision:

"In-memory" means learned state is directly available to the runtime and updated
locally. It does not merely mean "keep a database in RAM."

Reason:

The conceptual target is co-location of memory, scoring, and update.

Consequence:

The architecture must avoid offline-only learning loops as the primary path.

## D004: Neuromorphic Means Inspired By Locality

Decision:

The project uses neuromorphic ideas as design inspiration, not as a claim of
biological accuracy.

Reason:

The useful lesson is sparse, local, event-driven adaptation.

Consequence:

The project may use CPU/RAM and still be locality-inspired.

Update (D013): the "neuromorphic" label is retired from the project name and
framing; the locality principle stated here is unchanged.

## D005: CPU + RAM First, CUDA Optional

Decision:

The first serious prototype should run on CPU + RAM. CUDA should be an optional
accelerator and experiment line.

Reason:

Sparse local updates fit CPU memory structures well. A small GPU is useful but
should not define the runtime contract.

Consequence:

GPU results must be compared against CPU baselines.

## D006: Four Language Lines Are Allowed

Decision:

The project may eventually use Python, Rust, C, and C++/CUDA.

Reason:

Each language serves a different research purpose.

Consequence:

The project needs shared schemas and reports before cross-language
implementation begins.

## D007: No Implementation In This Scaffold

Decision:

This first artifact contains documentation only.

Reason:

The concept needs to be made precise before code creates accidental commitment.

Consequence:

No source files, build files, notebooks, or benchmark scripts are part of this
scaffold.

## D008: Append-Only Evidence Is A Core Value

Decision:

Learning events should be append-only by default.

Reason:

Continual learning must remain auditable. A changed behavior should be traced
to evidence.

Consequence:

Snapshots are acceleration artifacts. The event log is the evidence source.

Update (D013): the event log is now in-RAM (`cinm_log`); durable cross-restart
evidence on SSD is deferred. Append-only, auditable evidence still holds within a
process.

## D009: Pairwise Feedback Is First-Class

Decision:

Pairwise preference should be a primary feedback form.

Reason:

It is easier to say "A is better than B in this context" than to assign an
absolute score.

Consequence:

The theory models feature deltas and winner-minus-loser updates directly.

## D010: Consolidation Must Be Explicit

Decision:

Stable local learning may be consolidated, but consolidation must be recorded
and explainable.

Reason:

Silent consolidation can become hidden retraining under another name.

Consequence:

Consolidation is a visible operation with its own event record.

## D011: C Is The First Exploration Substrate

Decision:

The first CINM implementation is written in C, before Python or Rust. The
language ordering in doc 04 (Python, then Rust, then C, then C++/CUDA) is
inverted for this project: C is the first exploration line, not a later
optimization line.

Reason:

CINM is a hypothesis about a substrate, not only about an equation. Several open
questions in doc 06 are systems questions -- cache behavior, memory layout for
sparse access, retrieval cost as the map grows -- and these are only testable
close to the hardware in C. A float simulator abstracts away the property CINM
is trying to claim. Third-party Python numeric libraries are also deliberately
not used, which removes Python's main first-substrate advantage.

Consequence:

- Phase 1's goal is unchanged (validate the model on a toy task); only the
  substrate changes from Python to C.
- The original C-line risk in doc 04, premature optimization, is preserved as a
  constraint: the first C kernel is clarity-first and inspectable. SIMD, cache
  layout, and bit-level tricks are deferred until the update rule and model
  stabilize.
- The first experiment lives in experiments/c-kernel/ (synaptic map plus the
  Contextual Preference Memory task) with no external libraries.
- This amends, but does not delete, D006 and the doc 04 ordering. The four
  language lines still stand; the C line's role expands to include exploration.

## D012: Interactive Continual Preference Learning With Reversible Self-Adaptation

Decision:

CINM's working term for this line of work is now "interactive continual
preference learning with reversible self-adaptation." CINM should first
transfer the reversible adaptation skeleton from the local neuro-symbolic
research repos, not their proof-heavy machinery. The first adaptation loop
should treat candidate changes as reversible transactions: propose, apply
speculatively, evaluate, commit or rollback, and record evidence.

Reason:

In CINM, the confirming signal often comes from a human preference judgement or
another external evaluator. That makes the learning signal preference-based and
supervised in the broad sense. The adaptive part is that the system may propose
small local changes to update rules, plasticity, consolidation, addressing,
capacity, or scoring. A bad change is not expensive if it can be isolated and
reverted immediately. Therefore the first hard property is not "prove this is
useful"; it is "the system can undo this change, replay it, and learn
from the failed attempt."

Consequence:

- The Darwinian side is central: try small variants, measure them, keep local
  winners, preserve diversity, and archive failures as evidence.
- The Godelian side is narrowed: prove or check bounds, reversibility,
  deterministic replay, scorer purity, capacity limits, and event-log integrity
  before attempting proof of usefulness.
- The C kernel should gain snapshot/restore or delta rollback before complex
  proof tooling.
- A candidate must not grade itself. Measurement belongs to the harness/gate,
  not to the proposer.
- Failed adaptations should go to quarantine/negative evidence rather than
  disappearing.

Update (D013): reversible transactions, deterministic replay, and evidence are
in-RAM (`cinm_snapshot`/`cinm_restore`/`cinm_transaction` + `cinm_log`). Durable
archive/quarantine on SSD is deferred; the reversibility and replay invariants
stand within a process.

## D013: RAM-Only Scope; Durable SSD Store Deferred

Decision:

The `experiments/c-kernel` substrate is strictly in-memory: CPU caches + RAM, no
disk tier. The event log, snapshots, and replay live in RAM (`cinm_log` plus the
core `cinm_snapshot`/`cinm_restore`). The durable SSD store (append-only
`events.log`, on-disk snapshots, manifests, mmap HDC banks, io_uring/SPDK) is
deferred as a future hardware mapping, parked under `docs/future/`.

The project also drops the "neuromorphic" label from its name and framing
(doc 06's open question, answered): it becomes "Continual In-Memory Map" (CIM).

Reason:

64 GB RAM + CPU caches are sufficient for the current PoC. The durable store was
scope creep: it inverted the system into a disk-truth / RAM-projection model and
made "in-memory" (D003) misleading. Removing it realigns the implementation with
the stated in-memory principle. "Neuromorphic" was always inspiration-only (D004);
retiring the label removes a recurring source of confusion.

Consequence:

- Narrows the durability aspects of D008 and D012 (above): the replayable-truth
  and reversibility invariants hold within a process, not across restarts.
- Doc 12 splits: the cache/RAM model stays (`12-memory-model.md`); the SSD/Linux
  layer is parked (`docs/future/ssd-store-linux-layer.md`).
- The human-facing acronym becomes **CIM**. The `cinm_` code prefix is retained
  as a stable opaque identifier (renaming every symbol across the kernel is pure
  churn for no benefit); it is no longer re-derived from the project name.

## D014: Drum-First Vertical Slice As The First Case Study

Decision:

The first applied CIM work is a drum-first vertical slice: a taste-driven MIDI
drum generator built as one product, with CIM concepts (cells, event log,
reversible transaction, archive, gate) as its internal structure. The
domain-neutral substrate is extracted later, if and when the vertical proves it
out.

This deliberately amends D001 (domain-neutral first) and the doc 07 ordering
("music only after toy tasks"). It does not delete them; it scopes them.

Reason:

The lab already contains all three layers of this system, in three vocabularies:

```text
evidence / preference   aig-engine aig_ranker_preferences.py
                        (pairwise, append-only, train/slate_eval, catch-trials)
Darwinian generator     neuro-simbolic internal/evolution (MAP-Elites)
gate / critic + human   pc4-microkit-studio drummer player-model
                        (ANTI_PATTERNS, GOLDEN_SET, OUTPUT_CONTRACT)
                        + music-specific-ai-concepts DRUMMER_BASSIST_V0
```

Building the vertical is mostly wiring these existing assets onto the existing C
kernel — the fastest path to a real artifact.

Consequence:

- The doc 07 contamination risk is accepted, not ignored. M1 measures against a
  simple online baseline and uses held-out evaluation so drum heuristics cannot
  masquerade as substrate wins (docs 16, 18, 19, 20).
- A neutral-substrate extraction stays a tracked future move (doc 19), not an MVP
  goal.
- The substrate's domain-neutral charter (D001) still governs that eventual
  extraction. The vertical lives inside this repo but is fenced by claim
  boundaries (doc 19).

## D015: Reuse Existing Lab Assets Over Rebuilding

Decision:

The vertical consumes existing lab assets instead of re-implementing them:

- aig-engine `aig_ranker` JSONL is the event stream; CIM does not invent a new
  preference format.
- only the MAP-Elites *shape* is re-implemented in the vertical's own language;
  the neuro-simbolic Go crate is not a dependency (consistent with doc 10).
- the drummer player-model ANTI_PATTERNS / GOLDEN_SET / catch-trials serve as the
  gate and held-out evaluator.

Reason:

These assets already encode CIM's own values (pairwise-first, append-only,
proposer ≠ grader, human-gated). Rebuilding them would duplicate work and risk
divergence.

Consequence:

- doc 15 records an integration map with exact source paths and pinned schema ids.
- The vertical depends on the *contracts* of these assets, not on importing their
  code wholesale; schema ids are checked during verification.

## D016: Staged MVP — Proof Before Sound

Decision:

The MVP is two milestones with separate acceptance gates:

```text
M1 substrate-proof   CIM invariants hold on a drum-shaped preference task (numbers)
M2 audible generator a taste-driven groove generator you can hear (sound)
```

M2 work does not begin until the M1 gate passes.

Reason:

The substrate claim and the musical claim are different claims with different
evidence. Proving the invariants first prevents an audible-but-unattributable demo
from standing in for substrate evidence.

Consequence:

- doc 16 defines both gates; doc 17 tags each backlog section [M1] and/or [M2].
- M1 reuses already-green kernel gates (`run-txn`, `run-replay`,
  `run-log-invariants`).

## D017: Language Line For The Vertical (Confirm At Code Kickoff)

Decision (recommended; confirm when implementation begins):

- CIM core stays **C** (D011 — it already works).
- Vertical glue and evaluation are **Python** (reuse the existing Python ranker,
  the `taste-hdc` seed, and the EGMD tooling).
- MAP-Elites is re-implemented in C or Python for the MVP.
- Rust / aig-engine deep integration is deferred to post-MVP.

Reason:

The reusable assets (ranker, `taste-hdc`, EGMD importer) are Python; the substrate
core is C. A thin Python glue layer over the C kernel reuses the most for the least
new code. Rust integration with aig-engine is valuable but off the MVP critical
path.

Consequence:

- This is the one open knob in the plan; it stays documentation-only until code
  begins.
- The four language lines (D006) still stand; this only fixes the order for the
  vertical.

## D018: State Is Primary; Replay Is A Verification Property, Not The Source Of Truth

Decision:

The live map is the primary truth — operationally (immediate readout reads it
directly) and as the system's actual state. Full-log replay is **demoted** from
"the source of truth" to a *within-process, within-epoch* recovery and
verification property, plus a lab reproducibility instrument. It is not the
normal mode of operation; the map is never rebuilt from the log on the hot path.

The event log's role splits in two:

```text
decision / evidence ledger   append-only receipts: why each commit or
                             consolidation happened (audit; long-lived)
bounded undo window          snapshot ring + short delta tail, sized to the
                             declared retention horizon (reversal; bounded)
```

Consolidation is **genuinely lossy** — as aggressive as a real learner needs.
After a consolidation boundary the map is no longer reconstructable from the
surviving log. Reversibility is guaranteed only **within the retention window**;
beyond it, state is committed by design.

This amends the "source of truth" framing in doc 12, doc 13 (I-lane), and the
c-kernel surface, and the "snapshots are acceleration artifacts / map is a
derivative" half of D008. It keeps D008's "event log = evidence source" (that is
exactly the demoted role) and extends D010 (consolidation is recorded) with "and
is deliberately irreversible past its boundary."

Reason:

CIM is a *learner*, not a *ledger*. The discriminating question for event
sourcing is whether state is *defined by* events or is a *sufficient statistic*
of them. In a ledger (bank, git, CRDT) history is authoritative by policy and
state is a materialized view. In a learner the cell *is* the compressed
sufficient statistic of the evidence that addressed it — that local compression
is the substrate's reason to exist (D002, D003). Re-deriving the map from raw
events is a global recompute pass, the exact operation the project rejects
(README, D002); it contradicts bounded forgetting (an unbounded ever-growing log
is more memory than the map it rebuilds); and it breaks the moment consolidation
is genuinely lossy.

The kernel already keeps the two mechanisms separate: `cinm_snapshot` /
`cinm_restore` carries transaction rollback (`run-txn`); full replay is the
separate recovery path (`run-replay`). Neither the operational loop nor the
reversibility story rides on full-log replay — only recovery and audit do. So
"replay = truth" was over-claimed doctrine, not a load-bearing code property.

Consequence:

- **The identity claim survives intact.** Operator-level reversibility (Loop 2,
  "reversible self-adaptation", D012) is transaction/snapshot-scoped and
  unaffected by consolidation aggressiveness — the speculative transaction
  snapshots, tries, evaluates on held-out, and commits or restores within one
  epoch.
- **What is windowed is taste-history undo** (Loop 1, "undo N picks"): aggressive
  forgetting shortens it. A real learner *should* lose the ability to undo a habit
  it has consolidated; this is correct, not a regression — but it must be stated,
  or "reversible" is false for N beyond the window.
- **decay vs consolidation are distinct.** Decay = soft, gradual, in-window.
  Consolidation = hard, structural, irreversible past the boundary. This answers
  doc 06's "Can consolidation be reversible?" with *no, by design*.
- **Every irreversible truth-change leaves a receipt** in the decision ledger:
  operator commits/rollbacks AND consolidation events. You forget the content, you
  keep the receipt that you forgot — that is what keeps aggressive forgetting
  auditable (D008, D010).
- **The M1 replay gate (doc 16, gate 5) re-scopes to within-epoch determinism**
  (replay of the tail since the last consolidation reconstructs the current map) —
  exactly what the Isabelle snapshot-tail lane already proves. Demotion does not
  weaken the gate; it scopes it honestly.
- **The forgetting probe (doc 18) is the upper bound on consolidation
  aggressiveness:** aggressive on redundant/stable state, never enough to fail the
  probe (catastrophic forgetting). "As aggressive as needed" stops at "as
  aggressive as passes the probe."
- Ripples: doc 12 (memory-model wording), doc 13 (I-lane invariant), doc 06
  (consolidation question), FORMAL_PROOF (replay theorems are within-epoch
  determinism; consolidation is outside the reversible envelope), and strengthens
  doc 19 claim boundaries. The c-kernel doc/comment surface is corrected as part
  of the refactor that follows this decision.

## D019: Nearest-Neighbour / Prototype Addressing (Alongside Exact-Symbolic Keys)

Decision:

The kernel gains a continuous nearest-neighbour (prototype) addressing mode
(`cinm_address_nn`) alongside the existing exact-symbolic keys (`cinm_address`). A
cell carries a continuous `proto[NFEAT]` context vector as its address; a query
addresses the in-use cell whose prototype is within a novelty radius (squared L2,
libm-free), else it allocates a fresh cell anchored at the query. The prototype is
birth-fixed — only merge consolidation moves it. Exact-symbolic keys remain the
default path and are byte-unchanged (`proto` is zero for them); radius 0 with a
one-hot context degenerates to exact addressing, so NN strictly generalizes the
exact path.

This resolves the lead Addressing Question in doc 06 ("exact symbolic keys first, or
continuous nearest-neighbour keys first?") with *both* — exact first, NN added — and
unblocks R3.5 (doc 22).

Reason:

A learner's reason to exist is local compression (D002, D003, D018: "the cell is the
compressed sufficient statistic of the evidence that addressed it"). Under
exact-symbolic keys every distinct context is its own cell, so "the same context with
variation" fragments without bound and consolidation can only evict-dead and
freeze-strong — there is no redundancy to compress (the R3 ceiling, doc 22).
Prototype addressing lets near-identical contexts share a cell, and lets
consolidation merge near-duplicate prototypes into one (schema compression). That is
the local compression the substrate claims, made structural.

Consequence:

- Additive: NN is a parallel mode; the exact path and all its green gates are
  untouched. Merge is opt-in via `merge_radius2` (0 = off).
- Unblocks R3.5 (doc 22): `cinm_consolidate` now merges near-duplicate prototypes
  (evidence-weighted blend, frozen-exempt, receipted, genuinely lossy). It answers
  doc 06's "how should novelty be detected" (a radius threshold) and "what prevents
  over-fragmentation" (novelty radius + merge radius) with measured numbers
  (`run-nn-address`).
- The gate `run-nn-address` is neutral (contextual preference with variation, doc 03),
  no drum dependency. Its headline: NN folds jittered contexts into a few prototypes
  where exact keys fragment to the cell ceiling.
- Pre-registered negative-result watch (doc 03): if addressing turns out to dominate
  every other design choice, that is a recorded result, not a silent win.
- Deferred, not dropped: cell *splitting* (the inverse of merge); replayable NN
  addressing (the event log stays exact-key — `cinm_event` carries `key`, not a
  context vector); an EMA / drift-tracking prototype (birth-fixed ships first).
- The kernel stays libm-free (squared-L2 distance, no sqrt) and RAM-only (D013); the
  `cinm_` prefix is unchanged.
