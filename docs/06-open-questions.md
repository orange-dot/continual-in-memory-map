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

- Exact symbolic keys first, or continuous nearest-neighbor keys first?
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
- Can consolidation be reversible?
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

