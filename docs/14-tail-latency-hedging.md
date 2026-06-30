# 14. Tail-Latency Hedging And TailSlayer Fit

Status: research note only. No TailSlayer integration exists in this repo.

Reviewed: 2026-06-21

This note records where LaurieWired's TailSlayer idea might fit into CIM, and
where it should not be used. TailSlayer is a C++ library / experiment for
reducing tail latency in RAM reads caused by DRAM refresh stalls. The public
description is:

- GitHub: <https://github.com/LaurieWired/tailslayer>
- Article: <https://www.tomshardware.com/software/ambitious-hacker-reduces-worst-case-memory-latency-by-up-to-93-percent-but-with-severe-downsides-1960s-bottleneck-overcome-by-hedging-memory-accesses-to-avoid-running-into-dram-refresh-stalls>

## Short Assessment

TailSlayer is interesting for CIM, but not for the current core kernel.

The current `experiments/c-kernel` map is deliberately small and cache-friendly:

```text
MAX_CELLS = 256
NFEAT = 8
```

The current memory model also keeps the hot score/update path inside CPU cache
where possible. TailSlayer targets a different problem: rare but expensive DRAM
read stalls caused by refresh collisions. If the hot data fits in L1/L2/L3,
TailSlayer has little to fix.

**Measured at scale (cim-sys-scale-v1, decision A).** `scale_bench.c` swept the
real kernel to 1M cells with the doc-style percentile target below. The tail that
shows up first is **not** a DRAM-refresh tail — it is the linear-scan addressing
cost itself: at 1M cells `cinm_address` is p50 823 µs / p99 4.3 ms / max 7.4 ms,
and `cinm_address_nn` p99 13.4 ms. `score_ns_per_cell` stays flat ~4–6 ns. So the
first latency problem to solve is **algorithmic (index the scan)**, not a memory
hedge; TailSlayer stays a later, separate question for a large read-mostly bank.
Numbers in `experiments/c-kernel/runs/cim-sys-scale-v1/`.

**Done for exact-key addressing (cim-sys-scale-v2, Phase 3a).** Indexing the scan
removed this tail entirely: the `cinm_index` hash answers an exact-key lookup with a
single probe — ~47 ns at 1M instead of p50 823 µs / p99 4.3 ms — so there is **no
count-dependent tail left to hedge** on the exact-key path. The index is byte-exact
with the linear scan (`scale_index_check.c` green), which stays the reference. Two
tails remain open: (1) `cinm_address_nn` (p99 13.4 ms at 1M) now has an exact metric
index (`cinm_nn_index`, a static KD-tree, cim-sys-scale-v3), but at NFEAT=8 it is
backtracking-bound — it replaces the O(count) scan with O(log count)-ish backtracking
and lowers the **mean** ≈8.3× at 1M (6.2 ms → 0.74 ms), a constant factor rather than
the ~O(1) tail-elimination the exact-key hash gave. So the NN path's extreme linear
tail is gone but a smaller variable cost remains (its own percentiles are not
separately measured here — the bench reports means); an approximate index would shrink
it further but break byte-exactness. (2) The DRAM-refresh tail TailSlayer targets is
still a later question. Numbers in `experiments/c-kernel/runs/cim-sys-scale-v2/` and
`.../cim-sys-scale-v3/`.

The useful role is therefore a future optional experiment:

```text
large read-mostly RAM bank
  -> hedged read across replicas / channels
  -> lower p99 / p99.9 / p99.99 readout latency
  -> accept memory, bandwidth, and core cost explicitly
```

## Why TailSlayer Is Relevant At All

CIM is built around immediate readout from memory:

```text
event stream
  -> sparse context addressing
  -> local memory-cell activation
  -> bounded local update
  -> immediate readout
  -> periodic consolidation
```

If a later CIM backend has a large RAM-resident prototype bank, HDC bank, or
candidate pool, some readout paths may become DRAM-bound. In that regime average
latency is not the only metric. A rare long read can matter if the runtime has a
hard interactive or realtime deadline.

TailSlayer is relevant specifically to that tail-latency problem. It replicates
read-mostly data across independent memory placement regions, issues hedged
reads in parallel, and uses the first result that arrives. If one replica is
delayed by a DRAM refresh stall, another replica may return sooner.

## Candidate Fit Areas

### 1. Large HDC Or Prototype Bank Readout

Best fit.

The future SSD/Linux note already describes large read-mostly HDC banks and
prototype banks. If such banks are pulled into RAM or page cache and scanned or
sampled during readout, a TailSlayer-style experiment could compare:

```text
single-copy random read
single-copy indexed/chunked scan
two-replica hedged read
N-replica hedged read, only if the hardware has useful channels
```

The target metric should be a latency distribution:

```text
p50
p95
p99
p99.9
p99.99
max observed
```

Throughput alone is the wrong measure for this experiment.

### 2. Realtime Readout Shield

Possible later fit.

If a future runtime has a small deadline-sensitive readout path, TailSlayer could
wrap a tiny read-mostly bank used for the final decision. This would be a
specialized "tail shield" for immutable or rarely changed state, not the default
map representation.

The right shape would be:

```text
normal CIM state:
  compact, single-copy, replayable, update-friendly

optional tail-latency bank:
  replicated, read-mostly, explicitly expensive
```

### 3. Benchmark Harness

Near-term fit.

The safest first step is not integration. It is a benchmark gate:

```text
make run-tail-latency
```

The benchmark should allocate a bank larger than LLC, arrange one or more
replicas, perform randomized reads, and report the latency distribution with and
without hedging. It should also report the cost:

```text
replication factor
bytes allocated
threads/cores pinned
reads issued per logical read
estimated bandwidth pressure
```

This keeps the idea honest and prevents an impressive headline number from
becoming a default architecture.

## Poor Fit Areas

### Event Log And Replay

Not a good fit.

The in-RAM event log is append-only and replay-oriented. It benefits from
sequential layout, simple invariants, and deterministic replay. TailSlayer is
about hedged random-ish reads from replicated data, so it does not naturally
improve the log path.

### Mutable `cinm_update` State

Not a good fit.

Replicating mutable weights would complicate:

```text
transactions
rollback
deterministic replay
conflict handling
explainability
```

Under D018, CIM's primary state is the live map; reversibility comes from
snapshots + a bounded undo window, and the event log is evidence + within-epoch
replay (not a source of truth the map is rebuilt from). Tail-latency replication
should not weaken transactions, rollback, or within-epoch replay.

### SSD / `mmap` Page Faults

Not a direct fit.

TailSlayer does not solve storage latency or major page faults. It only becomes
relevant after the target data is resident in RAM. The SSD/page-cache path should
continue to use indexing, chunking, `mmap`, `madvise`, and prefetch hints first.

## Hardware Reality

The local machine is a laptop-class single-socket system:

```text
Intel Core Ultra 7 165H
64 GiB DDR5 SODIMM
single NUMA node
```

This is not the same environment as a many-channel server CPU. The reported
TailSlayer headline improvements from server systems should not be treated as a
local expectation.

For this repo, the realistic claim boundary is:

```text
TailSlayer may be worth testing for p99+ RAM-read tails on large read-mostly
banks. It is not a general speedup, not a current dependency, and not a default
CIM architecture.
```

## Proposed Backlog Item

### L. Tail-Latency DRAM Hedging Experiment

Goal: test whether hedged DRAM reads reduce p99+ readout latency for a large
read-mostly CIM bank enough to justify the memory, core, and bandwidth cost.

Initial scope:

- create an isolated `experiments/c-kernel/tail_latency_bench.c`
- keep it separate from `cinm_map`
- allocate a large read-mostly bank bigger than LLC
- support baseline and hedged modes
- report latency percentiles, not only average time
- report memory and core cost next to any improvement

Acceptance boundary:

```text
Adopt nothing into the core kernel unless:
  p99.9 or p99.99 improves materially
  correctness is byte-for-byte equivalent
  the extra memory/core/bandwidth cost is visible in the report
  deterministic replay and rollback stay unchanged
```

Non-goal:

```text
Do not convert the current small cache-resident map to TailSlayer.
```

