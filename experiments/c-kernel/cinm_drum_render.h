/* cinm_drum_render.h — phi -> Standard MIDI File (vertical Q render, M2). The ONLY place a
 * groove's phi is lowered to drum voices; the kernel and the generator stay neutral behind
 * it, exactly as cinm_drum_adapter is the only drum-aware layer on the ingest side. Emits a
 * type-0 SMF on the GM drum channel plus a sidecar loss map for the lossy continuous->grid
 * lowering. No runtime authority: bytes/files only, no pc4_transport / composition.json
 * (doc 19). Real ADG -> aig-pc4-midi export and sample-backend audition are the deferred
 * upstream (D017); the lowering here is a modeling act and may drift. */
#ifndef CINM_DRUM_RENDER_H
#define CINM_DRUM_RENDER_H

#include "cinm.h"          /* NFEAT, clampf, absf */
#include <stddef.h>

enum { CINM_RENDER_STEPS = 16, CINM_RENDER_VOICES = 4, CINM_RENDER_TPQN = 96 };
_Static_assert(CINM_RENDER_TPQN % 4 == 0, "TPQN must divide into 16th-note steps");

/* Upper bound on the encoded SMF: MThd(14) + MTrk header(8) + tempo(7) + per-note on/off
 * (<=10 each, one note per voice per step) + end-of-track(4). */
enum { CINM_RENDER_SMF_MAX = 14 + 8 + 7 + CINM_RENDER_STEPS * CINM_RENDER_VOICES * 10 + 8 };

/* What the lossy lowering dropped: per voice the requested (continuous) density, the
 * quantized hit count, and the 7-bit velocity; plus totals. */
typedef struct {
    float density_req[CINM_RENDER_VOICES];
    int   hits[CINM_RENDER_VOICES];
    int   vel[CINM_RENDER_VOICES];
    int   note_count;
    float quant_error;        /* sum_v |density_req - hits/STEPS| */
} cinm_render_loss;

/* Encode the groove into a type-0 SMF in buf; returns bytes written (0 on overflow). Pure:
 * no I/O, so the same phi yields byte-identical output. *loss (if non-nullptr) receives the
 * lowering record. */
size_t cinm_drum_render_encode(const float phi[static NFEAT], unsigned char *buf, size_t cap,
                               cinm_render_loss *loss);

/* Encode + write the .mid and its .midi-loss-map sidecar. Returns false on any write error. */
bool   cinm_drum_render_write(const float phi[static NFEAT], const char *mid_path,
                              const char *lossmap_path);

#endif /* CINM_DRUM_RENDER_H */
