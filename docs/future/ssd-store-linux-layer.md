# SSD Store And Linux Memory Layer

> **Status: DEFERRED — future hardware mapping, not in current scope.**
> Per decision D013 the kernel is strictly in-memory (CPU caches + RAM). The
> durable SSD store described below — append-only `events.log`, on-disk
> snapshots, manifests, mmap HDC banks, io_uring/SPDK — is parked here as a later
> mapping. None of it is currently implemented; the in-RAM event log lives in
> `experiments/c-kernel/cinm_log.{c,h}`.

Reviewed: 2026-06-21

This document records how local SSD storage should fit into CINM.

The short version:

```text
CPU caches = execution locality
RAM        = active working projection
SSD        = persistent replayable memory layer
```

The SSD should not be treated as slow RAM inside the hot loop. It should be
treated as the durable substrate for the largest and oldest things:

- append-only event logs
- evidence history
- candidate/adaptation records
- quarantine
- old snapshots
- large HDC banks
- prototype banks
- replay datasets
- learned preference corpus
- content-addressed artifacts

This must be integrated deeply into the design from the beginning, but the first
implementation should use mature Linux mechanisms before reaching for kernel
modules or raw NVMe userspace stacks.

## Memory Hierarchy

CINM should use a full hierarchy:

```text
registers
  -> L1 cache
  -> L2 cache
  -> L3 cache
  -> RAM
  -> SSD
```

Working model:

```text
L1:
  active cell, phi/dphi, local score/update

L2:
  chunk of cells, local candidate evaluation window, thread-local buffers

L3:
  shared read-mostly prototypes, compact archive summaries, immutable banks

RAM:
  current working map, active archive, rollback snapshots, hot HDC store

SSD:
  append-only truth, durable evidence, full archive, cold snapshots, large banks
```

The important conceptual inversion:

```text
RAM state is a projection.
SSD event log is the durable source of truth.
```

The system should be able to recover by:

```text
load latest snapshot
replay events after snapshot
reconstruct map/archive/gate state
resume adaptation
```

## What Belongs On SSD

SSD is for large, durable, replayable data.

Good SSD residents:

```text
events.log
  append-only event stream

snapshots/
  periodic full map/archive snapshots

candidates/
  candidate/adaptation records

quarantine/
  failed or suspicious adaptations retained as negative evidence

hdc-bank/
  large hypervector banks, prototype banks, immutable vector stores

indexes/
  compact lookup structures and offsets

manifests/
  schema versions, hashes, file layout, compatibility metadata

replay/
  deterministic replay datasets and test vectors

reports/
  benchmark and evaluation outputs
```

Poor SSD residents for the hot loop:

```text
active cell being updated every event
tiny score accumulators
per-token/per-event scratch state
thread-local counters
small rollback records for current transaction
```

Those belong in registers, caches, and RAM.

## First Store Layout

Suggested initial layout:

```text
store/
  manifest.toml
  events/
    events-000000.log
    events-000001.log
  snapshots/
    snap-000000.cinm
    snap-000001.cinm
  candidates/
    cand-000000.bin
    cand-000001.bin
  quarantine/
    q-000000.bin
  hdc-bank/
    bank-000000.hv
    bank-000001.hv
  indexes/
    events.idx
    hdc-bank.idx
  replay/
    run-000000.bin
  reports/
    bench-000000.txt
```

First implementation can use binary files with fixed headers instead of complex
databases.

Example file header:

```c
typedef struct {
    char     magic[8];      /* "CINMEVT" etc. */
    uint32_t version;
    uint32_t header_size;
    uint64_t created_at_event;
    uint64_t record_count;
    uint64_t payload_bytes;
    uint64_t checksum;
} cinm_file_header;
```

Do not overbuild this yet. The important parts are:

- explicit version
- explicit record size or schema
- append-only semantics where needed
- deterministic replay
- compatibility checks

## Transaction Model

Every reversible adaptation should become a transaction.

Conceptual flow:

```text
BEGIN candidate
  record candidate id, parent id, operator, parameters

SNAPSHOT or DELTA-BEGIN
  capture enough state to rollback

APPLY
  mutate working RAM state

EVALUATE
  run feedback/evaluation window

COMMIT or ROLLBACK
  commit useful adaptation or restore previous state

APPEND EVIDENCE
  record result, metrics, reason code
```

SSD should record this as an event stream:

```text
EV_CANDIDATE_PROPOSED
EV_TX_BEGIN
EV_DELTA_APPLIED
EV_EVAL_STARTED
EV_EVAL_RESULT
EV_TX_COMMIT
EV_TX_ROLLBACK
EV_QUARANTINE
EV_SNAPSHOT
```

The hot RAM state should not be considered authoritative until the corresponding
event has been durably appended.

## Durability Levels

Not every write needs the same durability.

Suggested levels:

```text
volatile
  scratch data; no sync; can be lost

recoverable
  written to file; page cache may delay disk write

durable
  fdatasync/fsync after important boundary

anchored
  manifest/checkpoint updated after data files are durable
```

First rule:

```text
commit events should be durable before the system claims the adaptation is committed.
```

In C:

```c
write(fd, buf, len);
fdatasync(fd);
```

Use `fdatasync` for append-only data where metadata does not need a full sync.
Use `fsync` when directory entries, renames, or manifest files must be durable.

For atomic file replacement:

```text
write temp file
fsync temp file
rename temp -> final
fsync parent directory
```

This matters for manifests and snapshots.

## Linux Integration Levels

### Level 1: POSIX Files

Use:

```text
open
read
write
pwrite
fsync
fdatasync
rename
```

Good for:

- append-only event logs
- small binary records
- snapshots
- deterministic, simple first implementation

This is the best starting point.

### Level 2: `mmap` + Page Cache

Use:

```text
mmap
munmap
msync
madvise
posix_fadvise
```

Good for:

- large read-mostly HDC banks
- prototype banks
- snapshot loading
- index files
- random-access stores where the kernel page cache can help

`mmap` makes the SSD-backed file appear as memory. The Linux kernel moves pages
between SSD and RAM on demand.

This effectively makes SSD a lower memory layer behind the page cache.

Useful hints:

```c
madvise(ptr, len, MADV_SEQUENTIAL);
madvise(ptr, len, MADV_RANDOM);
madvise(ptr, len, MADV_WILLNEED);
madvise(ptr, len, MADV_DONTNEED);
posix_fadvise(fd, offset, len, POSIX_FADV_SEQUENTIAL);
posix_fadvise(fd, offset, len, POSIX_FADV_RANDOM);
posix_fadvise(fd, offset, len, POSIX_FADV_WILLNEED);
posix_fadvise(fd, offset, len, POSIX_FADV_DONTNEED);
```

Use cases:

```text
large sequential replay:
  POSIX_FADV_SEQUENTIAL
  MADV_SEQUENTIAL

large random prototype lookup:
  POSIX_FADV_RANDOM
  MADV_RANDOM

preload next batch:
  POSIX_FADV_WILLNEED
  MADV_WILLNEED

drop cold pages after scan:
  POSIX_FADV_DONTNEED
  MADV_DONTNEED
```

### Level 3: `io_uring`

Use later for:

- asynchronous read/write
- batched I/O submission
- reduced syscall overhead
- prefetching next SSD ranges while CPU evaluates current batch

Good for:

- large sequential replay
- batch loading HDC bank chunks
- background snapshot writing
- log compaction

Not first. `io_uring` adds complexity and should sit behind a stable store API.

### Level 4: `O_DIRECT`

Use later for:

- large streaming reads/writes that should not pollute page cache
- benchmark-controlled I/O
- explicit buffer management

Tradeoff:

```text
more control
more alignment constraints
less help from kernel page cache
more ways to make mistakes
```

Not first.

### Level 5: SPDK / Raw NVMe / Custom Block Layout

Use only if the project proves it needs lower-than-kernel storage control.

Potential benefits:

- userspace NVMe queues
- very low overhead
- explicit I/O scheduling
- bypass kernel block stack

Costs:

- much more complexity
- harder debugging
- less ordinary filesystem tooling
- more operational risk
- more code that distracts from the CINM model

This is research-possible, but not the first integration.

## Recommended Backend Strategy

Define a store API that hides backend details:

```c
typedef struct cinm_store cinm_store;

int cinm_store_open(cinm_store **out, const char *root);
int cinm_store_close(cinm_store *s);

int cinm_store_append_event(cinm_store *s,
                            const void *record,
                            size_t record_size);

int cinm_store_write_snapshot(cinm_store *s,
                              const void *snapshot,
                              size_t snapshot_size);

int cinm_store_load_latest_snapshot(cinm_store *s,
                                    void *dst,
                                    size_t dst_size);

int cinm_store_map_hdc_bank(cinm_store *s,
                            const char *bank_id,
                            const void **ptr,
                            size_t *len);

int cinm_store_unmap_hdc_bank(cinm_store *s,
                              const void *ptr,
                              size_t len);

int cinm_store_prefetch_range(cinm_store *s,
                              const char *object_id,
                              uint64_t offset,
                              uint64_t len);

int cinm_store_drop_range(cinm_store *s,
                          const char *object_id,
                          uint64_t offset,
                          uint64_t len);
```

Backends:

```text
backend 1:
  POSIX read/write + mmap

backend 2:
  io_uring for async batch I/O

backend 3:
  O_DIRECT for controlled streaming

backend 4:
  SPDK/raw NVMe if needed
```

The CINM core should call the store API, not `fopen` everywhere.

## Page Cache As L4

Linux page cache is a major part of the design.

When using normal file I/O or `mmap`, Linux may keep recently used file pages in
RAM. That means the storage hierarchy is effectively:

```text
CPU caches
  -> process RAM
  -> Linux page cache
  -> SSD
```

For read-mostly HDC banks, this is valuable:

- frequently used prototype pages stay hot
- cold pages can be evicted by the kernel
- the program can request hints with `madvise`/`posix_fadvise`
- the store remains simple files on disk

Important distinction:

```text
process heap RAM:
  explicit state owned by CINM

page cache RAM:
  kernel-managed cache of file-backed data
```

Do not fight the page cache early. Use it.

## SSD Access Patterns

Good SSD patterns:

```text
append sequentially
read sequentially for replay
map large immutable files
read batches by offset
write snapshots as whole files
compact logs in background
```

Poor SSD patterns:

```text
tiny synchronous write per micro-update
random write amplification
fsync after every trivial metric
hot loop page faults
mixed log/snapshot/index writes in one file
unbounded small files
```

SSD integration should be deep, but not noisy.

Good:

```text
durable transaction boundary
batch evidence writes
periodic snapshot
background compaction
```

Bad:

```text
call fsync inside every inner score/update loop
```

## Event Log Design

The event log is the durable truth.

First binary event header:

```c
typedef enum {
    CINM_EV_CANDIDATE_PROPOSED = 1,
    CINM_EV_TX_BEGIN           = 2,
    CINM_EV_DELTA_APPLIED      = 3,
    CINM_EV_EVAL_STARTED       = 4,
    CINM_EV_EVAL_RESULT        = 5,
    CINM_EV_TX_COMMIT          = 6,
    CINM_EV_TX_ROLLBACK        = 7,
    CINM_EV_QUARANTINE         = 8,
    CINM_EV_SNAPSHOT           = 9
} cinm_event_kind;

typedef struct {
    uint64_t event_id;
    uint64_t prev_checksum;
    uint32_t kind;
    uint32_t version;
    uint64_t payload_len;
    uint64_t payload_checksum;
} cinm_event_header;
```

First payloads can be simple and fixed-size.

Reason codes:

```text
accepted_margin
rejected_low_margin
rejected_capacity
rejected_rollback_missing
rejected_bounds
rolled_back_regression
quarantined_suspicious
```

Design rule:

```text
If the event log cannot explain why state changed, the change should not exist.
```

## Snapshot Design

Snapshots are acceleration artifacts.

They are not the ultimate evidence source.

Flow:

```text
snapshot at event N
events after N remain in log
recovery = load snapshot N + replay events N+1...
```

First snapshot can be a raw serialized `cinm_map` plus header:

```text
magic
version
event_id covered
sizeof map
checksum
payload
```

Later snapshots may include:

- active archive
- candidate table
- store offsets
- compact indexes
- HDC bank manifest references

Snapshot write discipline:

```text
write temp snapshot
fsync temp snapshot
rename to final snapshot path
fsync snapshots directory
append EV_SNAPSHOT event
```

## HDC Bank Design

Large HDC banks belong naturally on SSD as file-backed arrays.

Bit-packed bank example:

```text
bank file:
  header
  vector_count
  words_per_vector
  vector records
```

Addressing:

```text
offset = header_size + vector_id * words_per_vector * sizeof(uint64_t)
```

Use `mmap` for read-mostly banks:

```text
map whole bank or large segment
scan contiguous vectors
use POPCNT similarity
drop cold pages after scan
```

Chunking:

```text
RAM hot set:
  currently active bank chunk

SSD cold set:
  full bank

index:
  compact RAM index from context -> candidate vector ranges
```

Do not randomly scan a multi-GB HDC bank on every event. Use context addressing,
indexes, and chunks.

## C API Boundary

Avoid storage calls in the scoring/update hot path.

Good split:

```text
cinm_core
  pure map/address/score/update/rollback in RAM

cinm_store
  events, snapshots, banks, manifests

cinm_runtime
  orchestrates transactions, evaluation, store durability
```

The core should not know whether storage is POSIX, `io_uring`, or SPDK.

The runtime should know when durability matters.

The store should know file layout and Linux hints.

## Linux Hints Cheat Sheet

For append-only event log:

```text
open(..., O_APPEND | O_CLOEXEC)
write records
fdatasync at commit boundary
```

For snapshot:

```text
open temp
write full snapshot
fsync temp
rename
fsync parent directory
```

For large sequential replay:

```text
posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL)
read in large buffers
```

For mapped HDC bank:

```text
fd = open(bank)
ptr = mmap(...)
madvise(ptr, len, MADV_RANDOM or MADV_SEQUENTIAL)
```

For prefetch:

```text
posix_fadvise(fd, offset, len, POSIX_FADV_WILLNEED)
madvise(ptr + offset, len, MADV_WILLNEED)
```

For dropping pages after one-time scan:

```text
posix_fadvise(fd, offset, len, POSIX_FADV_DONTNEED)
madvise(ptr + offset, len, MADV_DONTNEED)
```

## Kernel-Level Ambition

The user explicitly wants the option to go Linux-kernel-level or lower.

That is possible, but should be staged.

Stages:

```text
Stage 1:
  POSIX + mmap + page cache
  ordinary files
  simple binary formats

Stage 2:
  io_uring backend
  async prefetch and background snapshot writes

Stage 3:
  O_DIRECT streaming backend
  explicit aligned buffers
  controlled page-cache bypass

Stage 4:
  eBPF/perf tracing
  observe page faults, I/O latency, cache misses, block behavior

Stage 5:
  custom kernel module or block-layer experiment, only if justified

Stage 6:
  SPDK/raw NVMe userspace, if kernel bypass becomes a real research target
```

Do not start at Stage 5. It would make storage the project instead of CINM.

Start with Stage 1 but design the API so Stage 2/3 can replace the backend.

## Failure And Recovery

Required recovery behavior:

```text
crash before candidate commit:
  rollback or ignore incomplete transaction

crash after candidate commit event is durable:
  replay commit and reconstruct state

crash during snapshot write:
  ignore temp snapshot

crash after snapshot rename but before manifest update:
  discover snapshot by directory scan or previous manifest

corrupt event payload:
  stop replay at last valid event, quarantine tail
```

This is why append-only logs and checksums matter.

The system should be conservative:

```text
unknown tail == not committed
valid commit event == committed
```

## SSD Wear

A reversible adaptation machine can generate many events.

Do not write one tiny durable record per micro-operation.

Use:

- batching
- binary records
- segment files
- periodic snapshots
- background compaction
- commit-boundary syncing, not inner-loop syncing

Segment logs:

```text
events-000000.log
events-000001.log
events-000002.log
```

When a segment is old and covered by a snapshot, it can be compacted or archived.

## Security And Integrity

First version:

```text
checksums
monotonic event IDs
schema versions
manifest
reason codes
```

Later:

```text
hash chain
Merkle tree
signed manifests
content-addressed objects
```

Do not require cryptographic integrity for the first C kernel unless the test
requires it. Deterministic replay and simple checksums already buy a lot.

## Relationship To Reversible Self-Adaptation

The current project term is:

```text
interactive continual preference learning
  with reversible self-adaptation
```

SSD integration is what makes the "continual" part durable.

RAM can hold:

```text
what the system currently believes
```

SSD holds:

```text
why it believes it
what it tried
what failed
what was rolled back
what can be replayed
```

That distinction is essential.

Without SSD:

```text
rollback is local and temporary
evidence is fragile
archive is limited by RAM
```

With SSD:

```text
rollback becomes auditable
evidence survives restarts
archive can grow beyond RAM
large HDC banks become practical
experiments become replayable
```

## First Implementation Plan

For the C kernel:

```text
Phase 1:
  keep all active state in RAM
  implement bounded update and whole-map rollback

Phase 2:
  add store directory and manifest
  append transaction events as binary records
  no mmap yet

Phase 3:
  add snapshot write/load
  recovery = latest snapshot + event replay

Phase 4:
  add mmap-backed HDC bank file
  implement sequential scan and context-addressed scan

Phase 5:
  add posix_fadvise/madvise hints
  benchmark page-cache behavior

Phase 6:
  decide whether io_uring backend is justified
```

The first successful storage milestone:

```text
run adaptation
commit or rollback
exit process
restart
load snapshot/log
reconstruct same state
replay same evaluation
```

That is more important than raw I/O speed at the beginning.

## Bottom Line

The SSD layer should be designed as a durable memory substrate:

```text
SSD = append-only truth + cold memory + replay substrate
RAM = active projection + hot archive + rollback workspace
CPU caches = local execution hierarchy
```

The implementation should start with Linux files, `mmap`, page cache, and
explicit sync points. It should expose a storage API that can later grow
`io_uring`, `O_DIRECT`, kernel-level experiments, or SPDK without rewriting the
CINM core.

