# 03. Research Program

This document defines the initial research program.

## Main Question

Can a small local memory field learn continuously from events without global
retraining, while staying inspectable, bounded, and immediately useful?

## Hypotheses

### H1: Local preference updates are enough for small adaptive behavior

For tasks with repeated contextual feedback, a sparse local memory map can
improve choices without retraining a global model.

### H2: Bounded local updates reduce catastrophic forgetting

Old behavior should not be overwritten unless the same context repeatedly
provides contrary evidence or consolidation explicitly changes the structure.

### H3: In-memory state gives immediate learning

After a feedback event, changed behavior should be visible on the next relevant
query.

### H4: Inspectability is compatible with useful adaptation

The system should be able to report which memory cells changed and why without
destroying performance on small tasks.

### H5: CPU + RAM is the right first substrate

Sparse local updates should be efficient and simpler on CPU + RAM before GPU
acceleration is explored.

## Toy Task Suite

The first experiments should be domain-neutral.

### Non-Stationary Bandit

Task:

- choose between actions
- reward distribution changes over time
- context may indicate the regime

Purpose:

- test adaptation
- test old-regime retention
- test conflict and cell splitting

### Contextual Preference Memory

Task:

- rank candidates from features
- receive pairwise feedback
- contexts repeat with variation

Purpose:

- test pairwise local updates
- test immediate reranking
- test explanation quality

### Sequence Adaptation

Task:

- observe short event sequences
- adapt local predictions or choices based on recent sequence context

Purpose:

- test eligibility traces
- test short-term versus long-term memory

### Catastrophic Forgetting Probe

Task:

- learn behavior A
- learn behavior B in overlapping context
- revisit behavior A

Purpose:

- measure overwrite
- test cell splitting and confidence decay

### Sparse Versus Dense Baseline

Task:

- compare CINM against a small dense online learner on the same feedback stream

Purpose:

- identify when sparse locality helps or hurts
- avoid self-deception

## Metrics

Learning metrics:

- regret
- pairwise win rate
- time to adapt
- retained performance on old contexts
- recovery after context switch

State metrics:

- active cells per event
- cells allocated
- cells merged
- cells split
- average confidence
- conflict rate
- update magnitude
- memory footprint

Explainability metrics:

- percentage of outputs with active-cell explanation
- top contributing cells
- provenance trace length
- consolidation summary quality

Systems metrics:

- update latency
- readout latency
- memory bandwidth
- cache behavior
- CPU versus GPU overhead

## Experiment Phases

### Phase 0: Documentation

Current phase.

Deliverables:

- theory
- architecture
- decision log
- open questions
- research program
- language-line plan

No code.

### Phase 1: Python Simulator

Purpose:

- validate equations
- visualize behavior
- rapidly change update rules

Outputs:

- plots
- toy task results
- failure notes

### Phase 2: Rust Reference Engine

Purpose:

- deterministic state
- replayable logs
- snapshot format
- stable contracts

Outputs:

- reference behavior
- replay tests
- explanation reports

### Phase 3: C Kernel Probe

Purpose:

- isolate hot memory-cell operations
- measure locality
- test SIMD or cache-optimized layouts

Outputs:

- microbenchmarks
- ABI design notes
- performance envelope

### Phase 4: C++/CUDA Probe

Purpose:

- test batch similarity and parallel update patterns
- identify which parts actually benefit from GPU

Outputs:

- GPU versus CPU comparison
- transfer overhead analysis
- list of CUDA-appropriate operations

### Phase 5: Domain Case Study

Purpose:

- apply the substrate to a real domain only after the general model is clear

Possible future case studies:

- music preference learning
- local agent preference memory
- adaptive ranking
- personal tool behavior

## Acceptance Criteria For The First Real Prototype

A first prototype is interesting only if it can show:

- learning after a single event
- bounded update size
- replayable learning log
- no global retraining step
- active-cell explanation
- retention test against an older context
- explicit failure report

## Negative Results To Preserve

These outcomes are valuable and should be recorded:

- local updates are too noisy
- addressing dominates all other design choices
- consolidation causes more harm than forgetting
- GPU acceleration is not useful for sparse updates
- simple online logistic regression beats CINM on most toy tasks
- explainability adds too much overhead

The project should prefer honest negative results over optimistic naming.

