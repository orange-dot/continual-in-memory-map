# 04. Language Lines

This document records the intended language split. The current project includes
an imported Python/C HDC seed under `experiments/taste-hdc/`, but the language
lines below still describe the broader research program rather than a finished
implementation.

> **Update (D011):** for this project the ordering below is **inverted** — C is
> the first *exploration* substrate, before Python or Rust. See `05-decisions.md`
> (D011). The role descriptions still hold; only the order changed, and the C
> line now also serves exploration, not just stable hot loops.

## Principle

Each language line should answer a different research need.

```text
Python     -> think fast
Rust       -> define the reference truth
C          -> test memory-cell hot loops
C++/CUDA   -> test parallel acceleration
```

No language should become the hidden authority accidentally.

## Python Line

Role:

- mathematical simulation
- toy tasks
- plotting
- analysis
- dataset sketches
- quick failure exploration

Useful for:

- trying update rules
- comparing activation functions
- visualizing forgetting
- testing consolidation policies
- generating experiment reports

Risk:

- simulator behavior may become too loose or non-deterministic
- fast notebooks can obscure the real state contract

Boundary:

- Python may explore.
- Python should not define the durable state format alone.

Current seed:

- `experiments/taste-hdc/taste/` contains a NumPy bipolar hypervector reference
  for random hypervectors, binding, bundling, similarity, encoding, and a
  two-timescale `TasteMemory`.
- `experiments/taste-hdc/demo.py` is a domain-colored smoke example from the
  source seed. It is useful as a quick HDC sanity check, not as the core CINM
  task definition.

## Rust Line

Role:

- reference engine
- deterministic replay
- schema definitions
- snapshot/log formats
- explainability reports
- testable contracts

Useful for:

- making the concept precise
- preserving auditability
- preventing accidental global hidden state
- later integration with systems work

Risk:

- overengineering too early
- turning theory into infrastructure before toy tasks are understood

Boundary:

- Rust should become the reference only after the Python simulator clarifies the
  equations.

## C Line

Role:

- small memory-cell kernels
- explicit layout experiments
- cache behavior
- SIMD probes
- FFI kernels callable from Rust

Candidate kernel shapes:

```text
cell_activate
cell_score
cell_update
cell_decay
cell_normalize
```

Risk:

- premature optimization
- unsafe memory complexity before the model stabilizes

Boundary:

- C should only receive small, stable kernels.
- C should not own project-level state.

Current seed:

- `experiments/taste-hdc/c/hdc.c` contains a bit-packed binary HDC kernel:
  XOR binding, popcount similarity, and per-bit majority bundling.
- The imported binary from `/home/dev/taste-hdc/c/hdc` was deliberately not
  copied into the lab project.

## C++/CUDA Line

Role:

- GPU probes
- batch similarity
- dense fallback baselines
- parallel activation experiments
- optional acceleration

Useful for:

- comparing CPU sparse lookup versus GPU batch computation
- testing whether any part of CINM maps naturally to CUDA
- exploring hybrid CPU/GPU scheduling

Risk:

- designing the whole system around the GPU
- forcing sparse local learning into dense tensor shapes
- losing the in-memory continual character

Boundary:

- CUDA is an accelerator and laboratory line.
- The core learning loop should remain usable without CUDA.

## Shared Contracts

Future implementation lines should communicate through documented artifacts:

```text
learning_event.v1.jsonl
context_features.v1
candidate_features.v1
synaptic_snapshot.v1
experiment_report.v1
```

The exact formats are not defined yet. The important decision is that language
lines should share contracts, not private assumptions.

## CPU, RAM, And GPU Decision

For the first prototype:

- CPU + RAM is the primary substrate.
- GPU is optional.
- 64 GB RAM is more aligned with large in-memory maps than a small VRAM budget.
- 4 GB VRAM is useful for experiments, not for making CUDA mandatory.

Expected split:

```text
CPU/RAM:
  sparse map
  local updates
  event replay
  explanations

CUDA:
  batch descriptors
  dense baselines
  batch similarity
  offline probes
```
