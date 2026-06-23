# 22. State-Primacy Refactor Plan (D018)

Status: **implemented — R0–R5 landed and green.** The staged plan below was executed
in `experiments/c-kernel/`; every stage gate passes (`make run-replay run-ledger
run-consolidate run-undo`, and the full suite at 31 PASS / 0 FAIL, no warnings). The
text is kept as the rationale and stage map.

This plan brings the C kernel in line with **D018** (doc 05): the live map is the
primary state; full-log replay is a *within-epoch* recovery and verification
property, not the source of truth; consolidation is genuinely lossy; reversibility
is windowed. The core docs (05, 06, 12, 13, 14, FORMAL_PROOF) were corrected when
D018 was recorded. The **kernel surface still carries the old framing** — that is
stage R0 below.

## Sequencing decision: refactor first, clean substrate

The refactor lands **before** the drum vertical (docs 15–21) consumes the
substrate. Rationale:

- The vertical's M1 sections — **P** (Gödel–Darwin self-adaptation) and **O**
  (taste-loop undo) — *consume* the primitives this refactor adds (decision
  ledger, epoch, consolidation, bounded undo). Building them as **neutral
  substrate** first, then letting the drum adapter sit on top, preserves the
  phi-seam (doc 07 / `AGENTS.md`) and avoids rebuilding the same machinery twice
  (risk R8, doc 20).
- D018 is a substrate-level correction (D001 charter), not a drum concern. The
  substrate should be D018-correct on its own terms; the vertical inherits it.

So: the kernel reaches state primacy as a domain-neutral substrate; the drum
vertical begins (or resumes) against the corrected substrate.

## What does not change (load-bearing — stays green throughout)

```text
cinm_snapshot / cinm_restore / cinm_transaction   reversibility (snapshot-based)
run-txn                                           byte-exact rollback gate
```

This is the reversibility the project's identity rides on ("reversible
self-adaptation", D012). It is **untouched** by R0–R5. Most of the refactor is
**additive**; the only genuinely new behavior is **R3** (lossy consolidation),
plus the **R1** re-scope of the replay gate. If at any stage the new surface
starts duplicating or replacing `cinm_snapshot`/`cinm_transaction`, that is drift
— stop and re-scope.

## Stages

Each stage in the doc-13/doc-17 style: *Goal / Planned surface / Invariant /
Gate / Acceptance*. Surfaces are grounded in the current API (`cinm.h`,
`cinm_log.h`).

### R0 — Language / comment correction (zero behavior change)

- **Goal:** the kernel's own comments and README stop claiming the log is the
  source of truth.
- **Planned surface:** `cinm_log.h` header comment ("append-only learning events,
  replayable truth"; "the map is a projection of this log, reconstructed by
  deterministic replay"; "the replayable source of truth"), `cinm.h` ("rollback
  and store replay reconstruct the same map"), `experiments/c-kernel/README.md`
  ("The replayable source of truth…", "derivative working projection", "in-RAM
  event log = replayable truth (within a session)"). Reworded to the D018 framing:
  map = primary state; log = evidence + within-epoch replay; snapshot =
  reversibility.
- **Invariant:** no behavior change; no symbol renamed (the `cinm_` prefix stays,
  D013).
- **Gate:** existing suite unchanged, all PASS.
- **Acceptance:** `grep -ri "source of truth\|replayable truth\|derivative
  projection" experiments/c-kernel` returns only D018-consistent, annotated uses;
  `make run-core-check run run-cmp run-learning run-txn run-replay
  run-log-invariants run-memory run-hdc` all PASS unchanged.

### R1 — Make the epoch explicit

- **Goal:** name the epoch boundary that replay and undo are scoped to.
- **Planned surface:** an explicit epoch base (`base_seq`, plus an epoch counter)
  on the map or a small run-context struct. `cinm_log_replay(from_seq)` **already
  takes `from_seq`** — the mechanism exists; what is missing is the *named* epoch
  base that consolidation (R3) advances and that the gate anchors to. Re-scope
  `replay_check.c`: "event-log alone recovers state" → "within-epoch replay (from
  the epoch base) recovers state". Full-from-seq-0 replay remains valid only as
  the *no-consolidation-yet* special case.
- **Invariant:** with no consolidation yet, within-epoch replay ≡ full replay
  (backward-compatible); `run-txn` untouched.
- **Gate:** `run-replay` (re-scoped to within-epoch).
- **Acceptance:** replay is anchored at `base_seq`; the report names the epoch;
  pre-consolidation behavior is byte-identical to today.

### R2 — Decision / evidence ledger (receipts)

- **Goal:** an append-only record of *why* the state changed, separate from the
  *what* of learning events.
- **Planned surface:** a `cinm_ledger` (parallel to `cinm_log`) of `cinm_decision`
  receipts: operator commit/rollback (Loop 2) and consolidation events (R3).
  **Kept separate from `cinm_event`** so the hot event `{seq,type,key,reward,
  dphi}` stays lean (doc 12 cache discipline) — receipts carry different fields
  (operator id, held-out `slate_eval` delta, what-was-forgotten) and live cold.
  This is the in-RAM analogue of doc 18's `decisions.jsonl`.
- **Invariant:** append-only; every receipt references the seq/epoch it applies
  to; the ledger never feeds the hot path (read for audit/undo only).
- **Gate:** `run-ledger` (new).
- **Acceptance:** each commit/rollback/consolidation appends exactly one receipt;
  ledger replays its receipts in order; receipts are sufficient to *explain* a
  state change (which D008 "trace to evidence" requires) without being sufficient
  to *reconstruct* the map (which D018 forbids requiring).

### R3 — Consolidation as a genuine lossy operation

- **Goal:** real, bounded forgetting — "as aggressive as a real learner needs"
  (D018), explicit and recorded (D010).
- **Ceiling (grounding the policy):** the ceiling is set by the addressing model.
  Addressing is **exact-symbolic** today (`cinm_address` matches keys exactly;
  NN/clustered addressing is an open doc-06 question, unbuilt). Under exact keys
  there is **no redundant cell** — every cell is the sole source for its context,
  so nothing can be merged into a prototype without losing the ability to address a
  key. R3 is therefore **evict-dead + freeze-strong only**. Schema/merge
  compression (folding similar contexts into a prototype — what a "real learner"
  does) needs NN/prototype addressing and is deferred to **R3.5**, gated on the
  doc-06 addressing decision.
- **Resolved policy — decay proposes, consolidation disposes:**

  ```text
  decay  (existing, soft, continuous)  weakens idle / uncorroborated cells toward 0
                                       — the in-window soft forgetting (D018)
  evict  (new, hard, at trigger)       reap cells decay drove below a floor AND
                                       low-conf / low-evidence / stale -> receipt,
                                       epoch++, undo tail truncated (couples R4)
  freeze (new, promotion)              high-conf + high-evidence cells: plast->floor,
                                       exempt from decay + eviction (needs a
                                       `protected` marker) -> receipt, NOT lossy
  ```

  Consolidation is the garbage-collection of what decay has already emptied, not an
  independent culler (this answers doc 06 "which state should decay first" and
  "useful vs harmful forgetting"). Eviction targets only contexts outside the active
  distribution; a revived dead context is re-learned cold — acceptable forgetting,
  but its cost is measured (see acceptance).
- **Policy ownership:** R3 ships a **fixed default policy with aggressiveness
  exposed as a parameter** (decay floor / keep-threshold). Loop-2 self-adaptation
  *tuning* that parameter (propose a threshold -> score on held-out ->
  commit/rollback) is deferred — not mixed into the neutral refactor.
- **Planned surface:** `cinm_consolidate(...)` (evict + freeze; no merge), a
  `frozen` marker for promoted cells, the aggressiveness parameter, a summary
  consolidation receipt (evicted/promoted counts, epoch, base_seq) appended by the
  caller to the R2 ledger, epoch advance (`base_seq` <- current seq), and R4
  undo-window truncation. The existing `cinm_decay` stays as the soft in-window
  forgetting it feeds on, and is exempt for frozen cells.
- **Invariant:** consolidation is the *only* lossy-past-boundary operation; it
  evicts only dead/weak cells and never a frozen one; post-consolidation full-log
  replay does **not** reconstruct the map (the loss is real), while within-epoch
  replay from the consolidation snapshot does.
- **Gate:** `run-consolidate` (new).
- **Acceptance:** on a stale-context workload, consolidation reduces `count` /
  memory, emits a summary receipt (counts, epoch, base_seq), and bumps the epoch; the
  **extended forgetting probe (doc 18) PASSES all three cases as hard spec numbers,
  not report lines:** (a) active contexts retained, (b) **revival cost** of an
  evicted context within the bound in `config.toml`, (c) frozen cells do not drift;
  and a test asserts post-consolidation full replay diverges (lossy) while
  within-epoch replay matches.

### R4 — Bounded undo window (retention horizon)

- **Goal:** reversibility that is windowed and honest at its edge.
- **Planned surface:** a snapshot **ring** + a short delta **tail**, sized to a
  declared retention horizon `H`, built from the existing `cinm_snapshot`. "Undo
  N picks" works for N within the tail; coarse undo hops to a ring snapshot;
  nothing reaches past the last consolidation (R3 truncates the tail at the epoch
  boundary).
- **Invariant:** undo within `H` is byte-exact (reuses snapshot equality,
  `cinm_equal`); undo beyond `H` or across a consolidation is **refused**
  ("outside retention window"), never silently wrong.
- **Gate:** `run-undo` (new).
- **Acceptance:** undo of the last k ≤ H operations reconstructs the prior map
  byte-exactly; undo of k > H or across an epoch returns a clear out-of-window
  error; operator-level rollback (Loop 2, R3-independent) is unaffected.

### R5 — Gate suite + proof-lane finalization

- **Goal:** the runnable + formal surface states D018 honestly.
- **Planned surface:** `experiments/c-kernel/README.md` expected-results updated
  for the re-scoped `run-replay` and the new `run-ledger`/`run-consolidate`/
  `run-undo`; the full `make` list extended. Isabelle lane stays as-is
  (within-epoch determinism; FORMAL_PROOF already carries the D018 scope note).
  *Optional planned theory:* consolidation as a well-defined epoch transition —
  explicitly **not** reversible (no reconstruction obligation).
- **Invariant:** no machine-checked claim beyond what the Isabelle lane actually
  checks (`formal-proof-readiness`).
- **Gate:** full `make` suite green under the re-scoped semantics.
- **Acceptance:** README expected-results match real output; FORMAL_PROOF "Known
  Gaps" still lists everything not proven; nothing claims reconstruction across a
  consolidation.

## Dependencies

```text
R0 (anytime, zero-risk)
R1 epoch ──► R2 ledger ──┐
        └──► R3 consolidation (needs R1 epoch + R2 receipt) ──► R4 undo window ──► R5
```

R3 is the high-care stage (the only new lossy behavior); R0/R1/R2/R4/R5 are
additive or test/doc.

## Relationship to the drum vertical (docs 15–21)

These are **neutral substrate** primitives; the vertical consumes them:

```text
R2 decision ledger      ◄── doc 17 P (self-adaptation receipts), doc 18 decisions.jsonl
R3 consolidation        ◄── doc 17 O/P (taste consolidation is a domain use of this)
R4 bounded undo window  ◄── doc 17 O ("undo N picks"), doc 16 M2 audible undo
R1 within-epoch replay  ◄── doc 16 M1 gate 5 (deterministic replay, now within-epoch)
```

The drum adapter (doc 17 L/M) remains the only drum-aware code; the kernel still
sees `phi[NFEAT]`, not drums. The vertical adds no substrate primitive that this
refactor has not already made neutral.

## Boundaries

- Implemented under RAM-only (D013): no durable store, no symbol renames (the
  `cinm_` prefix stands), hot path stays libm-free.
- The claim after this refactor is **within-epoch** deterministic replay and
  **windowed** reversibility — not global reconstruction, which D018 retired.
- Consolidation is deliberately outside the reversible envelope; that is the
  point, not a gap.
- Schema/merge consolidation (R3.5) is deferred and gated on the doc-06 addressing
  decision (exact-symbolic keys → no prototypes yet); R3 ships evict-dead +
  freeze-strong on exact keys.
