# CINM Market And Research Landscape

Reviewed: 2026-06-21

This document records the first web research pass for CINM:

```text
CINM: Continual In-Memory Neuromorphic Map
```

The question was:

```text
Do products, papers, research prototypes, or active implementation efforts
already exist that match this idea?
```

Short answer:

```text
No exact CINM-equivalent product was found in this pass.

Many adjacent pieces exist:
- HDC/VSA libraries and papers
- in-memory HDC papers and accelerators
- neuromorphic continual-learning research
- neuromorphic edge chips and SDKs
- compute-in-memory inference accelerators
- agent-memory products
- conventional continual-learning toolkits

The apparent gap is the combination:
small domain-neutral continual memory substrate
  + local bounded updates
  + in-memory / compute-near-memory design discipline
  + HDC/VSA-friendly representations
  + inspectable cells, prototypes, evidence, and consolidation
  + CPU/RAM-first reference path
  + optional C/Rust/C++/CUDA/backend acceleration later
```

This is not a patent search, procurement report, or exhaustive academic
survey. It is a positioning note for the local lab.

## CINM In One Sentence

CINM asks whether a small local memory field can learn continuously from an
event stream without global retraining, while remaining bounded, inspectable,
and useful immediately after each event.

The current conceptual flow is:

```text
event stream
  -> sparse context addressing
  -> local memory-cell activation
  -> bounded local update
  -> immediate readout
  -> periodic consolidation
```

The core bet is not simply "use HDC" or "use neuromorphic hardware".

The core bet is:

```text
learning should happen where the memory lives,
from small local events,
with explicit evidence and bounded state change.
```

## Main Conclusion

The market has products near the idea, but not the whole shape.

There are HDC libraries, neuromorphic chips, in-memory accelerators, continual
learning frameworks, and LLM agent-memory systems. Each covers a useful slice.
None of the reviewed systems is obviously a domain-neutral, inspectable,
CPU/RAM-first continual in-memory learning substrate of the CINM kind.

That does not mean the idea is guaranteed novel in an academic or patent sense.
It means the first landscape scan did not reveal a directly substitutable
product or obvious open-source project.

## What Already Exists

### 1. HDC / VSA Foundations

Hyperdimensional Computing, also called Vector Symbolic Architectures, is a
real and mature-enough research family. It represents information with large
distributed vectors and uses algebraic operations such as bundling, binding,
permutation, and similarity search.

Relevant sources:

- "A Survey on Hyperdimensional Computing aka Vector Symbolic Architectures,
  Part I: Models and Data Transformations"  
  https://arxiv.org/abs/2111.06077
- "A Survey on Hyperdimensional Computing aka Vector Symbolic Architectures,
  Part II: Applications, Cognitive Models, and Challenges"  
  https://arxiv.org/abs/2112.15424
- TorchHD, an open-source Python/PyTorch HDC/VSA library  
  https://github.com/hyperdimensional-computing/torchhd  
  https://arxiv.org/abs/2205.09208

Why this matters for CINM:

HDC/VSA gives a compact algebra for compositional memory. It is naturally
friendly to:

- one-shot or few-shot updates
- similarity-based recall
- noisy hardware
- bit-packed or low-precision operations
- interpretable prototypes
- CPU/GPU vector operations

Why it is not already CINM:

HDC by itself is a representation and computation family. It does not prescribe
the full continual-learning substrate: event log, cell lifecycle,
provenance, conflict handling, fast/slow consolidation, bounded local update
rules, or backend contracts.

### 2. In-Memory HDC

There is direct research on HDC running in memory arrays.

Relevant sources:

- "In-memory hyperdimensional computing"  
  https://arxiv.org/abs/1906.01548
- "XL-HD: Extended Learning in Hyperdimensional Computing via Deterministic
  Projections for In-Memory Accelerators"  
  https://arxiv.org/abs/2605.24788
- "ImageHD: Energy-Efficient On-Device Continual Learning of Visual
  Representations via Hyperdimensional Computing"  
  https://arxiv.org/abs/2604.21280

Why this matters for CINM:

This is very close to the hardware intuition we discussed. The important idea
is that HDC often manipulates large patterns that are naturally memory-heavy,
so it can benefit from avoiding the memory-bus cost. In analog in-memory
systems, matrix-vector style operations can happen physically where the weights
or patterns live.

Why it is not already CINM:

The reviewed in-memory HDC work is mostly task/system oriented: classification,
accelerator design, FPGA/PCM/ReRAM-style execution, and hardware-aware
optimizations. CINM is trying to define a general memory substrate first:
cells, evidence, local update, consolidation, decay, conflict, inspectability,
and backend portability.

ImageHD is especially relevant because it explicitly combines HDC with
on-device continual learning. CINM should treat it as a near neighbor and
baseline inspiration, not as unrelated work.

### 3. Spiking + HDC Hybrids

There is research combining spiking neural networks with HDC.

Relevant source:

- "Spiking Hyperdimensional Network: Neuromorphic Models Integrated with
  Memory-Inspired Framework"  
  https://arxiv.org/abs/2110.00214

Why this matters for CINM:

SpikeHD supports the intuition that low-level temporal/event processing and
high-level distributed memory can be separated. Spiking systems can process
events and temporal structure; HDC can hold abstracted memory and do robust
classification or recall.

Why it is not already CINM:

SpikeHD is a model architecture direction. CINM is more of a substrate:
append-only evidence, mutable but bounded memory cells, local credit/update
rules, consolidation policy, and an implementation path that starts on ordinary
CPU/RAM.

### 4. Neuromorphic Continual Learning

Neuromorphic continual learning is an active research area.

Relevant sources:

- "Continual Learning with Neuromorphic Computing: Theories, Methods, and
  Applications"  
  https://arxiv.org/abs/2410.09218
- "Real-time Continual Learning on Intel Loihi 2"  
  https://arxiv.org/abs/2511.01553
- "CLANE: Continual Learning of Actions on Neuromorphic Hardware from Event
  Cameras"  
  https://arxiv.org/abs/2605.28387
- "Genesis: A Spiking Neuromorphic Accelerator With On-chip Continual
  Learning"  
  https://arxiv.org/abs/2509.05858

Why this matters for CINM:

This confirms that the core pain is not imaginary. Continual learning under
power, memory, and latency constraints is a known hard problem. Neuromorphic
work attacks it with event-driven computation, local learning rules,
metaplasticity, sparse activity, and sometimes on-chip learning.

Why it is not already CINM:

Most of this line is SNN-first or hardware-first. CINM starts from a
memory-substrate contract and keeps the biological inspiration loose:
locality, sparse activation, event-driven updates, fast/slow memory, and
bounded plasticity. It does not require biological faithfulness or a specific
spiking chip.

### 5. Intel Loihi / Lava

Intel's neuromorphic ecosystem is a major research reference point.

Relevant sources:

- Intel Neuromorphic Computing  
  https://www.intel.com/content/www/us/en/research/neuromorphic-computing.html
- Lava software framework  
  https://lava-nc.org/

Important observed facts from the public pages:

- Loihi 2 is Intel's second-generation neuromorphic processor.
- Lava is the associated open-source software framework.
- Intel describes Loihi 2 around sparse, event-driven computation, integrated
  memory and computing, asynchronous SNNs, and sparse changing connections.
- Lava documentation includes STDP, custom learning rules, and three-factor
  learning examples.

Why this matters for CINM:

Loihi/Lava is the strongest "real neuromorphic research stack" reference in
this scan. It gives vocabulary and possible future backend inspiration for
local learning and event-driven execution.

Why it is not already CINM:

Loihi is a research hardware ecosystem, not a small CPU/RAM-first general
CINM memory substrate. CINM can learn from Loihi/Lava without depending on
access to Loihi hardware.

### 6. Commercial Neuromorphic Edge Hardware

Several vendors are building neuromorphic or neuromorphic-adjacent edge AI
products.

Relevant sources:

- BrainChip Akida  
  https://brainchip.com/
- Innatera Pulsar / Talamo SDK  
  https://www.innatera.com/
- SynSense products and software  
  https://www.synsense.ai/

Observed positioning:

- BrainChip presents Akida as a neuromorphic edge AI portfolio covering IP,
  tools, models, chips, and developer/cloud tooling.
- Innatera positions itself around ultra-low-power neuromorphic processing for
  real-world edge devices, with spiking compute fabric, RISC-V control, sensor
  interfaces, and SDK direction.
- SynSense offers neuromorphic chips and software for ultra-low-power,
  ultra-low-latency edge applications, including products such as Xylo, Speck,
  AEVEON, DVS series, and software such as Rockpool, Sinabs, and Samna.

Why this matters for CINM:

These companies validate the substrate argument: energy, locality, event
streams, sparse processing, and edge learning are becoming product-shaped.

Why they are not already CINM:

They are hardware/product platforms for edge AI. CINM is currently a
domain-neutral research substrate and mathematical/software concept. The first
implementation target is not a chip; it is a clean reference model that can
later map to C/Rust/C++/CUDA and possibly neuromorphic/in-memory backends.

### 7. Compute-In-Memory Inference Hardware

There are commercial efforts attacking the memory bottleneck for AI inference.

Relevant sources:

- d-Matrix  
  https://www.d-matrix.ai/
- Mythic  
  https://mythic.ai/
- EnCharge AI  
  https://www.enchargeai.com/

Observed positioning:

- d-Matrix focuses on memory-centric compute and stacked memory for low-latency
  AI inference at scale.
- Mythic focuses on analog compute-in-memory for AI inference, with parameters
  stored directly in the processor to reduce the memory bottleneck.
- EnCharge AI positions analog in-memory computing for edge-to-cloud AI
  acceleration.

Why this matters for CINM:

These companies confirm the "computation is cheap, data movement is expensive"
argument. They are commercial evidence that the memory wall is not academic
hand-wringing.

Why they are not already CINM:

Their public positioning is inference acceleration, mostly for existing AI
workloads. CINM is about continual local learning and inspectable memory state,
not only faster inference of a pre-trained dense model.

### 8. Agent Memory Products

LLM agent-memory products exist and are commercially active.

Relevant sources:

- Zep  
  https://www.getzep.com/
- Mem0  
  https://mem0.ai/

Observed positioning:

- Zep offers persistent agent memory using temporal context graphs, source
  episodes, governance, retrieval, and provenance.
- Mem0 offers drop-in persistent memory infrastructure for agents and apps,
  including memory extraction, update, and retrieval.

Why this matters for CINM:

These products are close in spirit to "continual memory" at the application
layer. They also validate provenance, temporal change, and retrieval as
first-class product concerns.

Why they are not already CINM:

They are agent-memory systems around LLM context and knowledge graphs. CINM is
lower-level: a learning substrate with local memory cells and bounded updates.
Agent memory could be a client of CINM later, but it is not the same layer.

### 9. Conventional Continual-Learning Libraries

Standard continual-learning tooling also exists.

Relevant source:

- Avalanche  
  https://avalanche.continualai.org/

Observed positioning:

Avalanche is an end-to-end PyTorch-based continual-learning library with
benchmarks, training strategies, evaluation, models, logging, and reproducible
research support.

Why this matters for CINM:

Avalanche is useful as a benchmark/comparison ecosystem. It can provide
baselines and experimental vocabulary.

Why it is not already CINM:

Avalanche is a framework for continual-learning experiments, mostly in the
standard ML stack. CINM is proposing a different memory/update substrate rather
than another training strategy inside a conventional neural model.

## Gap Analysis

The missing combination appears to be:

```text
HDC/VSA-compatible representation
  + local memory cells
  + sparse context addressing
  + online event updates
  + append-only evidence log
  + explicit conflict and forgetting
  + fast episodic memory
  + slow consolidated prototypes
  + inspectable state and provenance
  + CPU/RAM-first implementation discipline
  + optional CUDA / SIMD / in-memory backend experiments
```

The strongest nearby research neighbors are:

- ImageHD, because it combines HDC and on-device continual learning.
- In-memory HDC, because it directly combines HDC and in-memory hardware.
- Loihi 2 continual-learning work, because it attacks online continual learning
  with local event-driven neuromorphic computation.
- Agent-memory products, because they productize persistent memory,
  provenance, and temporal change.

The strongest market neighbors are:

- BrainChip, Innatera, SynSense for neuromorphic edge AI.
- d-Matrix, Mythic, EnCharge AI for memory-centric or in-memory acceleration.
- Zep and Mem0 for persistent agent memory.

The gap is not that nobody works on memory or continual learning. Many people
do. The gap is the precise layer boundary.

CINM should be framed as:

```text
an inspectable continual learning memory substrate,
not a model zoo, not a chip, not an LLM memory SaaS, and not a music model.
```

## Relationship To Our Design Decisions

The web scan supports the earlier CINM decisions:

- Continual learning should remain central.
- CPU + RAM should be first for development because it gives maximum freedom,
  debuggability, and evidence capture.
- CUDA is useful later for batch similarity search, hypervector operations, and
  stress testing, but should not become the conceptual dependency.
- In-memory should first mean "co-locate state, update, and readout in the
  software design"; analog/neuromorphic hardware is a later mapping.
- HDC/VSA is a strong representation candidate, but CINM should not collapse
  into "just HDC".
- Fast/slow memory is important: episodic additions must work immediately, but
  consolidation must prevent unbounded growth and interference.
- Provenance is not decoration. It is what keeps the system inspectable.

## What CINM Should Avoid

CINM should avoid becoming any of these:

- a transformer alternative by rhetoric only
- a vague neuromorphic branding project
- a pile of HDC demos without a memory lifecycle
- a hardware-first project blocked on unavailable chips
- an LLM agent-memory clone
- a music-specific model too early
- a benchmark-free theory document

The project should stay strict:

```text
Every learned change should be explainable as:
event -> activated cells -> local update -> changed readout -> evidence link
```

## Practical Positioning

If asked "what is this?", the clean answer is:

```text
CINM is a small continual-learning memory substrate.
It studies local, bounded updates over inspectable memory cells.
It borrows from HDC/VSA, neuromorphic locality, and in-memory computing,
but starts as a CPU/RAM-first mathematical and systems experiment.
```

If asked "does it already exist?", the clean answer is:

```text
Pieces exist. The exact combination was not found in the first scan.
The nearest pieces are HDC/VSA libraries, in-memory HDC research,
neuromorphic continual-learning systems, edge neuromorphic chips,
compute-in-memory inference accelerators, and LLM agent-memory products.
```

## Research Risks

The main risks are:

- HDC-style representations may be too weak for complex tasks without a learned
  feature front-end.
- Local updates may fail on delayed credit assignment.
- Consolidation may become the real hard problem.
- Inspectability may reduce performance if every state change must be bounded
  and attributable.
- CPU/RAM simulations of "in-memory" locality may not translate cleanly to
  analog or neuromorphic hardware.
- GPU acceleration may tempt the project back toward dense batch thinking.

These risks are acceptable because they are testable.

## Recommended Next Research Steps

1. Define a tiny task suite independent of music.
2. Implement only enough Python to simulate cells, events, local updates, and
   consolidation.
3. Compare against a simple online baseline, not a large neural model.
4. Track exact event evidence for every changed cell.
5. Measure capacity, interference, forgetting, adaptation latency, and memory
   growth.
6. Keep HDC/VSA as one representation backend, not the whole theory.
7. Add a C/Rust hot-loop only after the math is visible.
8. Add CUDA only for specific operations such as batched similarity search or
   hypervector operations.
9. Revisit hardware mapping only after the software substrate has stable
   invariants.

## Source Registry

HDC / VSA:

- https://arxiv.org/abs/2111.06077
- https://arxiv.org/abs/2112.15424
- https://github.com/hyperdimensional-computing/torchhd
- https://arxiv.org/abs/2205.09208

In-memory / HDC / edge continual learning:

- https://arxiv.org/abs/1906.01548
- https://arxiv.org/abs/2604.21280
- https://arxiv.org/abs/2605.24788
- https://arxiv.org/abs/2110.00214

Neuromorphic continual learning:

- https://arxiv.org/abs/2410.09218
- https://arxiv.org/abs/2511.01553
- https://arxiv.org/abs/2605.28387
- https://arxiv.org/abs/2509.05858

Neuromorphic hardware and software ecosystems:

- https://www.intel.com/content/www/us/en/research/neuromorphic-computing.html
- https://lava-nc.org/
- https://brainchip.com/
- https://www.innatera.com/
- https://www.synsense.ai/

Compute-in-memory / memory-centric inference hardware:

- https://www.d-matrix.ai/
- https://mythic.ai/
- https://www.enchargeai.com/

Agent memory:

- https://www.getzep.com/
- https://mem0.ai/

Continual-learning tooling:

- https://avalanche.continualai.org/

