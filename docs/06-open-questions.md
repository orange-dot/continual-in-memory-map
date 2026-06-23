# 06. Open Questions

## Mathematical Questions

- What is the best first activation function?
- Should cells store vectors, low-rank adapters, counters, or small rules?
- How should confidence be updated under repeated weak evidence?
- How should contradictory evidence be represented?
- When does a conflict split a cell instead of lowering confidence?
- What should plasticity mean after consolidation?
- How much eligibility trace is useful before it becomes hidden sequence memory?

## Addressing Questions

- Exact symbolic keys first, or continuous nearest-neighbor keys first? (This now
  also gates schema/merge consolidation — R3.5, doc 22: prototype compression needs
  nearest-neighbour addressing; exact keys cap consolidation at evict + freeze.)
- Should addressing be learned or fixed?
- How many cells should activate per event?
- How should novelty be detected?
- What prevents over-fragmentation into too many cells?
- What prevents over-generalization into cells that match too broadly?

## Learning Questions

- Is pairwise feedback enough for the first toy tasks?
- How should ties and "neither" labels affect local state?
- Can delayed feedback be handled without global credit assignment?
- What is the smallest update rule that still works?
- How should update magnitude be bounded?
- What should happen when feedback is noisy or adversarial?

## Forgetting And Consolidation Questions

- Which state should decay first: weights, confidence, eligibility, or
  activation priority?
- How should the system distinguish useful forgetting from harmful forgetting?
  (Partial answer, D018 / doc 22 R3: useful = evicting dead contexts that decay has
  already emptied; harmful = caught by the consolidation sub-probe, doc 18.)
- ~~Can consolidation be reversible?~~ **Resolved (D018): no — by design.
  Consolidation is genuinely lossy (as aggressive as a real learner needs);
  reversibility is guaranteed only within the retention window, and each
  consolidation leaves an audit receipt. Decay is the soft, in-window forgetting;
  consolidation is the hard, irreversible-past-boundary commit.**
- What makes a learned pattern stable enough to promote?
- Should consolidation produce human-readable rules, compact cells, or both?

## Systems Questions

- What memory layout best supports sparse local access?
- Is CPU cache behavior more important than arithmetic throughput?
- Which operations, if any, benefit from CUDA?
- What is the cost of explanation?
- How large can the map grow before retrieval dominates?
- What snapshot format balances speed and inspectability? (In-RAM today a
  snapshot is a whole-map copy; an on-disk format is deferred — D013.)

## Evaluation Questions

- Which toy task reveals catastrophic forgetting most clearly?
- What baseline is fair?
- How should immediate learning be measured?
- How should old-context retention be measured?
- How should explanation quality be scored without turning it into subjective
  prose?

## Philosophy And Claim Boundary Questions

- ~~Is "neuromorphic" the right term, or does it invite confusion?~~
  **Resolved (D013): no — the label is retired; the locality principle (D004)
  stands. The project is "Continual In-Memory Map".**
- Should the project emphasize "memory substrate" more than "model"?
- How much biological analogy is useful before it becomes misleading?
- What would count as a meaningful negative result?
- What would justify applying the substrate to a real domain?

## Drum Vertical Questions (D014)

The drum-first vertical (docs 15–21) is the answer-in-progress to "what would
justify applying the substrate to a real domain?". It will test several questions
above on a drum-shaped task:

- Addressing: do exact symbolic law-axis keys suffice before nearest-neighbor keys?
- Learning: is pairwise feedback enough for the first drum task? (H1, doc 03)
- Evaluation: what baseline is fair? (a simple online learner, doc 18)
- Forgetting: which drum task reveals catastrophic forgetting most clearly?
  (the fill-heavy vs sparse probe, doc 18)

New questions the vertical raises:

- Which niche axes make MAP-Elites diversity musically meaningful, not just varied?
- Is HDC groove encoding stronger or weaker than the scalar phi path?
- How much of the kernel path is drum-agnostic enough to extract later (doc 19)?
- Can tie / neither preference labels update state without oscillation?

