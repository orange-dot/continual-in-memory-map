/* log_invariants_check.c — logical replay invariants for the in-RAM event log. */
#include "cinm.h"
#include "cinm_log.h"
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

static cinm_event one_event(uint32_t seq) {
    cinm_event ev = { .seq = seq, .type = 0, .key = 7, .reward = 1.0f };
    for (int k = 0; k < NFEAT; k++)
        ev.dphi[k] = (k & 1) ? 0.25f : -0.25f;
    return ev;
}

static bool replay_once(cinm_log *l, cinm_replay_report *r) {
    cinm_map m;
    cinm_init(&m);
    return cinm_log_replay_checked(l, &m, 0, r);
}

static bool clean_replay_pass(void) {
    cinm_log l;
    cinm_log_init(&l);
    cinm_event ev = one_event(0);
    bool ok = cinm_log_append(&l, &ev);
    cinm_replay_report r = {0};
    ok = ok && replay_once(&l, &r) && r.applied == 1 && r.last_seq == 0
       && !r.bad_sequence && !r.unsupported_type && !r.capacity_exceeded;
    cinm_log_free(&l);
    return ok;
}

static bool sequence_gap_rejected(void) {
    cinm_log l;
    cinm_log_init(&l);
    cinm_event ev0 = one_event(0), ev2 = one_event(2);
    bool ok = cinm_log_append(&l, &ev0) && cinm_log_append(&l, &ev2);
    cinm_replay_report r = {0};
    ok = ok && !replay_once(&l, &r) && r.bad_sequence;
    cinm_log_free(&l);
    return ok;
}

static bool duplicate_sequence_rejected(void) {
    cinm_log l;
    cinm_log_init(&l);
    cinm_event ev0 = one_event(0);
    bool ok = cinm_log_append(&l, &ev0) && cinm_log_append(&l, &ev0);
    cinm_replay_report r = {0};
    ok = ok && !replay_once(&l, &r) && r.bad_sequence;
    cinm_log_free(&l);
    return ok;
}

static bool unsupported_type_rejected(void) {
    cinm_log l;
    cinm_log_init(&l);
    cinm_event ev = one_event(0);
    ev.type = 99;
    bool ok = cinm_log_append(&l, &ev);
    cinm_replay_report r = {0};
    ok = ok && !replay_once(&l, &r) && r.unsupported_type;
    cinm_log_free(&l);
    return ok;
}

static bool capacity_overflow_rejected(void) {
    cinm_log l;
    cinm_log_init(&l);
    bool ok = true;
    for (uint32_t seq = 0; seq <= MAX_CELLS; seq++) {
        cinm_event ev = one_event(seq);
        ev.key = seq;
        ok = ok && cinm_log_append(&l, &ev);
    }
    cinm_replay_report r = {0};
    ok = ok && !replay_once(&l, &r) && r.capacity_exceeded;
    cinm_log_free(&l);
    return ok;
}

int main(void) {
    bool a = clean_replay_pass();
    bool b = sequence_gap_rejected();
    bool c = duplicate_sequence_rejected();
    bool d = unsupported_type_rejected();
    bool e = capacity_overflow_rejected();
    printf("clean replay: %s\n", a ? "PASS" : "FAIL");
    printf("sequence gap rejected: %s\n", b ? "PASS" : "FAIL");
    printf("duplicate sequence rejected: %s\n", c ? "PASS" : "FAIL");
    printf("unsupported type rejected: %s\n", d ? "PASS" : "FAIL");
    printf("capacity overflow rejected: %s\n", e ? "PASS" : "FAIL");
    return (a && b && c && d && e) ? 0 : 1;
}
