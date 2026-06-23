# Formal Proof Lane

Status: minimal Isabelle/HOL proof lanes — a core cell-update lane and a replay
sidecar lane.

This proof lane is separate from the executable C smoke tests. It does not claim
that the C implementation is formally verified.

## Smoke Path

The runnable kernel evidence remains under `experiments/c-kernel/`:

```sh
make -C experiments/c-kernel run-replay
make -C experiments/c-kernel run-log-invariants
make -C experiments/c-kernel run-txn
```

These commands validate the current C behavior. They are not theorem proofs.

## Formal Path

The Isabelle session lives under `docs/isabelle/`:

```sh
ISABELLE=/home/dev/sel4/verification/isabelle/bin/isabelle make proof
```

`make proof` loads the core cell-update lane by default. `make proof-build` is a
heavier session build that checks every theory (core and replay sidecar).

## Machine-Checked Scope

### Core proof lane

`docs/isabelle/CIM_Core_V0a_CellUpdate.thy` models one cell update as abstract
scalar arithmetic over the reals (NOT C floats):

- a bounded update stays within `[-wmax, wmax]`
- lower plasticity commands no larger an (unclamped) step than higher plasticity
- for a positive feature delta, negative reward does not raise the margin, given
  the standing invariant that the stored weight is already in `[-wmax, wmax]`
- decay by a factor in `[0, 1]` never increases the absolute weight

### Replay sidecar proof lane

V0a models only the abstract replay core:

- an event has a sequence number, key, and abstract delta
- a map is an abstract function from key to value
- replay applies events left-to-right
- snapshot-tail replay is equivalent to full replay
- splitting a contiguous valid log preserves the validity condition needed by
  the tail

The checked theorems are in `docs/isabelle/CIM_V0a_Log.thy`.

V0b models checked replay outcomes:

- unsupported event kinds return `UnsupportedType`
- bad sequence numbers return `BadSequence`
- exceeding an abstract distinct-key capacity returns `CapacityExceeded`
- those failures are not reported as successful replay

The checked theorems are in `docs/isabelle/CIM_V0b_CheckedReplay.thy`.

## Known Proof Gaps

This proof lane does not verify:

- the C `cinm_map` layout
- C memory safety or allocation behavior
- floating-point arithmetic
- concrete C replay-report field correspondence
- concrete `MAX_CELLS` capacity refinement
- transaction rollback implementation
- correspondence between C structs and the abstract Isabelle event model

## Code/Proof Drift Risks

The proof is useful only for the replay shape that the C code continues to
implement: append-only events, deterministic left-to-right replay, and
snapshot-plus-tail reconstruction. If the C log starts filtering, reordering,
coalescing, or partially accepting invalid input, this proof lane must be
extended before stronger claims are made.

## Scope Under D018 (State Primacy)

Decision D018 demotes full-log replay from "the source of truth" to a within-epoch
recovery and verification property; the live map is primary. This *narrows the
interpretation* of the replay lane — it does not invalidate it:

- The checked theorems (`CIM_V0a_Log.thy`) prove that **snapshot-tail replay
  equals full replay** and that splitting a valid log preserves tail validity.
  Under D018 these are read as **within-epoch determinism** lemmas — exactly the
  property the M1 replay gate now claims — not as a proof that the map's truth is
  the log.
- Consolidation is deliberately **outside the reversible envelope**: it is
  genuinely lossy, so there is no proof obligation (now or planned) that
  post-consolidation state is reconstructable from the surviving log. The
  bounds/reversibility claims hold **within the retention window** between
  consolidations.
- No theorem is removed and no new machine-checked claim is added; this note only
  records the scope D018 puts on what the existing replay lane already means.

## Planned Extension (Drum Vertical, D014)

Backlog sections P and Q (`docs/17`) describe a *planned* extension of the abstract
invariants to the drum vertical's self-adaptation path: bounded operator effects,
rollback restores prior state, and deterministic decision replay. The extension
targets the abstract model only — bounds, reversibility, replay — explicitly NOT
"the drums sound good", which is a human aesthetic judgement and never a proof
claim (doc 19, doc 21). It is gated by the `formal-proof-readiness` posture: no
machine-checked claim is made until the theory actually checks it. Nothing here is
proven yet; this note records the intended direction, not a result.
