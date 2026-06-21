# 00. Conversation Summary

This file records the discussion that produced the CINM scaffold. It is written
as a durable project note rather than a transcript.

> Scope note (D013): the project is now "Continual In-Memory Map" — the
> "neuromorphic" label is retired (the locality principle, D004, stands) and the
> kernel is strictly in-memory (no SSD tier). Some framing below predates that
> decision.

## Starting Concern

The initial concern was that neural networks feel like an inappropriate
computational method because they inherit vocabulary from biology while modern
training is unlike biological learning.

The refined diagnosis:

- The word "neural" is mostly historical branding.
- Modern neural networks are better described as trainable differentiable
  circuits optimized by gradient descent.
- The real discomfort is not that the method is biologically inspired. The real
  discomfort is that current systems often depend on large, global, opaque,
  expensive training loops.

## Training Cost

The central objection became training cost and continual learning.

Modern AI often pays a huge global training bill:

- dense compute over many parameters
- repeated full-corpus retraining
- weak incremental update behavior
- catastrophic forgetting when naively updated online
- high energy cost from moving data between memory and compute

The important distinction:

- some cost is the lawful cost of general learning over high-dimensional data
- some cost is engineering and architectural waste

The project focuses on the second category without pretending the first does
not exist.

## Continual Learning

Continual learning was identified as the deeper issue.

If a system cannot learn from each small event, it compensates by accumulating
data and retraining globally. That creates the compute and energy profile that
feels wrong for small, personal, adaptive systems.

Desired behavior:

```text
new event
  -> small local update
  -> immediately available changed behavior
  -> no global retraining
  -> no loss of old knowledge
  -> inspectable reason for the change
```

## Neuromorphic Angle

Neuromorphic thinking was useful because it reframed the problem as one of
locality.

The key systems insight:

```text
moving data can cost more than computing on it
```

Digital GPU or CPU systems separate memory and compute. Biological synapses and
analog in-memory proposals suggest a different pattern: memory, computation,
and update rule are co-located.

The project does not assume memristors, spiking chips, or special hardware. It
borrows the design principle:

```text
keep learning local to the state that was activated
```

## In-Memory Direction

The in-memory part means that learned state should be directly accessible to
the runtime. Learning should not be an offline artifact that must later be
loaded into a separate inference system.

The runtime state should support:

- sparse lookup
- local scoring
- local update
- confidence tracking
- decay
- conflict detection
- rollback through append-only logs
- periodic consolidation into more stable structures

## Hardware Discussion

The available machine context included large RAM and a small CUDA GPU.

Decision:

- CPU + RAM should be the main prototype substrate.
- CUDA should be optional and experimental.
- GPU work is useful for batch descriptors, dense similarity, audio/perception
  experiments, and parallel probes.
- The core continual learning loop should not depend on GPU availability.

Reason:

- sparse local updates are branchy and memory-structure heavy
- GPU excels at dense tensor workloads
- small, frequent updates can lose to CPU due to transfer and launch overhead
- a large RAM budget is more aligned with in-memory learning maps

## Language Lines

The project should eventually allow four implementation lines:

```text
Python     -> theory, simulation, analysis, plots, datasets
Rust       -> reference engine, deterministic state, snapshots, contracts
C          -> small memory-cell kernels, cache locality, SIMD experiments
C++/CUDA   -> batch similarity, GPU kernels, parallel probes
```

This scaffold contains no implementation. The language split is documented only
to prevent future confusion.

## Domain Separation

The concept first emerged during a music-system discussion, but the first work
is intentionally not music-specific.

Decision:

- The first project is theoretical and mathematical.
- It should not be coupled to ADG, Drum Engine, Magenta, EnCodec, PC4MS, or any
  other domain system.
- Music can be a later case study after the substrate is defined independently.

## Working Name

Chosen working name:

```text
CINM: Continual In-Memory Neuromorphic Map
```

The name is descriptive, not final branding.

