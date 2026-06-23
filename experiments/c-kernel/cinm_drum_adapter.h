/* cinm_drum_adapter.h — the single drum-aware ingestion seam (vertical L+M). Reads a flat
 * drum_events file (real aig_ranker schema, NFEAT phi per side) into cinm_events, split into a
 * train set and a held-out slate_eval set, deduplicated by record_id. Downstream the kernel
 * sees only phi/dphi + reward, never a drum symbol. No JSON/TOML library: a purpose-built line
 * reader for the fixed column order. Live ADG->phi extraction (aig-engine, Rust) is the
 * deferred upstream that produces these files for real grooves (D017). */
#ifndef CINM_DRUM_ADAPTER_H
#define CINM_DRUM_ADAPTER_H

#include "cinm.h"
#include "cinm_log.h"
#include <stddef.h>
#include <stdbool.h>

enum { CINM_DRUM_MAX_EVENTS = 256, CINM_DRUM_ID_MAX = 48 };

enum { DRUM_WIN_A = 0, DRUM_WIN_B = 1, DRUM_WIN_TIE = 2, DRUM_WIN_NEITHER = 3 };
enum { DRUM_PHASE_TRAIN = 0, DRUM_PHASE_SLATE = 1 };

typedef struct {
    cinm_event train[CINM_DRUM_MAX_EVENTS];   /* phase=train: learn on these          */
    size_t     train_len;
    cinm_event slate[CINM_DRUM_MAX_EVENTS];   /* phase=slate_eval: held-out gate       */
    size_t     slate_len;
    size_t     duplicates;                    /* records skipped: duplicate record_id  */
    size_t     ties_neither;                  /* tie/neither: emitted with dphi=0       */
} cinm_drum_ingest;

/* Ingest a flat drum_events file. Returns false on open/parse error or overflow; on a
 * malformed line *line_no is set. Each set gets seq = its own 0-based monotone index, so
 * train and slate each replay as a cinm_log. */
bool cinm_drum_ingest_file(const char *path, cinm_drum_ingest *out, int *line_no);

#endif /* CINM_DRUM_ADAPTER_H */
