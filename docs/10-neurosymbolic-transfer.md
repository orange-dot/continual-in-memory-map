# Neurosymbolic Transfer: Interactive Continual Preference Learning

Reviewed: 2026-06-21

> Scope note (D013): the reversible-self-adaptation machine below is implemented
> strictly in-memory — `cinm_snapshot`/`cinm_restore`/`cinm_transaction` plus the
> in-RAM `cinm_log`. References to durable storage, SSD archive, or persistent
> quarantine describe the deferred future store (`docs/future/`), not current
> behavior. The reversibility and deterministic-replay invariants hold within a
> process.

This note records what CINM should transfer from the two local neuro-symbolic
research repositories:

- `workspace/research/neuro-simbolic`
- `workspace/research/neuro-simbolic-rs`

The transfer target is the CINM C exploration substrate:

- `workspace/research/continual-in-memory-neuromorphic-map/experiments/c-kernel`

The important conclusion is:

```text
CINM should transfer the reversible adaptation skeleton first.
It should not transfer proof-heavy machinery first.
```

CINM's working terminology is:

```text
interactive continual preference learning
  with reversible self-adaptation
```

This matters because a human or external evaluator often confirms what counts
as better. That is supervised preference signal. The system may still propose
and test local changes to its memory/update machinery, but the acceptance path
is human-gated or evidence-gated, not autonomous self-certification.

For CINM, a bad adaptation is not expensive if it can be identified and
reverted quickly. Therefore the first safety property is not:

```text
prove that every candidate is truly useful before applying it
```

The first safety property is:

```text
apply small changes as reversible transactions,
measure them quickly,
commit only useful changes,
rollback failed changes cheaply,
and keep the failed attempt as evidence.
```

This reframes the Darwin-Godel material for CINM.

`self-improvement` remains useful as a source term when discussing the
neuro-symbolic repositories. CINM's own term should be `reversible
self-adaptation`, usually inside the larger phrase `interactive continual
preference learning with reversible self-adaptation`.

## Source Repositories

### Go Repo: `neuro-simbolic`

Path:

```text
/home/dev/work-base-20260421/workspace/research/neuro-simbolic
```

Useful source concepts:

- `internal/evolution/archive.go`
  - MAP-Elites / quality-diversity archive.
  - Population bounded by total size and elites per niche.
  - History of all variants, including discarded variants.
  - Best variant and per-niche local elites.
- `internal/evolution/lineage.go`
  - Parent/child lineage.
  - Generation tracking.
  - Elite marking and survival tracking.
- `internal/evolution/variant.go`
  - Variant identity.
  - Parent IDs.
  - Mutation log.
  - Fitness attached after evaluation, not before.
- `internal/audit/trail.go`
  - Append-only audit entries.
  - Entry types for candidates, validation, rewrite, repair, proof accept/reject,
    config change, start, and stop.
  - Previous-hash chain of custody.
- `internal/audit/hash.go`
  - Hashing and Merkle-style integrity ideas.
  - Useful later, but too heavy for the first C kernel.
- `internal/verify/antihack.go`
  - Reward-hacking checks.
  - "Empirical looks good but formal/evidence side disagrees" as a warning sign.
  - Novelty and triviality checks.
- `internal/metrics/evaluator.go`
  - Multi-metric comparison.
  - Pareto dominance first, weighted score only as a fallback.
- `internal/synthesis/cegis.go`
  - Counterexample-guided refinement loop.
  - Useful later for turning failed contexts into repair proposals.
- `internal/ir/types.go`, `internal/rewrite/`, `internal/egraph/`
  - Program-as-data, rewrite, and equivalence machinery.
  - Useful later if CINM gets a small operator language.

What should transfer now:

```text
candidate -> evidence -> gate -> archive -> audit -> rollback
```

What should not transfer yet:

```text
Lean bridge
full proof certificates
full E-graph machinery
full CEGIS loop
large JSON IR
LLM proposal path
```

### Rust Docs Repo: `neuro-simbolic-rs`

Path:

```text
/home/dev/work-base-20260421/workspace/research/neuro-simbolic-rs
```

Useful source concepts:

- `docs/04-improvement-operators.md`
  - Source-repo improvement operator proposes; gate decides.
  - Candidate carries evidence.
  - Darwinian path measures benefit.
  - Godelian path supplies certificates for local invariants.
  - Unified path accepts proof-carrying empirical edits.
  - Two gates: validation margin and capacity.
- `docs/03-architecture.md`
  - Gate is the center of gravity.
  - Archive remembers.
  - Runtime measures.
  - No proposer grades its own candidate.
  - Stable identity and hashing matter for lineage and dedup.
- `docs/05-safety-and-limits.md`
  - Reward hacking is expected.
  - Held-out evaluation matters.
  - Fitness must be objective and tamper-resistant.
  - Utility must live outside the mutable surface.
  - Capacity caps are a safety throttle.
  - Rollback anchors matter.
- `docs/adrs/0005-restricted-self-modification-language.md`
  - Restrict the editable language when exact verification is impossible.
- `docs/adrs/0006-stratified-ir-egg-over-certifiable-core.md`
  - Keep a small certifiable core and a larger empirical exploratory shell.

What should transfer now:

```text
proposer != grader
gate decides
archive remembers
runtime measures
capacity bounds drift
proofs are local invariant checks, not the main learning loop
```

What should not transfer yet:

```text
the whole crate map
proof-first acceptance
egg/egglog dependency
SMT dependency
Turing-complete self-modification framing
```

## CINM Reframe

The neuro-symbolic repos are mostly about self-improving programs.

CINM is about interactive continual preference learning with reversible
self-adaptation.

That changes what "improvement" means and who confirms it.

In the neuro-symbolic repos, an improvement can be a program rewrite, tactic
change, proof strategy change, or genome mutation.

In CINM, the primary feedback is often a human preference signal:

```text
this response/ranking/memory behavior is better than that one
```

That makes the learning signal supervised in the broad sense, and more
precisely preference-based. The self-directed part is narrower: the system may
propose local adaptations to how it stores, updates, consolidates, or retrieves
memory.

In CINM, a reversible adaptation can be smaller:

- a weight update rule
- a plasticity schedule
- a decay rule
- a consolidation rule
- a cell allocation policy
- a capacity/pruning policy
- a context-addressing policy
- a score calibration rule
- a conflict-resolution policy

These changes are local and cheap. They should be tried quickly, measured
quickly, and reverted quickly if they do not help.

Therefore CINM should not begin as a proof-heavy autonomous improver. It
should begin as a reversible, human/evidence-gated adaptation system.

## Core Transfer Thesis

```text
No proof of usefulness is required for the first CINM reversible adaptation loop.

A failed adaptation must be cheap to undo.

The system should learn from both committed and rolled-back attempts.
```

The Godelian role moves downward:

```text
Do not prove that a candidate is useful.
Prove or check that the candidate is bounded, reversible, replayable,
and unable to corrupt the measurement harness.
```

The first Godelian-style checks are infrastructure checks:

- map count never exceeds `MAX_CELLS`
- weights remain inside `[-W_MAX, W_MAX]`
- event clock is monotonic
- evidence count is monotonic per committed update
- speculative changes have a rollback path
- scorer does not mutate state
- evaluation uses held-out data
- proposer cannot directly edit the grader
- archive capacity is bounded
- replay from a seed and event log is deterministic

The Darwinian role remains central:

```text
try many small changes,
measure real behavior under human or external preference feedback,
keep local winners,
preserve diversity,
archive failures as evidence.
```

## Reversible Self-Adaptation Machine

The target machine is:

```text
stable state
  -> propose small candidate
  -> snapshot or begin transaction
  -> apply candidate to working state
  -> evaluate on short held-out window
  -> commit / rollback / quarantine
  -> append evidence
  -> update archive
```

The important states are:

```text
canonical state
  last committed stable state

working state
  currently executing state

speculative branch
  candidate-applied state under evaluation

rollback record
  enough data to undo candidate effects

quarantine
  failed or suspicious candidate, retained as negative evidence

archive
  accepted and interesting variants, grouped by behavior niche
```

The first C kernel can make rollback simple:

```text
copy the whole cinm_map before speculative evaluation
restore it if the candidate fails
```

This is acceptable because the first map is tiny:

```text
MAX_CELLS = 256
NFEAT = 8
```

Later, whole-map copy can be replaced by a delta log:

```text
touched cell id
old weights
old plasticity
old confidence
old evidence count
old last_touched
old map count
```

The design should prefer correctness and inspectability before memory tricks.

## Candidate Shape

The first candidate should not be a C callback or arbitrary function pointer.

It should be a small, closed operator description.

Example operator kinds:

```text
CINM_OP_PAIRWISE_STEP
CINM_OP_MARGIN_SCALED_STEP
CINM_OP_PLASTICITY_DECAY
CINM_OP_CONFIDENCE_UPDATE
CINM_OP_CELL_CONSOLIDATE
CINM_OP_CAPACITY_PRUNE
CINM_OP_CONTEXT_ALIAS
```

A candidate should carry:

```text
candidate_id
parent_id
operator_kind
small parameter vector
created_at_event
source
expected capacity cost
evaluation window
evidence summary
status
```

Status values:

```text
proposed
applied_speculative
committed
rolled_back
quarantined
archived
```

This keeps the adaptation-operator language small and replayable.

It also leaves room for later Rust or Lean tooling to reason about the operator
space without understanding arbitrary C behavior.

## Gate Shape

The first CINM gate should be lightweight.

It should not require formal proof. It should require:

```text
boundedness gate
  candidate cannot exceed memory, weight, or operator limits

validation-margin gate
  candidate must beat baseline by at least tau on held-out feedback

rollback gate
  candidate must have a valid undo path

anti-hack gate
  candidate cannot grade itself or edit the held-out evaluator

capacity gate
  candidate cannot grow cells/operators/state beyond the current budget
```

The acceptance rule is:

```text
accept iff all hard gates pass
```

But rollback changes the psychology:

```text
failed candidate != disaster
failed candidate == evidence
```

This is the key difference from proof-first autonomous improvement and
from ordinary supervised learning: the feedback may be supervised, but the
adaptation policy itself is explicitly transactional, reversible, and archived.

## Archive Shape

CINM should not keep only the global best candidate.

It should keep local winners in behavior niches.

Possible niches:

```text
fast_adaptation
low_forgetting
low_memory_growth
low_update_cost
high_stability
high_plasticity
conflict_resistant
good_on_sparse_contexts
good_on_noisy_feedback
```

Each niche can keep a small number of elites.

Archive records should include:

```text
candidate_id
parent_id
niche_id
fitness summary
commit count
rollback count
contexts helped
contexts harmed
created_at_event
last_used_event
```

Rolled-back candidates should not disappear. They should go to quarantine with
negative evidence:

```text
candidate X failed because it improved context A but harmed context B
candidate Y adapted quickly but grew memory too fast
candidate Z passed short window but failed held-out replay
```

This makes the system smarter even when the attempted adaptation is rejected.

## Audit And Replay

The Go repo's audit layer uses structured entries, previous hashes, and
optional persistence. CINM should borrow the shape, not the full machinery.

First C version:

```text
event_id
prev_checksum
event_kind
candidate_id
parent_id
cell_key
operator_kind
score_before
score_after
capacity_before
capacity_after
decision
reason_code
```

Reason codes can be simple:

```text
accepted_margin
rejected_low_margin
rejected_capacity
rejected_rollback_missing
rejected_bounds
rolled_back_regression
quarantined_suspicious
```

This is enough for:

- deterministic replay
- rollback diagnosis
- lineage tracing
- future external verification
- test vector generation

Cryptographic hashes and Merkle proofs can wait.

## Evaluation Discipline

The current C preference task already has one important property:

```text
predict before learning
```

This should remain a hard rule.

For reversible-adaptation candidates, evaluation should also distinguish:

```text
training window
  feedback used while candidate is active

validation window
  held-out feedback used by the gate

replay window
  deterministic rerun used to confirm the decision
```

The candidate may affect the working state. It must not affect the evaluator.

This imports the neuro-symbolic anti-hack rule in CINM language:

```text
proposer does not grade itself
candidate does not own the score
fitness definition is outside the mutable surface
```

## Proof Role

Proof is not discarded.

It is deferred and narrowed.

First proof-like checks:

```text
assertions
deterministic replay
bounded arrays
clamped updates
explicit rollback records
no evaluator mutation
fixed seed test vectors
```

Later external proof targets:

```text
cinm_score is pure
cinm_update respects weight bounds
rollback restores prior state
capacity gate prevents unbounded growth
event log replay reconstructs committed state
candidate language is finite and closed
```

This is the useful Godelian transfer:

```text
prove reversibility and bounds before proving usefulness.
```

## What To Implement First In The C Kernel

The current `experiments/c-kernel` has:

- `cinm_map`
- `cinm_cell`
- exact discrete addressing
- local score
- `cinm_update` placeholder
- preference task harness

The recommended sequence is:

1. Fill the minimal bounded pairwise update.
2. Add a whole-map snapshot/restore function.
3. Add a tiny candidate/operator struct.
4. Add a speculative evaluation wrapper:

   ```text
   snapshot -> apply candidate -> evaluate -> commit or restore
   ```

5. Add a minimal gate:

   ```text
   margin + bounds + capacity + rollback availability
   ```

6. Add a tiny archive:

   ```text
   fixed array of candidate records
   best per niche
   quarantine for failures
   ```

7. Add an audit ledger:

   ```text
   fixed array or append-only text output
   deterministic checksum, not SHA-256 yet
   ```

Only after that should the project consider:

- richer operator language
- CEGIS-style repair
- Rust wrapper
- Lean proof bridge
- E-graph/rewrite search
- CUDA acceleration

## What To Avoid

Avoid importing too much neuro-symbolic machinery too early.

Do not start with:

- Lean proof certificates
- generic theorem proving
- full E-graphs
- arbitrary self-modifying C callbacks
- LLM-generated operators
- unbounded archives
- a complex JSON IR in the C hot path
- proof-first acceptance of every update

Those are interesting later, but they obscure the CINM-specific insight.

The CINM-specific insight is:

```text
small local changes are cheap,
so safety comes first from rollback, locality, and evidence,
not from pre-proving every useful adaptation.
```

## Updated Working Claim

Old transfer framing:

```text
CINM should become a proof-carrying autonomous improvement system.
```

Better CINM terminology:

```text
CINM studies interactive continual preference learning
with reversible self-adaptation.

It uses Darwinian search for small local operator adaptations.
It uses Godelian discipline for bounds, rollback, replay, and invariants.
It treats failed adaptations as evidence, not as catastrophic errors.
```

This is the clean bridge from neuro-symbolic self-improvement ideas into CINM
without overclaiming autonomy. The human or external feedback source confirms
preference; the system supplies reversible adaptation machinery around that
signal.
