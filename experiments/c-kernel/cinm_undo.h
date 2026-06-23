/* cinm_undo.h — bounded undo window (D018, R4): a ring of map snapshots plus the
 * event log gives fine-grained "undo last N" within a retention horizon H. Undo never
 * crosses a consolidation boundary (R3 truncates the window at the epoch base). RAM-only. */
#ifndef CINM_UNDO_H
#define CINM_UNDO_H

#include "cinm.h"
#include "cinm_log.h"

enum { CINM_UNDO_RING = 4 };   /* snapshots retained; horizon <= (RING-1)*stride */

typedef struct {
    cinm_map snaps[CINM_UNDO_RING];
    uint32_t at[CINM_UNDO_RING];   /* events-applied position of each snapshot */
    bool     used[CINM_UNDO_RING];
    size_t   next;                 /* ring write cursor */
    uint32_t stride;               /* checkpoint every `stride` events */
    uint32_t horizon;              /* H: furthest back (in events) undo may reach */
} cinm_undo_window;

void cinm_undo_init(cinm_undo_window *u, uint32_t stride, uint32_t horizon);

/* Checkpoint the map if `at` is on a stride boundary. Call after each logged event. */
void cinm_undo_mark(cinm_undo_window *u, const cinm_map *m, uint32_t at);

/* Drop all checkpoints (consolidation calls this: undo cannot reach a prior epoch). */
void cinm_undo_clear(cinm_undo_window *u);

/* Reconstruct the map as of `target_seq` events: restore the nearest checkpoint at or
 * before target_seq and replay the log up to target_seq. Returns false and leaves m
 * untouched if target_seq is outside the retention window — beyond the horizon from
 * `now`, before the current epoch base (m->base_seq), or with no usable checkpoint. */
bool cinm_undo_to(const cinm_undo_window *u, const cinm_log *log, cinm_map *m,
                  uint32_t target_seq, uint32_t now);

#endif /* CINM_UNDO_H */
