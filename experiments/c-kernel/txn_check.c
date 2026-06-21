/* txn_check.c — verify the in-memory reversible transaction (snapshot/rollback). */
#include "cinm.h"
#include <stdio.h>
#include <stdint.h>

/* A destructive candidate that the gate rejects: it trashes the weights, then
 * returns false, so cinm_transaction must roll the map back unchanged. */
static bool bad_candidate(cinm_map *m, void *ctx) {
    (void)ctx;
    for (size_t i = 0; i < m->count; i++)
        for (int k = 0; k < NFEAT; k++)
            m->w[i][k] = -7.0f;
    return false;
}

int main(void) {
    cinm_map map;
    cinm_init(&map);

    float phi[NFEAT];
    for (int k = 0; k < NFEAT; k++) phi[k] = (k % 2) ? 1.0f : -1.0f;
    for (uint32_t c = 0; c < 4; c++) {
        size_t i = cinm_address(&map, c, NULL);
        cinm_update(&map, i, phi, +1.0f, map.t++);
    }

    cinm_map before;
    cinm_snapshot(&map, &before);

    bool committed = cinm_transaction(&map, bad_candidate, NULL);
    bool identical = cinm_equal(&map, &before);

    printf("committed=%s  identical-after-rollback=%s -> %s\n",
           committed ? "true" : "false", identical ? "yes" : "no",
           (!committed && identical) ? "PASS" : "FAIL");
    return 0;
}
