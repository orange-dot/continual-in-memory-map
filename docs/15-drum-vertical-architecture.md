# 15. Drum Vertical Architecture

This is the first applied CIM architecture (decision D014). It maps the
domain-neutral architecture of doc 02 onto one domain — a taste-driven MIDI drum
generator — built as a single vertical with CIM concepts as its internal
structure.

The domain-neutral contract in doc 02 still governs the substrate. This document
does not replace it; it instantiates it for drums.

## The Recognition

The lab already built all three layers of this system, in three different
vocabularies. The vertical is mostly wiring, not green-field work.

```text
CIM substrate         neuro-symbolic          musician / drummer
-------------         --------------          ------------------
event log             audit trail             append-only preference JSONL
candidate             variant                 groove proposal
local update          mutation                taste shift
gate                  gate                     anti-pattern critic + golden set
archive               MAP-Elites archive      golden set / kept takes
held-out window       validation set          slate_eval phase
proposer != grader    proposer != grader      human is final judge
```

The shared invariant across all three is the CIM invariant:

```text
proposer != grader
human / evidence gates
everything reversible and traceable
```

(See doc 21 for the full term-by-term glossary.)

## Components

```text
EVIDENCE   aig_ranker JSONL (pairwise + confidence + phase)      reuse (Python)
   |
REPR.      ADG groove -> phi[NFEAT]   (optional HDC encode)      reuse aig-core/adg
   |
SUBSTRATE  CIM c-kernel: address -> taste cells -> bounded       reuse cinm/cinm_log
           adaptive update -> readout (re-rank) -> txn -> replay
   |
DARWIN     MAP-Elites over grooves AND over CINM_OP_* operators  reimplement shape only
           niches: density x syncopation x genre x fill x swing
   |
GATE       bounds + capacity + rollback + anti-hack              reuse drummer eval
           + drummer ANTI_PATTERNS + GOLDEN_SET + HUMAN gate
   |
OUTPUT     ADG (semantic authority) -> aig-pc4-midi lossy -> MIDI  reuse aig-pc4-midi
```

The substrate row is the existing kernel under `experiments/c-kernel/`. The
vertical does not rebuild it; it wires drum evidence and a generator around it.

## The Two Loops

The architecture has two adaptation loops. The docs and the code must keep them
distinct.

```text
Loop 1 (taste)
  pairwise pick -> CIM cell update -> re-rank / bias grooves
  candidates are GROOVES

Loop 2 (self-adaptation, the Godel-Darwin tier, doc 10)
  CINM_OP_* change -> speculative apply -> evaluate on slate_eval held-out
                   -> commit / rollback -> archive
  candidates are OPERATORS
```

Loop 1 is the primary product behavior: the system learns the composer's taste.
Loop 2 is CIM tuning its own update/plasticity/addressing rules, gated on held-out
preference. Loop 1 is required for both milestones; Loop 2 is an M1 substrate-proof
target (it is where reversibility and anti-hack earn their keep).

## MAP-Elites Plays Both Roles

The same quality-diversity archive serves both loops at different levels:

```text
Loop 1 level   archive of GROOVES        -> generates diverse candidates,
                                            never collapses to one groove
Loop 2 level   archive of OPERATORS      -> keeps local-winner adaptation rules
                                            per behavior niche
```

Both levels keep local winners per niche, bound population (capacity gate), and
quarantine failures as negative evidence. This is the `neuro-simbolic`
`internal/evolution` shape (Niche / NicheBounds / VariantRecord, ElitePerNiche /
MaxPopulation, `enforcePopulationLimit`, `IsBetterThan`, lineage), re-implemented
in the vertical's own language — not a dependency on the Go crate (D015, doc 10).

## Integration Map (reuse, not rebuild)

Exact sources, with the contracts the vertical depends on. Schema ids are pinned
here and checked during verification.

```text
Event stream / pairwise / held-out
  workspace/systems/aig-engine/tools/aig_ranker_preferences.py
    record schema   pc4ms.aig_ranker_preference.v1
    bundle schema   aig.ranker_phrase_preference_bundle.v2
    fields used     winner {a,b,tie,neither}, confidence, reason_tags,
                    phase {train, slate_eval}, feature_schema_id
    catch trials    identical_aa -> tie, degraded -> expected_winner

Groove IR + MIDI export + corpus
  workspace/systems/aig-engine/
    aig-core        dialect-neutral IR, GesturePacket, AtomSpec
    aig-adg         ADG TOML groove dialect (kit models, transforms, gestures)
    aig-pc4-midi    MIDI 1.0 export + ADG->MIDI loss map
    tools/aig_egmd_corpus.py             E-GMD MIDI corpus (baseline)
    tools/aig_external_drum_corpus.py    CC0 sample material (audition)

MAP-Elites shape (re-implement, do not depend on)
  workspace/research/neuro-simbolic/internal/evolution/{archive,variant,lineage}.go

Gate / critic / held-out / contexts
  workspace/systems/pc4-microkit-studio/compositions/support/player_models/drummer/
    language/NORMALIZED_LAW_AXES.md     context coordinates
    eval/EVALUATION_AXES.md             niche-axis candidates
    eval/ANTI_PATTERNS.md              anti-hack / sanity gate
    goldens/GOLDEN_SET_SPEC.md         held-out validator
    runtime/OUTPUT_CONTRACT.md         proposal output contract
  workspace/research/music-specific-ai-concepts/DRUMMER_BASSIST_V0.md

Substrate core (unchanged)
  workspace/research/continual-in-memory-map/experiments/c-kernel/
    cinm.{c,h}      address, score, margin, adaptive update, decay,
                    snapshot/restore, speculative transaction, equality
    cinm_log.{c,h}  in-RAM append-only event log + strict replay
```

## Groove As Candidate

An ADG groove is a TOML document: `bpm`, `bars`, `seed`, per-role kit models
(e.g. `kick=anchor`, `snare=ghost`, `hat=breath`), `transforms` (e.g.
`nervousness` with an `amount`), and `gestures` (`role`, `kind`, `beat`,
`duration`, `strength`, `body`, `transient`, `presence`, `recovery`).

The feature vector the substrate scores is derived from these attributes:

```text
phi(groove, context) = features over gesture/transform attributes
                       under the bundle's feature_schema_id
```

`phi` keeps the substrate domain-shaped but not domain-bound: the kernel only sees
`phi[NFEAT]`, not drums. This is the doc 07 bridge rule ("one adapter around the
substrate") honored even inside a drum-first vertical.

## Context As Law-Axis Coordinate

The CIM addressing context is a musical context expressed in the drummer's
normalized law axes (density, fill policy, silence policy, accent logic, pulse,
section memory, relationship to bass). These become the `key`/context region a
taste cell activates on. The niche axes for the MAP-Elites groove archive are
drawn from the same vocabulary (`EVALUATION_AXES.md`); the exact axis set is a
tracked open question (doc 06), not a fixed claim.

## What Is New vs Reused

```text
Reused      ranker JSONL, ADG IR, MIDI export, EGMD corpus, drummer gate/golden,
            the C kernel (cinm + cinm_log + txn + replay), the Isabelle lane shape

New (thin)  drum adapter (JSONL -> cinm_event; ADG -> phi),
            niche-axis mapping, MAP-Elites groove/operator archive (re-impl shape),
            CINM_OP_* operator language, eval harness wiring, M2 render path
```

The "new" column is deliberately small. If it grows large, the vertical has
drifted from D015 (reuse over rebuild) and should be re-scoped.

## Authority And Output Boundary

The vertical produces review-gated proposals, not runtime authority. ADG is the
semantic authority; MIDI export is the lossy last mile (it carries notes, ticks,
channel, velocity — not the full ADG surface — so a `*.midi-loss-map` ships with
it). The generator must not call `pc4_transport`, write `composition.json`, or
mutate runtime state. The full boundary is doc 19.
