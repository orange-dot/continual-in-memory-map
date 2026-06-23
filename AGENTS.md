# AGENTS.md — Drum Vertical, Implementation Handoff

Orientation for an agent doing the **phased implementation** of the drum-first
vertical (decision D014). This is a map and a rulebook; the spec is the doc set
under `docs/`. Read this first, then the docs it points to. No code exists yet for
the vertical — the substrate core (`experiments/c-kernel/`) does.

## Scope in one paragraph

Build a taste-driven MIDI drum generator as one vertical, with CIM concepts (cells,
event log, reversible transaction, archive, gate) as its internal structure (D014).
It is **staged**: prove the substrate on a drum task first (M1), make it audible
second (M2) — M2 does not begin until the M1 gate passes (D016). It is strictly
**in-memory** (D013, no durable store), produces **review-gated proposals with no
runtime authority** (doc 19), and **reuses existing lab assets rather than
rebuilding** them (D015). The point is wiring, not green-field work.

## Confirm before writing any code

- **D017 language line** (the one open knob). Recommended: CIM core stays **C**;
  vertical glue + eval in **Python** (reuses the Python ranker, `taste-hdc`, EGMD
  tooling); MAP-Elites re-implemented in C or Python; Rust/aig-engine deferred.
  Confirm this with the user at kickoff.

## Read order (for an implementer)

```text
1. docs/15-drum-vertical-architecture.md   the whole picture + integration map
2. docs/16-mvp-milestones.md               M1/M2 + acceptance gates (= done)
3. docs/17-implementation-backlog-l-q.md   the work items, each with its own gate
4. docs/18-evaluation-and-baseline-plan.md how to measure (baseline, metrics, runs)
5. docs/19-claim-boundaries-and-authority.md  the rules you must not cross
6. docs/21-glossary.md                      three vocabularies; keep the loops distinct
7. docs/20-risk-register.md                 what to watch and which gate catches it
```

Background (the substrate you build on):

```text
docs/05-decisions.md            D001 (neutral), D011 (C-first), D013 (RAM-only),
                                D014–D017 (this vertical)
docs/10-neurosymbolic-transfer.md   the Godel-Darwin skeleton behind section P
docs/12-memory-model.md         L1->RAM execution-locality model
docs/13-implementation-backlog-g-k.md   the existing, already-built backlog (G–K)
docs/FORMAL_PROOF.md + docs/isabelle/   proof posture (abstract model only)
experiments/c-kernel/README.md  the current kernel surface + green gates
```

## File map (what each new doc gives you)

```text
docs/15  architecture   components, the two loops, MAP-Elites dual role,
                        integration map with exact source paths + schema ids
docs/16  milestones     M1 (substrate-proof, 8-check gate) and M2 (audible, 5-check
                        gate); the milestone -> backlog map
docs/17  backlog L–Q    L adapter · M representation · N groove generator ·
                        O taste loop · P self-adaptation · Q gate/eval/render.
                        Each: Goal / Planned surface / Invariant / Gate / Acceptance
docs/18  evaluation     E-GMD baseline+bootstrap, synthetic->review arc, held-out
                        discipline, metrics, named-run layout under runs/
docs/19  boundaries     what the MVP claims / does not; authority boundary; the
                        D001 tension owned; neutral-substrate extraction (future)
docs/20  risks          R1 contamination … R9 loops-conflated, each with a catcher
docs/21  glossary       CIM ↔ neuro-symbolic ↔ musician terms; loop + claim words
```

## Substrate you build on (do not rebuild)

`experiments/c-kernel/` is the existing core: `cinm` (address, score, adaptive
update, decay, snapshot/restore, speculative transaction, equality) + `cinm_log`
(append-only event log + strict replay). Its gates are **green today** — confirm
before and after your work:

```sh
make -C experiments/c-kernel run-core-check run run-cmp run-learning run-txn \
  run-replay run-log-invariants run-memory run-hdc native vec-report run-bench
```

M1 inherits `run-txn`, `run-replay`, `run-log-invariants` directly (gates 4–5 in
doc 16) — do not re-implement reversibility or replay.

## Reuse map (wire these, do not reimplement — D015)

```text
event stream / pairwise / held-out
  ../../systems/aig-engine/tools/aig_ranker_preferences.py
    schemas: pc4ms.aig_ranker_preference.v1, aig.ranker_phrase_preference_bundle.v2
    -> section L (adapter): JSONL -> cinm_event; phase train/slate_eval -> windows

groove IR + MIDI + corpus
  ../../systems/aig-engine/                      aig-core, aig-adg, aig-pc4-midi
  ../../systems/aig-engine/tools/aig_egmd_corpus.py        baseline + bootstrap
  ../../systems/aig-engine/tools/aig_external_drum_corpus.py  audition material
    -> sections M (phi) and Q (render)

MAP-Elites shape (re-implement the shape, NOT a Go dependency — doc 10)
  ../../research/neuro-simbolic/internal/evolution/{archive,variant,lineage}.go
    -> section N (grooves) and section P (operators)

gate / critic / held-out / contexts
  ../../systems/pc4-microkit-studio/compositions/support/player_models/drummer/
    eval/ANTI_PATTERNS.md, eval/EVALUATION_AXES.md, goldens/GOLDEN_SET_SPEC.md,
    language/NORMALIZED_LAW_AXES.md, runtime/OUTPUT_CONTRACT.md
  ../../research/music-specific-ai-concepts/DRUMMER_BASSIST_V0.md
    -> sections M (contexts/niches) and Q (gate)

representation backend (optional)
  experiments/taste-hdc/   the HDC seed (J-lane); one backend, never the theory
```

## Phase plan

```text
M1 substrate-proof   L -> M -> O -> P -> Q(eval/gate)
   gate: doc 16 M1 (8 checks). Inherits run-txn / run-replay / run-log-invariants.
        |
        v  M1 gate passes (D016)
M2 audible           N -> Q(render)
   gate: doc 16 M2 (5 checks).
```

Per-section planned gates (one runnable entrypoint each, doc 17):

```text
L run-drum-adapter   M run-drum-features   N run-groove-archive
O run-taste-loop     P run-self-adapt      Q run-drum-gate [M1] / run-drum-render [M2]
```

## Hard rules (violating these is drift, not progress)

```text
- proven != smoke != measured != sounds-good (doc 21). Never claim "verified"
  beyond what docs/isabelle actually checks.
- reuse over rebuild (D015). The "new" surface is thin (doc 15). If it balloons,
  re-scope (risk R8) — do not quietly rebuild an existing asset.
- two loops stay distinct (R9): a "candidate" is a GROOVE (Loop 1) or an OPERATOR
  (Loop 2), never silently both.
- the phi seam: the kernel sees phi[NFEAT], not drums. Only the adapter (L/M) is
  drum-aware. This keeps the doc 07 bridge rule even inside a drum vertical.
- no runtime authority (doc 19): no pc4_transport, no composition.json, no live
  state mutation. ADG is semantic authority; MIDI is lossy + ships a loss map.
- proposer != grader (doc 10): score on held-out slate_eval; honor catch trials;
  the candidate never touches the evaluator.
- RAM-only (D013): no durable store. SSD/Linux layer stays in docs/future/.
```

## Evidence discipline

Every evaluation is a **named run** under `runs/cim-drum-*` with
`config.toml + events.jsonl + metrics.json + decisions.jsonl + summary.md`
(doc 18). Follow the `experiment-evidence-operator` posture. Any coverage cap
(sampling, top-N, no-retry) must be stated in `summary.md` — silent truncation
reads as "covered everything" when it did not.

## Definition of done

```text
M1   doc 16 M1 gate (all 8) PASS on >=1 drum-shaped task, vs a simple online
     baseline, with the forgetting probe passing.
M2   doc 16 M2 gate (all 5): blind A/B prefers taste-adapted grooves over the
     generic baseline; reversible ("undo N picks"); explainable; zero runtime
     authority.
```

## Useful skills for this work

`experiment-evidence-operator` (runs/baselines), `formal-proof-readiness` (keep
proof claims honest), `dsp-rust-hot-path` / `sel4-c-execution-optimizer` (the
c-kernel hot loop), `adr-workflow` (if a new decision is needed),
`project-smoke-runner` (the kernel gates).
