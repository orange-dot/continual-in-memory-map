/* cinm_drum_adapter.c — flat drum_events -> cinm_events. No JSON/TOML library, no POSIX. */
#include "cinm_drum_adapter.h"
#include <stdio.h>
#include <string.h>

static bool seen_before(char seen[][CINM_DRUM_ID_MAX], size_t n, const char *id) {
    for (size_t i = 0; i < n; i++)
        if (strcmp(seen[i], id) == 0) return true;
    return false;
}

bool cinm_drum_ingest_file(const char *path, cinm_drum_ingest *out, int *line_no) {
    FILE *f = fopen(path, "r");
    if (!f) return false;

    *out = (cinm_drum_ingest){0};
    static char seen[CINM_DRUM_MAX_EVENTS][CINM_DRUM_ID_MAX];   /* dedup set (linear, fixture-scale) */
    size_t seen_len = 0;

    char line[512];
    int ln = 0;
    while (fgets(line, sizeof line, f)) {
        ln++;
        const char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0' || *p == '\n' || *p == '#') continue;       /* blank / comment */

        char id[CINM_DRUM_ID_MAX];
        int phase = 0, winner = 0;
        unsigned key = 0;
        float conf = 0.0f, a[NFEAT], b[NFEAT];
        int got = sscanf(line,
            "%47s %d %u %d %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f",
            id, &phase, &key, &winner, &conf,
            &a[0], &a[1], &a[2], &a[3], &a[4], &a[5], &a[6], &a[7],
            &b[0], &b[1], &b[2], &b[3], &b[4], &b[5], &b[6], &b[7]);
        if (got != 21) { if (line_no) *line_no = ln; fclose(f); return false; }

        if (seen_before(seen, seen_len, id)) { out->duplicates++; continue; }
        if (seen_len < CINM_DRUM_MAX_EVENTS)
            snprintf(seen[seen_len++], CINM_DRUM_ID_MAX, "%s", id);

        cinm_event ev = { .type = 0, .key = (uint32_t)key };
        if (winner == DRUM_WIN_A) {
            ev.reward = conf;
            for (int k = 0; k < NFEAT; k++) ev.dphi[k] = a[k] - b[k];
        } else if (winner == DRUM_WIN_B) {
            ev.reward = conf;
            for (int k = 0; k < NFEAT; k++) ev.dphi[k] = b[k] - a[k];
        } else {                                  /* tie / neither: non-silent no-op (dphi = 0) */
            ev.reward = 0.0f;
            for (int k = 0; k < NFEAT; k++) ev.dphi[k] = 0.0f;
            out->ties_neither++;
        }

        cinm_event *set = (phase == DRUM_PHASE_SLATE) ? out->slate : out->train;
        size_t *len     = (phase == DRUM_PHASE_SLATE) ? &out->slate_len : &out->train_len;
        if (*len >= CINM_DRUM_MAX_EVENTS) { if (line_no) *line_no = ln; fclose(f); return false; }
        ev.seq = (uint32_t)(*len);                /* per-set 0-based monotone -> replayable */
        set[(*len)++] = ev;
    }
    fclose(f);
    return true;
}
