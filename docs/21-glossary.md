# 21. Glossary

This document unifies the three vocabularies the drum vertical spans (doc 14
onward refer to it; doc 15 introduces the recognition). The point is that the same
machine has three names for each part. When a doc, a backlog item, or a code
symbol uses one column, this table fixes the other two.

Each term has exactly one meaning across docs 15–21. If a doc needs a fourth sense
of a word, it must define it locally rather than overload an entry here.

## Core Mapping

```text
CIM substrate        neuro-symbolic         musician / drummer
-------------        --------------         ------------------
event                audit entry            preference record (one A/B trial)
event log            audit trail            aig_ranker JSONL (append-only)
candidate            variant                groove proposal (Loop 1)
                                            / operator proposal (Loop 2)
local update         mutation               taste shift
cell                 (genome slot)          a learned taste region
context / key        behavior descriptor    law-axis coordinate
score / readout      fitness contribution   ranking of a groove
gate                 gate                    anti-pattern critic + golden set
archive              MAP-Elites archive     golden set / kept takes
niche                niche                   musical behavior region
held-out window      validation set         slate_eval phase
training window      train set              train phase
reversible txn       rollback anchor        undo of a taste shift
quarantine           discarded variant      rejected take (kept as evidence)
consolidation        elite promotion        a settled, low-plasticity preference
provenance           lineage                which picks shaped this
```

## Loop Vocabulary

```text
Loop 1  taste learning        candidates are GROOVES
Loop 2  reversible            candidates are OPERATORS (CINM_OP_*)
        self-adaptation
```

Both loops use the same gate, archive, and event log. The only difference is what
a "candidate" is. Confusing the two is the most likely source of doc/code drift.

## Operator Language (Loop 2)

`CINM_OP_*` is the small, closed set of self-adaptation operators a Loop 2
candidate can be (e.g. a step rule, a plasticity schedule, a decay rule, a
consolidation rule, an addressing alias, a capacity/pruning policy). It is a
finite description language, not arbitrary C callbacks (doc 10). The exact set is
defined in backlog section P (doc 17).

## Phases (from the ranker)

```text
train       feedback used while a candidate is active
slate_eval  held-out feedback the gate uses to decide commit / rollback
```

These map directly onto CIM's training window and validation window (doc 10). The
ranker also emits catch trials (`identical_aa`, `degraded`) that act as
evaluator-integrity checks: a proposer that cannot pass a catch trial cannot be
trusted to grade itself.

## Authority Terms

```text
semantic authority   the ADG groove is the source of musical truth
lossy export         MIDI carries notes/ticks/channel/velocity, not the full ADG
                     surface; ships with a *.midi-loss-map
review-gated         a proposal becomes action only after a human accepts it
runtime authority    the right to mutate live state (state_core, pc4_transport);
                     the vertical does NOT have this
```

## Claim Words

```text
proven        machine-checked in the Isabelle lane (docs/isabelle, FORMAL_PROOF)
smoke         a runnable C gate passed (make run-*); not a proof
measured      an evidence-backed metric from a named run (doc 18)
sounds good   a human aesthetic judgement; never a substrate claim
```

Keeping these four apart is the project's honesty discipline. The
`formal-proof-readiness` posture (doc 19) forbids using "proven" for anything that
is only "smoke" or "measured", and forbids using "measured" for "sounds good".
