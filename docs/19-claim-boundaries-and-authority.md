# 19. Claim Boundaries And Authority

The drum vertical (D014) lives inside a repo whose charter is domain-neutrality
(D001). That tension is real. This document fences the vertical so the tension is
owned, not hidden. It extends the claim boundaries of doc 13 section K and adopts
the drummer model's authority discipline (DRUMMER_BASSIST_V0, OUTPUT_CONTRACT).

## What The MVP Claims

```text
M1   a sparse local memory map can learn drum preference continually, with
     bounded local updates, reversible adaptation, and deterministic replay,
     and beat a simple online baseline on a drum-shaped task
M2   that learned taste can drive a diverse groove generator whose output a human
     prefers over a generic baseline, with reversible and explainable behavior
```

Both are narrow, testable claims tied to the acceptance gates in doc 16.

## What The MVP Does NOT Claim

```text
- not a domain-neutral substrate result (that is the later extraction, below)
- not a replacement drummer, and not a prompt->song generator
- not biological fidelity (D004)
- not a replacement for large-scale backpropagation
- not a durable system (RAM-only, D013; SSD stays in docs/future/)
- not a runtime control path
- not "formally verified C" (the Isabelle lane proves an abstract model; FORMAL_PROOF)
```

## The D001 / doc 07 Tension, Owned

doc 07 warned: if the first prototype is domain-specific, it may be unclear whether
results come from the memory substrate or from domain heuristics, handcrafted
features, evaluation bias, or existing machinery. We are deliberately taking that
risk (D014). The mitigations are concrete, not rhetorical:

```text
1. fair baseline    M1 measures vs a simple online learner, not a strawman (doc 18)
2. held-out         the gate scores on slate_eval the candidate cannot touch
3. anti-hack        catch trials + proposer != grader (doc 10, doc 18)
4. phi seam         the kernel sees phi[NFEAT], not drums; the adapter is the only
                    domain-aware layer (doc 15) — the doc 07 bridge rule kept
5. negative results drum-specific failure modes are pre-listed (doc 18) and will be
                    recorded, not buried
```

If these mitigations fail to separate substrate signal from drum heuristics, that
is itself a recorded result (doc 18), and the substrate claim is withheld.

## Neutral-Substrate Extraction (future move, not MVP)

The move that repays the D014 risk is extracting the domain-neutral substrate after
the vertical proves it out:

```text
vertical proves out
  -> identify what in the kernel path is drum-agnostic
  -> confirm it still passes the doc 03 neutral toy tasks
     (non-stationary bandit, contextual preference, forgetting probe)
  -> the result is the domain-neutral CIM substrate D001 always intended
```

This is tracked as the post-MVP direction. It is not an MVP acceptance criterion.
Until it happens, the substrate claim stays scoped to drums.

## Authority Boundary

The vertical produces review-gated proposals. It has no runtime authority. This
mirrors the drummer model's boundary (DRUMMER_BASSIST_V0, "Authority Boundary").

The vertical must NOT:

```text
- call pc4_transport
- write composition.json or instrument composition docs
- mutate state_core or any live runtime state
- emit final MIDI as authority
- bypass the human review gate
- claim to replace the composer-working-musician
```

The vertical MAY:

```text
- learn taste from human A/B picks
- generate and rank groove proposals
- produce a reversible, explainable taste model
- export a lossy MIDI proposal with its loss map
- record evidence and run artifacts
```

## Output Authority

```text
semantic authority   the ADG groove (aig-core / aig-adg) is the musical truth
lossy last mile      aig-pc4-midi exports MIDI 1.0; it carries notes, ticks,
                     channel, velocity — not the full ADG surface
loss map             every export ships a *.midi-loss-map (what MIDI dropped)
```

MIDI is a rendering of the proposal, never the authority for it.

## Proof Boundary

The Isabelle lane (docs/isabelle, FORMAL_PROOF) proves an abstract model of
bounded update, reversibility, and replay — not the C layout, not floating point,
not "drums sound good". The drum vertical does not extend any machine-checked claim
on its own; backlog section P/Q describes the *planned* extension of the abstract
invariants, gated by the `formal-proof-readiness` posture. Until that lands:

```text
proven    only what docs/isabelle actually checks
smoke     make run-* gates (behavior, not theorems)
measured  named-run metrics (doc 18)
sounds good   human judgement; never a substrate or proof claim
```

(Term definitions: doc 21.)
