/* cinm_log.h — in-RAM event log: append-only learning events, replayable truth.
 * RAM is the only tier; the map is a projection of this log, reconstructed by
 * deterministic replay. No disk, no persistence across process restart. */
#ifndef CINM_LOG_H
#define CINM_LOG_H

#include "cinm.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* A layout-independent learning event — the replayable source of truth.
 * Records semantics, not in-memory layout, so it survives kernel refactors. */
typedef struct {
    uint32_t seq;
    uint32_t type;        /* 0 = pairwise update */
    uint32_t key;         /* context id */
    float    reward;
    float    dphi[NFEAT];
} cinm_event;

/* Append-only event log, growable, RAM-resident. */
typedef struct {
    cinm_event *events;
    size_t      len;
    size_t      cap;
} cinm_log;

/* Replay outcome — logical invariants only (no disk/codec failure modes). */
typedef struct {
    uint32_t applied;
    uint32_t last_seq;
    bool saw_event;
    bool bad_sequence;
    bool unsupported_type;
    bool capacity_exceeded;
} cinm_replay_report;

void cinm_log_init(cinm_log *l);
void cinm_log_free(cinm_log *l);

/* Append one event (copied in). Returns false only on allocation failure. */
bool cinm_log_append(cinm_log *l, const cinm_event *ev);

/* Replay events with seq >= from_seq into m (apply via cinm_update).
 * A "snapshot" for recovery is just an in-RAM cinm_snapshot of the map plus the
 * log length at that point, replayed forward — see replay_check.c. */
bool cinm_log_replay(const cinm_log *l, cinm_map *m, uint32_t from_seq);
bool cinm_log_replay_checked(const cinm_log *l, cinm_map *m, uint32_t from_seq,
                             cinm_replay_report *report);

#endif /* CINM_LOG_H */
