/* cinm_undo.c — bounded undo window: snapshot ring + log replay. No disk, no POSIX. */
#include "cinm_undo.h"

void cinm_undo_init(cinm_undo_window *u, uint32_t stride, uint32_t horizon) {
    u->next = 0;
    u->stride = stride ? stride : 1;
    u->horizon = horizon;
    for (size_t i = 0; i < CINM_UNDO_RING; i++) u->used[i] = false;
}

void cinm_undo_clear(cinm_undo_window *u) {
    u->next = 0;
    for (size_t i = 0; i < CINM_UNDO_RING; i++) u->used[i] = false;
}

void cinm_undo_mark(cinm_undo_window *u, const cinm_map *m, uint32_t at) {
    if (at % u->stride != 0) return;
    u->snaps[u->next] = *m;
    u->at[u->next] = at;
    u->used[u->next] = true;
    u->next = (u->next + 1) % CINM_UNDO_RING;
}

bool cinm_undo_to(const cinm_undo_window *u, const cinm_log *log, cinm_map *m,
                  uint32_t target_seq, uint32_t now) {
    /* Outside the horizon, or before the current epoch base -> refuse, leave m untouched. */
    if (now - target_seq > u->horizon) return false;
    if (target_seq < m->base_seq) return false;

    /* Newest checkpoint at or before target_seq. */
    bool found = false;
    uint32_t best_at = 0;
    size_t best = 0;
    for (size_t i = 0; i < CINM_UNDO_RING; i++) {
        if (!u->used[i] || u->at[i] > target_seq) continue;
        if (!found || u->at[i] > best_at) { found = true; best_at = u->at[i]; best = i; }
    }
    if (!found) return false;       /* checkpoint rolled out of the ring */

    cinm_map tmp = u->snaps[best];
    if (!cinm_log_replay_range(log, &tmp, best_at, target_seq)) return false;
    *m = tmp;
    return true;
}
