/* drum_features_check.c — gate run-drum-features (vertical M): phi/dphi are deterministic
 * across runs, and a context activates a small cell set (|cells| << MAX_CELLS). */
#include "cinm.h"
#include "cinm_drum_adapter.h"
#include <stdio.h>
#include <string.h>

#define FIXTURE "testdata/drum_events.sample"

int main(void) {
    cinm_drum_ingest a, b;
    int ln = 0;
    if (!cinm_drum_ingest_file(FIXTURE, &a, &ln) || !cinm_drum_ingest_file(FIXTURE, &b, &ln)) {
        printf("ingest failed (line %d): FAIL\n", ln);
        return 1;
    }

    bool deterministic =
        a.train_len == b.train_len && a.slate_len == b.slate_len &&
        memcmp(a.train, b.train, a.train_len * sizeof a.train[0]) == 0 &&
        memcmp(a.slate, b.slate, a.slate_len * sizeof a.slate[0]) == 0;

    /* applying the train events activates one cell per event; the map holds only the
     * distinct context keys {0,1,2}, far below MAX_CELLS */
    cinm_map m;
    cinm_init(&m);
    for (size_t i = 0; i < a.train_len; i++) {
        size_t idx = cinm_address(&m, a.train[i].key, NULL);
        cinm_update(&m, idx, a.train[i].dphi, a.train[i].reward, a.train[i].seq);
    }
    bool sparse = m.count == 3 && m.count < (size_t)MAX_CELLS;

    printf("phi/dphi deterministic across runs: %s\n", deterministic ? "PASS" : "FAIL");
    printf("sparse activation (cells=%zu << MAX_CELLS=%d): %s\n",
           m.count, MAX_CELLS, sparse ? "PASS" : "FAIL");

    return (deterministic && sparse) ? 0 : 1;
}
