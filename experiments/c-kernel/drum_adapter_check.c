/* drum_adapter_check.c — gate run-drum-adapter (vertical L): a fixed real-schema fixture
 * ingests to a golden cinm_event sequence; train/slate_eval are separable; a duplicate
 * record_id does not double-apply; the kernel sees only neutral cinm_events. */
#include "cinm.h"
#include "cinm_drum_adapter.h"
#include <stdio.h>

#define FIXTURE "testdata/drum_events.sample"

/* dphi is the unit vector e_idx (one component == v, the rest 0) */
static bool dphi_is(const cinm_event *e, int idx, float v) {
    for (int k = 0; k < NFEAT; k++)
        if (e->dphi[k] != (k == idx ? v : 0.0f)) return false;
    return true;
}
static bool dphi_zero(const cinm_event *e) {
    for (int k = 0; k < NFEAT; k++) if (e->dphi[k] != 0.0f) return false;
    return true;
}

int main(void) {
    cinm_drum_ingest in;
    int line_no = 0;
    if (!cinm_drum_ingest_file(FIXTURE, &in, &line_no)) {
        printf("ingest failed (line %d): FAIL\n", line_no);
        return 1;
    }

    bool counts = in.train_len == 6 && in.slate_len == 3
               && in.duplicates == 1 && in.ties_neither == 2;

    bool golden =
        in.train[0].key == 0 && in.train[0].reward == 0.90f && dphi_is(&in.train[0], 0, 1.0f) &&
        in.train[1].key == 1 && in.train[1].reward == 0.75f && dphi_is(&in.train[1], 0, 1.0f) &&
        in.train[2].reward == 0.0f && dphi_zero(&in.train[2]) &&             /* tie */
        in.slate[0].key == 2 && in.slate[0].reward == 0.80f && dphi_is(&in.slate[0], 1, 1.0f) &&
        in.slate[2].key == 1 && in.slate[2].reward == 0.85f && dphi_is(&in.slate[2], 0, 1.0f);

    bool seqs = true;                                   /* per-set seq is 0-based monotone */
    for (size_t i = 0; i < in.train_len; i++) if (in.train[i].seq != (uint32_t)i) seqs = false;
    for (size_t i = 0; i < in.slate_len; i++) if (in.slate[i].seq != (uint32_t)i) seqs = false;

    bool neutral = true;                                /* no drum symbol leaks: plain cinm_events */
    for (size_t i = 0; i < in.train_len; i++) if (in.train[i].type != 0) neutral = false;
    for (size_t i = 0; i < in.slate_len; i++) if (in.slate[i].type != 0) neutral = false;

    printf("golden cinm_event sequence: %s (train=%zu slate=%zu dup=%zu tie/neither=%zu)\n",
           (counts && golden) ? "PASS" : "FAIL",
           in.train_len, in.slate_len, in.duplicates, in.ties_neither);
    printf("train/slate phase split + per-set seq: %s\n", seqs ? "PASS" : "FAIL");
    printf("duplicate record_id not double-applied: %s\n", (in.duplicates == 1) ? "PASS" : "FAIL");
    printf("kernel sees only neutral cinm_events: %s\n", neutral ? "PASS" : "FAIL");

    return (counts && golden && seqs && neutral) ? 0 : 1;
}
