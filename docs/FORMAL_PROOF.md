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
