# 16. MVP Milestones

The MVP is staged (decision D016): prove the substrate first, make it audible
second. The two milestones have separate, explicit acceptance gates. M2 work does
not begin until the M1 gate passes.

```text
M1 substrate-proof    CIM invariants hold on a drum-shaped task (numbers)
        |
        v  gate passes
M2 audible generator  a taste-driven groove generator you can hear (sound)
```

Each backlog section (doc 17) is tagged with the milestone(s) it serves. The
evaluation method, baseline, and metrics live in doc 18; the claim boundaries that
fence both milestones live in doc 19.

## M1 — Substrate-Proof On A Drum-Shaped Task

Goal: show that CIM's invariants still hold when the task is drum preference rather
than a neutral toy task, reusing the existing C kernel. M1 produces numbers and
PASS/FAIL gates, not audio.

### Setup

```text
context     drummer law-axis coordinate (density, fill policy, accent, pulse, ...)
candidate   groove feature vector phi[NFEAT] (from feature_schema_id)
feedback    pairwise from aig_ranker JSONL: synthetic bootstrap -> real review
windows     train phase (active) vs slate_eval phase (held-out gate)
```

### What M1 exercises

- Loop 1 taste learning: pairwise pick -> cell update -> re-rank.
- Loop 2 self-adaptation: `CINM_OP_*` candidate -> speculative apply -> slate_eval
  gate -> commit / rollback -> operator archive + quarantine.
- The existing reversibility and replay machinery on real drum evidence.

### M1 acceptance gate

All of the following must hold; any FAIL blocks M2.

```text
1. learning      beats a simple online baseline on the drum task
                 (win-rate up, regret down; method in doc 18)
2. retention     old-context performance survives learning a new context
3. forgetting    the catastrophic-forgetting probe passes (doc 18)
4. reversibility every rejected adaptation restores byte-exact prior state
                 (existing: make run-txn)
5. replay        snapshot+replay and log-only replay reconstruct committed state
                 (existing: make run-replay, make run-log-invariants)
6. evidence      every committed change traces to the events that caused it
7. anti-hack     the proposer cannot grade itself; catch trials are honored
8. bounds        cells <= MAX_CELLS, weights within [-W_MAX, W_MAX], clock monotonic
```

Gates 4–5 are already green in the kernel and are inherited, not rebuilt. Gates
1–3 and 6–8 are the new M1 work (backlog L, M, O, P, Q-eval).

### M1 is explicitly NOT

- not audible (no render, no MIDI required to pass M1)
- not a musical-quality claim
- not a domain-neutral substrate claim (that is the later extraction, doc 19)

## M2 — Audible Generator

Goal: produce drums you can hear that demonstrably track the composer's taste,
with reversibility and explanation intact. M2 begins only after the M1 gate passes.

### What M2 adds

- MAP-Elites generates diverse grooves across musical niches; the M1 taste model is
  the selection pressure (Loop 1 at the groove-archive level).
- A/B picks via the ranker `review` mode shift the generated population toward
  taste while preserving diversity.
- Output path: ADG (semantic authority) -> `aig-pc4-midi` lossy export -> MIDI;
  audition via the existing EGMD / Virtuosity / Karoryfer sample backends.
- "Undo the last N picks" demonstrated audibly.

### M2 acceptance gate

```text
1. taste audible   blind A/B prefers taste-adapted grooves over the generic
                   baseline at a margin set in doc 18
2. diversity       the archive does not collapse to one groove; niches stay populated
3. reversibility   "undo last N picks" returns audibly to the prior taste
4. explanation     each kept groove reports which picks shaped it (provenance)
5. authority       zero runtime authority: no pc4_transport, no composition.json,
                   ADG stays semantic authority, MIDI ships its loss map
```

### M2 is explicitly NOT

- not a replacement drummer or a prompt->song generator (doc 19, DRUMMER_BASSIST_V0)
- not a runtime control path
- not a durable system (RAM-only, D013; SSD stays in docs/future/)

## Milestone-To-Backlog Map

```text
M1   L (adapter)   M (representation)   O (taste loop)   P (self-adaptation)
     Q (eval + gate, no render)
M2   N (groove generator)   Q (render + audition)
```

## After The MVP (not in scope)

- neutral-substrate extraction (doc 19) — the move that repays the D014 risk
- Rust / aig-engine deep integration (D017)
- richer operator language, CEGIS-style repair, durable store (docs/future/)
