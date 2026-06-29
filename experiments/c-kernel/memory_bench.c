/* memory_bench.c — cache/RAM timing lines for CINM hot paths (in-memory only). */
#define _POSIX_C_SOURCE 200809L
#include "cinm.h"
#include "cinm_log.h"
#include <stdio.h>
#include <stdint.h>
#include <time.h>

enum { REPS = 2000, NEVENTS = 4000 };

static volatile float sinkf;
static volatile uint32_t sinku;

static uint64_t ns_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static void fill_map(cinm_map *m) {
    cinm_init(m);
    float dphi[NFEAT];
    for (int k = 0; k < NFEAT; k++)
        dphi[k] = (float)(k + 1) * 0.01f;
    for (uint32_t i = 0; i < MAX_CELLS; i++) {
        size_t cell = cinm_address(m, i, nullptr);
        cinm_update(m, cell, dphi, 1.0f, i);
        m->t++;
    }
}

static cinm_event event_at(uint32_t seq) {
    cinm_event ev = { .seq = seq, .type = 0, .key = seq % MAX_CELLS, .reward = 1.0f };
    for (int k = 0; k < NFEAT; k++)
        ev.dphi[k] = ((seq + (uint32_t)k) & 1u) ? 0.25f : -0.25f;
    return ev;
}

int main(void) {
    cinm_map m, tmp;
    fill_map(&m);
    float phi[NFEAT];
    for (int k = 0; k < NFEAT; k++)
        phi[k] = (float)(k + 1) * 0.02f;

    uint64_t t0 = ns_now();
    for (int r = 0; r < REPS; r++)
        for (uint32_t k = 0; k < MAX_CELLS; k++)
            sinku += (uint32_t)cinm_address(&m, k, nullptr);
    uint64_t t1 = ns_now();

    for (int r = 0; r < REPS; r++)
        for (size_t i = 0; i < m.count; i++)
            sinkf += cinm_score(&m, i, phi);
    uint64_t t2 = ns_now();

    for (int r = 0; r < REPS; r++)
        for (size_t i = 0; i < m.count; i++)
            cinm_update(&m, i, phi, 0.01f, (uint32_t)r);
    uint64_t t3 = ns_now();

    for (int r = 0; r < REPS * 16; r++) {
        cinm_snapshot(&m, &tmp);
        cinm_restore(&m, &tmp);
    }
    uint64_t t4 = ns_now();

    cinm_log log;
    cinm_log_init(&log);
    for (uint32_t i = 0; i < NEVENTS; i++) {
        cinm_event ev = event_at(i);
        if (!cinm_log_append(&log, &ev)) return 1;
    }
    cinm_init(&tmp);
    uint64_t t5 = ns_now();
    bool replay_ok = cinm_log_replay(&log, &tmp, 0);
    uint64_t t6 = ns_now();
    cinm_log_free(&log);

    printf("address_scan_ns_per_lookup=%.2f\n", (double)(t1 - t0) / (double)(REPS * MAX_CELLS));
    printf("score_ns_per_cell=%.2f\n", (double)(t2 - t1) / (double)(REPS * MAX_CELLS));
    printf("update_ns_per_cell=%.2f\n", (double)(t3 - t2) / (double)(REPS * MAX_CELLS));
    printf("snapshot_restore_ns=%.2f\n", (double)(t4 - t3) / (double)(REPS * 16));
    printf("replay_events_per_sec=%.2f\n", replay_ok ? (double)NEVENTS * 1e9 / (double)(t6 - t5) : 0.0);
    return replay_ok ? 0 : 1;
}
