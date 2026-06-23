# 17. Implementation Backlog L-Q (Drum Vertical MVP)

This continues the lettered backlog of `docs/13` (sections G-K). It is the
implementation set for the drum-first vertical (D014), sequenced toward the two
milestones in doc 16. It is research infrastructure and a documentation artifact —
no code is written by this document.

Each section follows the doc-13 shape — *Goal / Planned surface / Invariant /
Gate / Acceptance spec* — and is tagged with the milestone(s) it serves. "Planned
surface" replaces doc-13's "Implemented surface": nothing here exists yet. Gates
name *planned* `make` targets so the eventual kernel keeps one runnable entrypoint
per section, consistent with G-K.

Reuse over rebuild (D015): each section names the existing asset it wires, not a
new build. The new surface is deliberately thin (doc 15, "What Is New vs Reused").

## L. Drum Adapter And Evidence Bridge  [M1]

Goal: turn the existing ranker preference stream into CIM events without inventing
a new format. This is the single domain-aware ingestion seam.

Planned surface:

- reader for `aig_ranker` JSONL (`pc4ms.aig_ranker_preference.v1`)
- mapping `winner {a,b,tie,neither}` + `confidence` -> a `cinm_event`
  `{seq, type, key, reward, dphi[NFEAT]}`
- `phase {train, slate_eval}` -> CIM training window vs validation window
- idempotent ingestion: re-reading the same JSONL yields the same event sequence

Invariant:

```text
the adapter is the only drum-aware layer feeding the kernel
re-ingesting the same JSONL is deterministic (same seq -> same events)
tie / neither map to a defined, non-silent update policy
```

Gate (planned):

```sh
make run-drum-adapter
```

Acceptance spec:

```text
- a fixed sample JSONL ingests to a fixed cinm_event sequence (golden vector)
- train vs slate_eval events are separable downstream
- a duplicate trial (same bundle/trial/rater) does not double-apply
- the kernel sees only phi/dphi + reward, never a drum symbol
```

## M. Groove Representation And Context  [M1]

Goal: define `phi` (candidate features) and the addressing context (key) from ADG
grooves and drummer law axes, keeping the kernel domain-neutral behind the seam.

Planned surface:

- ADG groove attributes (gesture `strength/body/transient/presence/recovery`,
  transform `amount`, kit models) -> `phi[NFEAT]` under a pinned `feature_schema_id`
- drummer law-axis coordinate -> CIM `key` / context region
- optional HDC encoding of the context via the J-lane / `taste-hdc` as one backend

Invariant:

```text
phi is a pure function of (groove, context) under a fixed feature_schema_id
HDC encoding is one optional backend, not the theory (doc 09)
the scalar phi path is the default and must work without HDC
```

Gate (planned):

```sh
make run-drum-features
```

Acceptance spec:

```text
- the same groove+context yields byte-identical phi across runs
- a feature_schema_id change is explicit and versioned (no silent drift)
- |A_t| << |M|: a context activates a small cell set, not the whole map
- the HDC path, when enabled, agrees with the scalar path on ranking order
```

## N. MAP-Elites Groove Generator  [M2]

Goal: generate diverse candidate grooves across musical niches, with the M1 taste
model as the selection pressure. Re-implements the `neuro-simbolic` archive shape
(D015), not the Go crate.

Planned surface:

- niche axes from `EVALUATION_AXES.md` (e.g. density x syncopation x genre-feel x
  fill-rate x swing)
- archive: elites per niche, bounded population, lineage, history of discarded
- quarantine for rejected grooves (negative evidence)
- groove mutation/crossover producing valid ADG

Invariant:

```text
population is bounded (capacity gate); no unbounded archive growth
the archive keeps local winners per niche, not only a global best
a rejected groove is quarantined as evidence, not deleted
every generated groove lowers to valid ADG
```

Gate (planned):

```sh
make run-groove-archive
```

Acceptance spec:

```text
- niches stay populated under selection (no collapse to one groove)
- population never exceeds MaxPopulation; eviction is the documented policy
- a groove's lineage (parents, niche, fitness) is reconstructable
- quarantined grooves are retrievable with their rejection reason
```

## O. Taste-Learning Loop Wiring  [M1]

Goal: wire Loop 1 end to end on the existing kernel: address -> score -> bounded
adaptive update -> readout (re-rank) -> reversible undo -> explanation.

Planned surface:

- bind L (events) + M (phi/key) to `cinm` address/score/`cinm_update_adaptive`/decay
- readout = re-rank candidate grooves by `cinm_score`
- "undo last N picks" via `cinm_snapshot`/`cinm_restore` (or a delta log)
- per-decision explanation: active cells, contributions, provenance

Invariant:

```text
predict before learning (existing task rule) holds
an undo of N picks restores the pre-pick taste state exactly
every readout can name its active cells and their evidence
```

Gate (planned):

```sh
make run-taste-loop
```

Acceptance spec:

```text
- after one pick, the next relevant ranking reflects it (immediate learning, H3)
- undo-N restores byte-exact prior map state (reuses run-txn machinery)
- 100% of readouts carry an active-cell explanation
- learning beats a simple online baseline on a held-out drum slate (doc 18)
```

Implemented (`run-taste-loop`, green on synthetic drum-shaped data): context-addressed taste
beats a context-blind baseline (0.920 vs 0.565), old-context retention holds (0.905→0.910 vs
blind 0.912→0.597), and undo-N is byte-exact (R4 window). Real preference data is section L.

## P. Godel-Darwin Self-Adaptation  [M1]

Goal: Loop 2 — let CIM propose small, closed adaptations to its own machinery,
gated on held-out preference, reversible and archived. This is the doc-10 transfer
made concrete.

Planned surface:

- `CINM_OP_*` operator language (small, closed): step rule, plasticity schedule,
  decay rule, consolidation rule, addressing alias, capacity/pruning policy
- candidate record: `{candidate_id, parent_id, operator_kind, params,
  created_at_event, status}`
- speculative cycle: snapshot/begin -> apply -> evaluate on slate_eval -> commit /
  rollback / quarantine
- operator-level MAP-Elites archive (reuses N's shape at the operator level)

Invariant:

```text
the operator language is finite and closed (no arbitrary C callbacks, doc 10)
a candidate may change working state but never the evaluator (anti-hack)
every candidate has a valid rollback path before it is applied
failed candidates go to quarantine as negative evidence
```

Gate (planned):

```sh
make run-self-adapt
```

Acceptance spec:

```text
- a regressing operator is rolled back byte-exact and quarantined with a reason
- the proposer cannot edit the held-out evaluator or grade itself (catch trials)
- commit happens iff all hard gates pass (bounds, margin, capacity, rollback)
- the decision ledger replays deterministically from events + seed
```

Implemented (`run-self-adapt`, green on synthetic drum-shaped data): the operator self-tunes
the **decay** (forgetting) rate on a drifting taste (best decay 0.930, held-out 0.871 vs 0.779
no-forgetting), scored on held-out, with a MAP-Elites archive (`cinm_selfadapt`), ledger
receipts, and byte-exact rollback of rejected operators. Operator choice — **decay, not the
learning rate**: with the constant-step update ranking is scale-invariant in eta (doc 06), so
eta is an empty knob; decay-on-drift is the operator that measurably matters.

## Q. Gate, Eval Harness, And Render  [M1 eval/gate; M2 render]

Goal: the decision gate, the evaluation harness, and the M2 render path. Reuses the
drummer critic and golden set as the gate; the ranker catch trials as integrity
checks.

Planned surface:

- gate: boundedness + capacity + rollback-availability + anti-hack, plus the
  drummer `ANTI_PATTERNS` critic and `GOLDEN_SET` held-out check
- eval harness: named runs under `runs/cim-drum-*` (doc 18), metrics + summary
- [M2] render: ADG -> `aig-pc4-midi` lossy MIDI + `*.midi-loss-map`; audition via
  EGMD / Virtuosity / Karoryfer backends

Invariant:

```text
accept iff all hard gates pass (doc 10 gate shape)
the gate is held-out: ANTI_PATTERNS + GOLDEN_SET + slate_eval, not train data
[M2] ADG stays semantic authority; MIDI is a lossy rendering with its loss map
```

Gate (planned):

```sh
make run-drum-gate      # M1
make run-drum-render    # M2
```

Acceptance spec:

```text
M1:
- a candidate that fails any hard gate is rejected with a reason_code
- a candidate that passes ANTI_PATTERNS but fails GOLDEN_SET held-out is rejected
- every run writes config.toml + metrics.json + decisions.jsonl + summary.md
M2:
- blind A/B prefers taste-adapted grooves over the generic baseline (doc 18 margin)
- every exported MIDI ships a *.midi-loss-map
- the render path issues no runtime-authority calls (doc 19)
```

## Sequencing

```text
M1   L -> M -> O          (taste loop on real drum evidence)
            -> P          (self-adaptation, needs L/M/O)
            -> Q (eval/gate)
M2   N -> Q (render)      (begins only after the M1 gate passes, D016)
```

## Claim Boundary (continues doc 13 K, see doc 19)

```text
- strictly in-memory (D013); no durable store
- a drum case study, not the domain-neutral substrate result (doc 19)
- no runtime authority; review-gated proposals only
- proven != smoke != measured != sounds-good (doc 21)
```
