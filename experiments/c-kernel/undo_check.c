/* undo_check.c — prove R4 bounded undo (D018): undo within the horizon is byte-exact;
 * undo beyond the horizon or across a consolidation is refused and leaves the map intact. */
#include "cinm.h"
#include "cinm_log.h"
#include "cinm_undo.h"
#include <stdio.h>
#include <stdint.h>

enum { NSTEPS = 200, STRIDE = 8, HORIZON = 24 };   /* HORIZON <= (CINM_UNDO_RING-1)*STRIDE */

int main(void) {
    cinm_log log;
    cinm_log_init(&log);
    cinm_undo_window u;
    cinm_undo_init(&u, STRIDE, HORIZON);

    cinm_map M;
    cinm_init(&M);
    cinm_undo_mark(&u, &M, 0);

    for (uint32_t seq = 0; seq < NSTEPS; seq++) {
        cinm_event ev = { .seq = seq, .type = 0, .key = seq % 4, .reward = 1.0f };
        for (int k = 0; k < NFEAT; k++) ev.dphi[k] = (k == (int)(seq % NFEAT)) ? 0.5f : -0.1f;
        size_t i = cinm_address(&M, ev.key, NULL);
        cinm_update(&M, i, ev.dphi, ev.reward, seq);
        M.t = seq + 1;
        cinm_log_append(&log, &ev);
        cinm_undo_mark(&u, &M, seq + 1);
    }

    /* within horizon, non-checkpoint target: replay fills the gap from the nearest snapshot */
    uint32_t target = NSTEPS - 10;                 /* 10 back <= 24 */
    cinm_map ref;
    cinm_init(&ref);
    cinm_log_replay_range(&log, &ref, 0, target);  /* reference: state as of `target` events */

    cinm_map U = M;
    bool within = cinm_undo_to(&u, &log, &U, target, NSTEPS) && cinm_equal(&U, &ref);

    /* beyond the horizon -> refused, map untouched */
    cinm_map U2 = M;
    uint32_t far_target = NSTEPS - (HORIZON + 8); /* 32 back > 24 */
    bool refused_far = !cinm_undo_to(&u, &log, &U2, far_target, NSTEPS) && cinm_equal(&U2, &M);

    /* across a consolidation -> refused (window cleared, epoch base advanced) */
    const cinm_consolidate_policy pol = {
        .evict_floor = 0.0f, .evict_conf_max = 0.0f, .evict_idle_age = UINT32_MAX,
        .promote_conf = 2.0f, .promote_evidence = UINT32_MAX,   /* evict/freeze nothing */
    };
    cinm_consolidate(&M, &pol, M.t, NSTEPS);
    cinm_undo_clear(&u);
    cinm_map U3 = M;
    bool refused_epoch = !cinm_undo_to(&u, &log, &U3, target, NSTEPS) && cinm_equal(&U3, &M);

    printf("undo within horizon is byte-exact: %s\n", within ? "PASS" : "FAIL");
    printf("undo beyond horizon refused: %s\n", refused_far ? "PASS" : "FAIL");
    printf("undo across consolidation refused: %s\n", refused_epoch ? "PASS" : "FAIL");

    cinm_log_free(&log);
    return (within && refused_far && refused_epoch) ? 0 : 1;
}
