# 08. Imported taste-hdc Seed

This note records the import of `/home/dev/taste-hdc` into the CINM research
project.

## Import Location

```text
experiments/taste-hdc/
```

## Source Location

```text
/home/dev/taste-hdc
```

The source folder was copied, not removed.

## Imported Files

```text
experiments/taste-hdc/README.md
experiments/taste-hdc/demo.py
experiments/taste-hdc/taste/__init__.py
experiments/taste-hdc/taste/encoder.py
experiments/taste-hdc/taste/hdc.py
experiments/taste-hdc/taste/memory.py
experiments/taste-hdc/c/hdc.c
```

## Deliberately Excluded

```text
/home/dev/taste-hdc/c/hdc
/home/dev/taste-hdc/taste/__pycache__/
```

Reasons:

- `c/hdc` is a compiled ELF binary, not source.
- `__pycache__/` is generated Python cache state.

## What The Seed Contains

The seed is a small hyperdimensional computing playground.

Python line:

- bipolar `+1/-1` hypervectors in NumPy
- random hypervector generation
- bundling by majority vote
- binding by elementwise product
- normalized similarity
- an encoder that binds symbols to positions
- `TasteMemory` with an episodic store and placeholder consolidation step

C line:

- bit-packed binary hypervectors
- XOR binding
- popcount/Hamming similarity
- per-bit majority bundling
- tiny reproducible PRNG
- standalone demonstration in `main`

## Why It Belongs Here

The seed is not the final CINM design, but it already expresses several ideas
from the theory:

- memory and computation are close together
- learning can be append-like and immediate
- high-dimensional representations can support noisy associative recall
- there is a fast episodic path and a slower consolidation path
- CPU + RAM is enough for the first playground
- the C line tests a more substrate-honest bit-packed representation

## Boundary

This import does not change the core project boundary:

- CINM remains domain-neutral.
- Music examples in the seed are illustrative only.
- The seed does not define the final event log, snapshot format, update rule, or
  benchmark suite.
- The incomplete `consolidate()` method is a useful TODO, not a finished
  continual-learning result.

## Immediate Research Use

The seed can support the first exploratory questions:

- How does HDC bundling behave as a prototype mechanism?
- How much can episodic recall do before consolidation exists?
- What should consolidation mean: greedy clusters, label prototypes, or
  confidence-weighted prototypes?
- Does the bit-packed C representation preserve the useful behavior of the
  Python bipolar representation?
- Which parts should remain as HDC/VSA and which parts need a more general CINM
  memory-cell model?

## Next Documentation Step

Before extending the code, write a small design note for consolidation policy:

```text
episodes
  -> group by similarity / label / context
  -> bundle into prototype
  -> record provenance and n
  -> clear or retain fast episodic entries by policy
```

That policy is the first real bridge from the imported seed to CINM theory.
