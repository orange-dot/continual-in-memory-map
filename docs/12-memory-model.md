# 12. Memory Model (L1 → RAM)

This document describes the execution-locality model (CPU caches + RAM) for
CINM. It is design guidance, grounded in the local hardware (see doc 11) and in
what `experiments/c-kernel/` now implements.

Per decision D013 the kernel is **strictly in-memory**: there is no disk tier.
The event log, snapshots, and replay all live in RAM. The durable SSD store that
this document originally covered is parked under
`docs/future/ssd-store-linux-layer.md` as a deferred future mapping.

Guiding principle: you do not "fill" the caches — the CPU manages them. You
influence behavior through **data layout, access order, batch size, hot/cold
split, and how many threads write to what**.

> **Amended by D018 (state primacy).** The live **map is the primary state** —
> operationally (immediate readout reads it directly) and as the system's actual
> truth. The in-RAM event log is **evidence + a bounded undo window**, not the
> definition of the map; full replay **reconstructs the map only within an epoch**
> (since the last consolidation) and is a recovery/verification property, not the
> normal mode of operation. Lines below that call the log "the source of truth"
> predate D018 and are read in that demoted sense.

## The tiers

```text
L1 / L2 / L3 = the CPU's current thinking (execution locality)
RAM          = active map (primary state) + the event log (evidence + bounded undo)
```

| tier | CINM role |
|------|-----------|
| L1   | the live micro-op: one active cell, candidate phi/dphi, score/accumulators, a small rollback record |
| L2   | local batch / candidate working set; cache-blocked chunk of cells/prototypes; top-k temporaries |
| L3   | shared read-mostly: prototype bank, context keys, compact archive summaries, candidate pool |
| RAM  | working map, append-only event log, rollback snapshots, HDC banks |

## Cache discipline

- **L1 — hot working set:** keep it small and stable. The score/update path
  (one cell, `w[NFEAT]=32 B`, `dphi`, two accumulators) is ~one cache line.
- **L2 — cache blocking:** scan in chunks (256 / 1024 / 4096), not randomly over
  everything; keep `chunk_bytes` ≤ part of L2, leaving room for code/stack.
- **L3 — read-mostly shared:** read-shared is fine; avoid write-shared (false
  sharing / invalidation). For parallelism, give each thread local top-k / stats
  / rollback scratch and merge at the end; never pack per-thread counters
  adjacently.
- **RAM:** keep hot-loop access linear and predictable; the big world lives here
  but the hot path should not chase it randomly.

Earlier *estimate* (60 B cell, 48 KiB L1d): ≈ 819 cells in L1, ~35k in L2,
~420k in L3. **Two corrections from the cim-sys-scale-v1 run (decision A):**
(1) the cell is **90 B today** — the D019 `proto[NFEAT]` address nearly doubled
the 60 B AoS figure; (2) regimes are host-specific. On the benchmark host (i7-4600U:
L1d 32 KiB/core, L2 256 KiB/core, L3 4 MiB) the 90 B cell gives ≈ **364 in L1,
~2913 in L2, ~46603 in L3**, then DRAM.

**The cache regime is not the lever, though.** The measured `crossover_n = 256`:
linear-scan addressing (`cinm_address` O(count), `cinm_address_nn` O(count·NFEAT))
dominates the per-cell score work from the *smallest* swept map and stays dominant
to 1M cells — `score_ns_per_cell` is a flat ~4–6 ns across every regime, while one
lookup reaches ~404 µs (p50 823 µs) at 1M. Retrieval, not arithmetic or cache
layout, is the scaling wall. See `experiments/c-kernel/runs/cim-sys-scale-v1/`
(doc 18 watch "addressing dominates" — confirmed).

**Phase 3a removed that wall for exact-key retrieval (cim-sys-scale-v2).** An
open-addressing hash index (`cinm_index.c`, added *alongside* the untouched linear
scan) turns `cinm_address` from O(count) into ~O(1): on the same host the per-query
cost goes 130 ns @256 / 49.6 µs @131k / **443 µs → 47 ns at 1M** (≈ 9 000× there),
and the linear tail (v1 p99 4.3 ms) disappears — one hash probe has no
count-dependent tail. The index reproduces the scan's exact cell choice byte-for-byte
(`scale_index_check.c` green; the linear scan stays the reference, doc 11 rule #1), so
the cache-regime numbers above still describe the *scan*; the index sidesteps them.
Build cost is O(count) (~24–74 ns/cell), amortized for read-heavy addressing. The
nearest-neighbour scan (`cinm_address_nn`) is now indexed too (`cinm_nn_index.c`, a
static KD-tree, cim-sys-scale-v3) and byte-exact — but at NFEAT=8 it is
backtracking-bound, so it only *reduces* the wall (≈6.2 ms → 0.74 ms mean at 1M, 8.3×,
and slower than the scan below ~2k cells), not flatten it the way the exact-key hash
did. The exact-key/NN asymmetry is measured, not assumed (see
`runs/cim-sys-scale-v3/summary.md`).

## C implications — hot/cold + SoA (adopted now)

The kernel uses structure-of-arrays with a hot/cold split (this supersedes
doc 11's "SoA later" — adopted now to make scans cache-dense):

```text
hot  : key[]  w[][NFEAT]  plast[]  conf[]      (address / score / update)
cold : evidence[]  born[]  last_touched[]  in_use[]   (provenance / bookkeeping)
```

A "cell" is an index across these arrays. Scanning `key[]` (addressing) now
touches a contiguous ~1 KB region instead of striding 60 B AoS cells, so scans
do not drag cold fields through cache. Tradeoff: whole-map snapshot stays a
trivial `memcpy` of the ~15 KiB POD; single-cell rollback becomes a row gather.

## In-RAM event log — evidence + bounded undo (replayable within an epoch)

```text
The live map is the primary state.
The in-RAM event log is evidence + a bounded undo window.
recover (within an epoch) = take an in-RAM snapshot + replay events after it.
```

This is the reversible-self-adaptation model (D012). Each adaptation is a
transaction:

```text
BEGIN candidate -> APPLY delta -> EVALUATE -> COMMIT or ROLLBACK -> append evidence
```

The **event is layout-independent** (`{seq, type, key, dphi[NFEAT], reward}`), so
it survives in-memory refactors — replay events into any future layout. The log
is an append-only growable array (`cinm_log`); a "snapshot" is the existing
in-RAM `cinm_snapshot` (a whole-map `memcpy`) plus the log position at that point.

Replay is strict on the invariants that still matter without a disk: a sequence
gap or duplicate, an unsupported event type, or map capacity overflow is a failed
recovery, not a partial success. On-disk concerns — record framing, checksums,
corrupt tails, durability — belong to the deferred SSD store (`docs/future/`).

`cinm_init` zeroes the whole map so unused cells are deterministic and replay is
reproducible on the current build.

### `cinm_log` API

```text
cinm_log_init / cinm_log_free
cinm_log_append            append one event (grows the array)
cinm_log_replay            replay events with seq >= from_seq into a map
cinm_log_replay_checked    same, with a logical-invariant report
```

Durable persistence (the `cinm_store_*` POSIX boundary, mmap HDC banks,
io_uring/SPDK) is deferred — see `docs/future/ssd-store-linux-layer.md`.

## One-line summary

```text
CPU caches = execution locality.
RAM        = active map (primary state) + event log (evidence + bounded undo;
             replay is within-epoch recovery, not the source of truth — D018).
```

With this in the model, CINM is a layered in-memory machine where every learned
thing has a trace, a snapshot, rollback, and replay — all in RAM.
