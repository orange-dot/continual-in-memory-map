/* cinm_log.c — in-RAM event log: append + strict logical replay. No disk, no POSIX. */
#include "cinm_log.h"
#include <stdlib.h>

enum { EVENT_PAIRWISE = 0 };

void cinm_log_init(cinm_log *l) {
    l->events = nullptr;
    l->len = 0;
    l->cap = 0;
}

void cinm_log_free(cinm_log *l) {
    free(l->events);
    l->events = nullptr;
    l->len = 0;
    l->cap = 0;
}

bool cinm_log_append(cinm_log *l, const cinm_event *ev) {
    if (l->len == l->cap) {
        if (l->cap > SIZE_MAX / 2) return false;                /* cap * 2 would wrap */
        size_t cap = l->cap ? l->cap * 2 : 64;
        if (cap > SIZE_MAX / sizeof *l->events) return false;   /* byte size would wrap */
        cinm_event *grown = realloc(l->events, cap * sizeof *grown);
        if (!grown) return false;
        l->events = grown;
        l->cap = cap;
    }
    l->events[l->len++] = *ev;
    return true;
}

static bool replay_report_ok(const cinm_replay_report *r) {
    return !r->bad_sequence && !r->unsupported_type && !r->capacity_exceeded;
}

/* Validate the whole log's sequence, but apply only events in [from_seq, to_seq). */
static bool replay_window(const cinm_log *l, cinm_map *m, uint32_t from_seq, uint32_t to_seq,
                          cinm_replay_report *report) {
    cinm_replay_report r = {0};
    for (size_t i = 0; i < l->len; i++) {
        const cinm_event *ev = &l->events[i];
        /* Strict sequence: first event is seq 0, then monotone +1 with no gaps. */
        if (!r.saw_event) {
            if (ev->seq != 0) { r.bad_sequence = true; break; }
            r.saw_event = true;
        } else if (ev->seq != r.last_seq + 1) {
            r.bad_sequence = true;
            break;
        }
        r.last_seq = ev->seq;
        if (ev->seq == UINT32_MAX) { r.bad_sequence = true; break; }  /* guards +1 */
        if (ev->type != EVENT_PAIRWISE) { r.unsupported_type = true; break; }
        if (ev->seq >= from_seq && ev->seq < to_seq) {
            size_t idx = cinm_address(m, ev->key, nullptr);
            if (idx == MAX_CELLS) { r.capacity_exceeded = true; break; }
            cinm_update(m, idx, ev->dphi, ev->reward, ev->seq);
            if (m->t <= ev->seq) m->t = ev->seq + 1;
            r.applied++;
        }
    }
    if (report) *report = r;
    return replay_report_ok(&r);
}

bool cinm_log_replay_checked(const cinm_log *l, cinm_map *m, uint32_t from_seq,
                             cinm_replay_report *report) {
    return replay_window(l, m, from_seq, UINT32_MAX, report);
}

bool cinm_log_replay(const cinm_log *l, cinm_map *m, uint32_t from_seq) {
    cinm_replay_report r;
    return replay_window(l, m, from_seq, UINT32_MAX, &r);
}

bool cinm_log_replay_range(const cinm_log *l, cinm_map *m, uint32_t from_seq, uint32_t to_seq) {
    cinm_replay_report r;
    return replay_window(l, m, from_seq, to_seq, &r);
}
