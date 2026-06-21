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
