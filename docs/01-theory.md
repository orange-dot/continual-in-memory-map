# 01. Theory

This document defines the first theoretical model for CINM.

## Intuition

CINM treats learning as local modification of memory cells activated by an
event. It avoids making global retraining the primary learning operation.

The system receives a stream of events:

```text
x_1, x_2, ..., x_t
```

For each event, only a small set of memory cells becomes active. These cells
contribute to a readout. If feedback arrives, only those active cells receive a
bounded update.

## State

The memory map contains cells:

```text
cell_i = {
  key_i,
  w_i,
  e_i,
  confidence_i,
  plasticity_i,
  age_i,
  last_touched_i,
  conflict_i,
  provenance_i
}
```

Where:

- `key_i` defines the context region where the cell activates.
- `w_i` is the local weight or preference vector.
- `e_i` is an eligibility trace.
- `confidence_i` estimates how stable the cell's learned signal is.
- `plasticity_i` controls how much the cell can still change.
- `age_i` and `last_touched_i` support decay and consolidation.
- `conflict_i` records contradiction pressure.
- `provenance_i` ties the cell to learning events.

## Event Encoding

An event `x_t` is encoded into a context vector:

```text
c_t = encode_context(x_t)
```

The context may include discrete tags, numeric features, time features, or
structured descriptors. The core theory does not require a specific domain.

## Sparse Addressing

The active set is selected by an addressing function:

```text
A_t = address(c_t, M)
```

Where:

- `M` is the memory map.
- `A_t` is a small set of cell indices.

Possible addressing strategies:

- exact key match
- locality-sensitive hashing
- nearest neighbors
- product quantization
- tree or trie routing
- learned address projection
- hybrid symbolic + numeric lookup

The first theoretical requirement is sparsity:

```text
|A_t| << |M|
```

## Activation

Each active cell receives an activation:

```text
a_i(t) = activation(key_i, c_t)
```

The activation may be binary or continuous.

Expected properties:

- high for matching contexts
- low for unrelated contexts
- cheap to compute
- stable enough to make repeated evidence accumulate

## Readout

Given a candidate `y` and context `x_t`, define candidate features:

```text
phi_t(y) = features(y, x_t)
```

The score is:

```text
score(y | x_t) =
  base(y, x_t)
  + sum_{i in A_t} a_i(t) * dot(w_i, phi_t(y))
  - penalties(y, x_t)
```

The base term may be a deterministic heuristic, a small model, or zero. CINM
does not require the base term to be learned.

## Pairwise Feedback

For a comparison where candidate `y+` is preferred over `y-`:

```text
Delta phi_t = phi_t(y+) - phi_t(y-)
```

The local update uses only active cells:

```text
e_i <- lambda * e_i + a_i(t) * Delta phi_t
w_i <- clamp(w_i + eta * reward_t * plasticity_i * e_i - decay_i)
```

Where:

- `lambda` controls eligibility memory.
- `eta` is the learning rate.
- `reward_t` may be positive, negative, or confidence-weighted.
- `clamp` bounds the state change.
- `decay_i` prevents stale or weak evidence from accumulating forever.

This is inspired by local plasticity and eligibility traces, but it is not a
claim of biological realism.

## Direct Feedback

For scalar feedback `r_t` over a candidate:

```text
w_i <- clamp(w_i + eta * r_t * a_i(t) * phi_t(y))
```

Pairwise feedback is preferred when possible because it avoids requiring an
absolute score scale.

## Confidence

Each cell tracks confidence:

```text
confidence_i <- update_confidence(
  previous_confidence_i,
  agreement_i,
  conflict_i,
  evidence_count_i
)
```

Confidence should rise when repeated events support the same local direction
and fall when similar contexts produce contradictory updates.

## Conflict

Conflict is not an error. In a continual system, contradiction is information.

Potential policies:

- lower confidence
- split the cell into more specific cells
- route by hidden context feature
- retain both variants with different provenance
- require consolidation review before promotion

## Decay

Decay prevents the map from becoming an unbounded archive of stale impulses.

Possible decay targets:

- weight magnitude
- eligibility trace
- plasticity
- confidence
- activation priority

Decay must preserve auditability. A decayed cell should still be explainable
through its provenance or through a compacted summary.

## Homeostasis

Without homeostasis, frequently activated cells may dominate. Homeostasis keeps
the system usable.

Possible mechanisms:

- normalize total outgoing weight per cell
- cap update rate per time window
- lower plasticity after repeated stable updates
- split overloaded cells
- merge low-value cells
- reserve capacity for novel contexts

## Consolidation

Consolidation turns repeated local evidence into a more stable structure.

Examples:

```text
many event updates
  -> stable local cell
  -> consolidated rule/profile/adapter
  -> lower plasticity
  -> higher confidence
```

Consolidation should be explicit, inspectable, and reversible where possible.

## Forgetting

CINM should distinguish several kinds of forgetting:

- harmful forgetting: old useful behavior is accidentally overwritten
- useful forgetting: stale evidence loses influence
- contextual separation: old and new behaviors coexist in different contexts
- compaction: detailed evidence becomes a summary

The goal is not to never forget. The goal is to avoid uncontrolled overwrite.

## Claim Boundary

The theoretical claim is modest:

```text
A sparse local memory map can provide immediate continual adaptation for tasks
where useful feedback is local, contextual, and repeated.
```

The project does not claim:

- universal intelligence
- replacement of deep learning
- biological faithfulness
- hardware neuromorphic performance
- superiority before experiments exist

