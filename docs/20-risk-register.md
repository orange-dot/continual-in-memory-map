# 20. Risk Register

Each risk has a mitigation and a named catcher — the gate, milestone, decision, or
lab skill that is supposed to surface it. A risk with no catcher is a risk we are
choosing to run blind; there are none of those here on purpose.

## R1. D001 Contamination (the headline risk)

Risk: drum heuristics, handcrafted features, or evaluation bias masquerade as
substrate wins, so we cannot tell whether a result comes from CIM or from drums
(the exact doc 07 warning).

Mitigation:

```text
- M1 measures vs a simple online baseline, not a strawman (doc 18)
- held-out slate_eval the candidate cannot touch
- the phi seam: the kernel sees phi[NFEAT], not drums (doc 15)
- drum-specific negative results are pre-listed and recorded (doc 18)
```

Catcher: M1 acceptance gate (doc 16); D014/doc 19 own the tension; the planned
neutral-substrate extraction is the ultimate test.

## R2. Niche-Axis Design

Risk: the MAP-Elites niche axes for grooves are wrong, so "diversity" is musically
useless (variety along axes nobody cares about).

Mitigation: start from the drummer `EVALUATION_AXES.md`; treat the axis set as a
tracked open question, not a fixed claim.

Catcher: doc 06 open questions; backlog N acceptance spec (niches stay populated and
musically meaningful); M2 blind A/B (doc 18).

## R3. HDC Representation Too Weak

Risk: HDC groove encoding is weaker than the scalar `phi` path, and the project
drifts into "a pile of HDC demos" (doc 09 anti-goal).

Mitigation: keep HDC as one optional backend (the J-lane), never the theory; the
scalar `phi` path is the default and must work without HDC.

Catcher: backlog M invariant + acceptance spec (HDC path must agree with scalar
ranking order); doc 18 negative-result list.

## R4. Evaluator Gaming / Self-Grading

Risk: a candidate games the score — grades itself, edits the evaluator, or wins
only on the training window.

Mitigation: reuse the ranker catch trials (`identical_aa`, `degraded`) + the
anti-hack gate; the proposer never owns the held-out evaluator (doc 10, doc 18).

Catcher: backlog P + Q gates; M1 gate-7 (anti-hack).

## R5. MIDI Lossiness Hides Intent

Risk: the lossy ADG->MIDI export silently drops musical intent, so the MIDI is
judged as if it were the proposal.

Mitigation: ADG stays the semantic authority; every export ships a
`*.midi-loss-map`; MIDI is explicitly a rendering, never authority (doc 19).

Catcher: backlog Q invariant + M2 gate-5 (authority); doc 19 output authority.

## R6. Scope Creep Back To A Durable Store

Risk: the vertical drags the project back into the deferred SSD/durable-store work
(the scope creep D013 already removed once).

Mitigation: D013 holds; the SSD/Linux layer stays parked under `docs/future/`;
nothing in the MVP requires cross-restart persistence.

Catcher: doc 13 K / doc 19 claim boundaries; the "out of scope" section of the plan.

## R7. Proof Overclaim

Risk: docs or commit messages claim "formally verified" beyond what the Isabelle
lane actually checks (an abstract model, not the C).

Mitigation: the proven/smoke/measured/sounds-good discipline (doc 21); backlog P/Q
describe a *planned* proof extension, not a present claim.

Catcher: the `formal-proof-readiness` skill over doc 19 + `FORMAL_PROOF.md`; the
verification pass.

## R8. The "New" Column Grows

Risk: the supposedly thin new surface (doc 15) balloons, and the vertical becomes a
rebuild instead of wiring — violating D015.

Mitigation: each backlog section names the existing asset it wires; if a section's
new surface outgrows its reuse, re-scope before building.

Catcher: doc 15 "What Is New vs Reused"; architecture review at code kickoff.

## R9. Two Loops Conflated

Risk: Loop 1 (grooves) and Loop 2 (operators) get mixed in code or docs, producing
drift where a "candidate" silently means two different things.

Mitigation: the glossary fixes the two senses (doc 21); doc 15 and doc 17 tag every
candidate as groove or operator.

Catcher: glossary-consistency check (verification); backlog L/N (grooves) vs P
(operators) separation.

## Risk-To-Milestone Summary

```text
M1 critical   R1, R4, R7  (substrate honesty: contamination, gaming, overclaim)
M2 critical   R2, R5      (musical usefulness: niches, lossiness)
cross-cutting R3, R6, R8, R9
```
