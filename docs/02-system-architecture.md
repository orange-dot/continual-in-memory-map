# 02. System Architecture

This document describes a domain-neutral architecture for CINM.

No implementation exists yet. This is a conceptual architecture.

## Components

```text
Event Log
  -> Encoder
  -> Addressor
  -> Synaptic Map
  -> Readout
  -> Feedback Adapter
  -> Consolidator
  -> Snapshot Store
```

## Event Log

The event log is append-only evidence.

Event categories:

- observation
- candidate generation
- choice
- rejection
- correction
- scalar reward
- pairwise preference
- context change
- consolidation
- rollback

Properties:

- append-only by default
- timestamped
- schema-versioned
- deterministic replay where possible
- explicit provenance

## Encoder

The encoder transforms raw events into context and feature representations.

Outputs:

```text
context vector
candidate feature vector
feedback feature delta
metadata/provenance
```

The encoder is allowed to be domain-specific in future applications, but the
core architecture is not.

## Addressor

The addressor selects active cells.

Inputs:

- context vector
- memory map metadata
- optional retrieval index

Outputs:

- active cell ids
- activation values
- novelty score

If novelty is high, the system may allocate a new cell instead of forcing an
update into a poor match.

## Synaptic Map

The synaptic map is the central state.

Expected operations:

```text
activate(context) -> active cells
score(active cells, candidate features) -> score contribution
update(active cells, feedback delta) -> local state change
decay(time) -> state change
snapshot() -> in-RAM map copy
explain(cell id) -> provenance and current role
```

The map should be optimized for:

- sparse access
- local update
- direct inspectability
- memory locality
- bounded growth

## Readout

The readout converts active memory into behavior.

Examples:

- rank candidates
- choose an action
- predict a small preference
- adjust a parameter
- produce a confidence-weighted suggestion

The readout should expose:

- base score
- memory contribution
- penalties
- active cells
- confidence
- explanation

## Feedback Adapter

Feedback arrives in different forms. The adapter normalizes it into update
signals.

Feedback types:

- A beats B
- A ties B
- neither is acceptable
- candidate accepted
- candidate rejected
- manual correction
- scalar reward
- delayed outcome

Pairwise feedback is the preferred first-class form because it is natural for
preference learning and does not require absolute calibration.

## Consolidator

The consolidator runs periodically or on demand.

Responsibilities:

- identify stable repeated patterns
- merge redundant cells
- split conflicted cells
- lower plasticity of stable cells
- produce human-readable summaries
- create compact snapshots
- mark stale cells

Consolidation must not silently rewrite history. It should write a
consolidation event to the log.

## Snapshot Store

Snapshots make runtime state restartable. In the current implementation (D013)
this is **in-RAM**: a snapshot is a whole-map copy (`cinm_snapshot`) plus the
event-log position. A durable on-disk snapshot store — schema-versioned,
checksummed, manifest-checked — is deferred (`docs/future/`).

Snapshot properties (in-RAM today; the rest deferred):

- compact (a `memcpy` of the POD map)
- replay-compatible with the event log
- inspectable enough for debugging
- *(deferred: schema-versioned, checksummed, durable)*

The event log remains the source of evidence. Snapshots are acceleration
artifacts.

## Backend Model

CINM can have multiple backend implementations later:

```text
CPU RAM backend
  default path for sparse continual learning

C kernel backend
  small hot loops, local update, cache experiments

C++/CUDA backend
  batch similarity, dense probes, parallel experiments

Python backend
  simulator and analysis
```

The backend boundary should preserve the same conceptual contract:

```text
activate
score
update
decay
snapshot
explain
```

## Runtime Principle

The core continual loop should remain usable without GPU hardware.

CUDA or other accelerators may add speed for specific workloads, but they
should not become necessary for the basic learning behavior.

## Explainability

Every output should be able to answer:

- which cells were active?
- what did they contribute?
- what evidence created or changed those cells?
- how confident is the contribution?
- did any conflict or decay affect it?

If the system cannot explain a behavior, it has drifted away from the research
goal.

## Failure Modes

Expected failure modes:

- too many cells activate
- too few cells activate
- stale cells dominate
- novelty creates excessive fragmentation
- noisy feedback causes oscillation
- consolidation erases useful minority behavior
- GPU experiments optimize the wrong dense workload
- the system becomes an opaque neural model by accident

These are research targets, not reasons to abandon the project.

