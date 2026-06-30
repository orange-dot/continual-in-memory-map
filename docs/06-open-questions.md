# 06. Open Questions

## Mathematical Questions

- What is the best first activation function?
- Should cells store vectors, low-rank adapters, counters, or small rules?
- How should confidence be updated under repeated weak evidence?
- How should contradictory evidence be represented?
- ~~When does a conflict split a cell instead of lowering confidence?~~
  **Resolved (D019, `cim-learnq-split-v1`): when the dissent has a consistent context
  direction.** A cell splits when its broad-contradiction rate crosses 1/3 *and*
  `‖conflict_dir‖² ≥ split_dir_floor2` (the EWMA of `ctx − proto` on contradiction is
  address-separable). If the rate is high but the direction collapses to ~0 — the two
  sub-populations are *aliased* (same address, opposite reward) — the split is
  **suppressed** and the cell falls back to lowering confidence / route-by-hidden-context
  (`01-theory.md:179`). Address-splitting separates only what differs by address.
- What should plasticity mean after consolidation?
- How much eligibility trace is useful before it becomes hidden sequence memory?

## Addressing Questions

- ~~Exact symbolic keys first, or continuous nearest-neighbor keys first?~~
  **Resolved (D019): both — exact-symbolic keys stay the default; continuous
  nearest-neighbour / prototype addressing (`cinm_address_nn`) is added alongside and
  unblocks R3.5 merge consolidation (doc 22). Radius 0 + one-hot context degenerates
  to exact addressing, so NN strictly generalizes the exact path.**
- Should addressing be learned or fixed? (Partial answer, D019: the prototype is
  birth-fixed — set to the first context that allocates a cell, moved only by merge.
  A learned/drifting prototype (EMA) is a noted future knob.)
- How many cells should activate per event?
- How should novelty be detected? (Partial answer, D019: a squared-L2 distance
  threshold — a context beyond the novelty radius of every prototype allocates a new
  cell; within it, reuses. libm-free.)
- What prevents over-fragmentation into too many cells? (Partial answer, D019: the
  novelty radius clusters jittered contexts onto one prototype, and merge
  consolidation (R3.5) folds near-duplicate prototypes; `run-nn-address` measures it —
  NN holds a handful of cells where exact keys fragment to the cell ceiling.)
- What prevents over-generalization into cells that match too broadly? (Partial,
  D019: bounded by the novelty radius — too large a radius over-generalizes; the
  radius is the open tuning knob, and the doc-03 "addressing dominates" negative
  result is the watch.)

## Learning Questions

- Is pairwise feedback enough for the first toy tasks?
- How should ties and "neither" labels affect local state?
- Can delayed feedback be handled without global credit assignment?
- What is the smallest update rule that still works?
- How should update magnitude be bounded?
- What should happen when feedback is noisy or adversarial?
- **Insight (run-self-adapt, P):** with the constant-step pairwise update, ranking is
  *scale-invariant in the learning rate* `eta` — it scales `w` but never the sign of a
  comparison, until clamping — so `eta` is an empty self-adaptation knob on a stationary
  task. The operator that measurably matters is the **decay (forgetting) rate on a
  non-stationary task**: matching forgetting to drift changes held-out quality. This is why
  the self-adaptation loop (P) self-tunes decay, not eta.

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
- How large can the map grow before retrieval dominates? **(Measured, decision A.)**
  With the linear scan, retrieval dominates from the *smallest* map (`crossover_n =
  256`; `cim-sys-scale-v1`) — scoring is flat ~4–6 ns/cell, addressing is the wall.
  Phase 3a then indexed the exact-key scan: `cinm_address` becomes ~O(1) (~47 ns at
  1M vs 443 µs scanned), byte-exact, so an *exact-key* map now grows to ≥ 1M cells
  without retrieval dominating (`cim-sys-scale-v2`). The nearest-neighbour path
  (`cinm_address_nn`) is now indexed too (`cinm_nn_index`, a static KD-tree,
  `cim-sys-scale-v3`) and byte-exact, but only modestly: at NFEAT=8 the exact tree is
  backtracking-bound, so it reduces NN cost ~8.3× at 1M (and is *slower* than the scan
  below ~2k cells) rather than flattening it like the exact-key hash. The
  exact-key/NN asymmetry is now measured, not assumed.
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

