# Continual In-Memory Map

Status: research scaffold with an imported prototype seed and an active C kernel
experiment.

> Renamed from "Continual In-Memory Neuromorphic Map" — the "neuromorphic" label
> is retired (decision D013, doc 06). The locality design principle (D004) is
> unchanged; the `cinm_` code prefix is kept as a stable identifier.

This project captures a domain-neutral research concept for a small continual
learning substrate. It is not an application, not a music system, and not a
replacement for the existing ADG or Drum Engine work. Those domains may become
case studies later. The first work is theoretical, mathematical, and systems
oriented, with a small imported HDC playground preserved as an experiment seed.

Working name:

```text
CIM: Continual In-Memory Map
```

(The code keeps the `cinm_` prefix as a stable identifier.)

Core thesis:

```text
event stream
  -> sparse context addressing
  -> local memory-cell activation
  -> bounded local update
  -> immediate readout
  -> periodic consolidation
```

The experiment asks whether a small local memory field can learn continuously
from events without global retraining, while remaining inspectable, bounded,
and useful immediately after each event.

Current terminology for the human/evidence-gated adaptation line:

```text
interactive continual preference learning
  with reversible self-adaptation
```

## Motivation

The starting discomfort is that modern neural networks make training the
central operation. They often require large global optimization passes,
expensive dense compute, and poor continual learning behavior. This project
explores a different regime:

- continual learning as the normal case
- memory and computation kept together
- sparse activation instead of full-model activation
- local plasticity instead of global retraining
- append-only learning evidence instead of opaque weight churn
- bounded state change, decay, conflict handling, and consolidation

The goal is not to beat transformers at their own task. The goal is to define
and test a different learning substrate.

## Reading Order

1. `docs/00-conversation-summary.md`
2. `docs/01-theory.md`
3. `docs/02-system-architecture.md`
4. `docs/03-research-program.md`
5. `docs/04-language-lines.md`
6. `docs/05-decisions.md`
7. `docs/06-open-questions.md`
8. `docs/07-future-domain-bridge.md`
9. `docs/08-imported-taste-hdc-seed.md`
10. `docs/09-market-and-research-landscape.md`
11. `docs/10-neurosymbolic-transfer.md`
12. `docs/11-c-cpu-simd-hdc.md`
13. `docs/12-memory-model.md`
14. `docs/13-implementation-backlog-g-k.md`
15. `docs/14-tail-latency-hedging.md`
16. `docs/FORMAL_PROOF.md`

Deferred / future mapping (not in current RAM-only scope, decision D013):

- `docs/future/ssd-store-linux-layer.md` — durable SSD store + Linux deep dive.

## Layout

- `docs/` records the theory, decisions, and research program.
- `docs/isabelle/` contains the minimal Isabelle/HOL proof lane for abstract
  replay, snapshot-tail equivalence, and checked replay rejection.
- `experiments/taste-hdc/` preserves the imported CPU + RAM HDC taste-memory
  seed from `/home/dev/taste-hdc`.
- `experiments/c-kernel/` contains the first C substrate: SoA synaptic map,
  pairwise update, sigma A/B harness, reversible transaction check, and an in-RAM
  event-log replay/recovery check, plus G-K evidence, learning, memory, and HDC
  experiment gates. Strictly in-memory — no disk tier (decision D013).

The imported seed is intentionally small and exploratory. It is not the final
CINM implementation contract.

## Non-Goals

- No production implementation claim.
- No ADG, Drum Engine, PC4MS, or music-specific dependency in the core concept.
- No claim that this is biologically faithful neuroscience.
- No claim that this replaces backpropagation for large-scale representation
  learning.
- No production claim, benchmark claim, or quality claim.

## First Artifact Target

The first durable artifact should remain a clean research note and experiment
plan. The imported prototype seed can inform this work, but it should not
silently become the theory.

```text
theory
  + state model
  + local update rules
  + consolidation rules
  + toy task suite
  + language/backend split
  + explicit claim boundaries
```
