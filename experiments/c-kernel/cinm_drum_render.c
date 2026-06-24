/* cinm_drum_render.c — groove phi -> type-0 Standard MIDI File on the GM drum channel, plus
 * a loss-map sidecar. The lowering table below is the modeling act (cim.drum_gm_grid.v1):
 * four GM voices, each driven by a density gene (even-spread hit grid) and a velocity gene.
 * No POSIX, no MIDI library — SMF is a handful of byte chunks. */
#include "cinm_drum_render.h"
#include <stdio.h>

/* lowering table: voice -> GM note, density gene index, velocity gene index, name, and a hit
 * floor. The floor is the groove backbone: kick and snare always sound at least their first
 * step, so a groove is never silent even when taste drives the density genes to zero. The
 * floor is lossy when it fires (realized > requested) and shows up in quant_error. */
static const int   NOTE[CINM_RENDER_VOICES]     = { 36, 38, 42, 46 };  /* kick snare c.hat o.hat */
static const int   DENS_IDX[CINM_RENDER_VOICES] = { 0, 2, 4, 6 };
static const int   VEL_IDX[CINM_RENDER_VOICES]  = { 1, 3, 5, 7 };
static const int   FLOOR[CINM_RENDER_VOICES]    = { 1, 1, 0, 0 };
static const char *NAME[CINM_RENDER_VOICES]     = { "kick", "snare", "closed_hat", "open_hat" };

enum { STEP_TICKS = CINM_RENDER_TPQN / 4,    /* 16th-note grid */
       GATE_TICKS = STEP_TICKS / 2 };        /* note length    */

/* MIDI variable-length quantity (big-endian, 7 bits/byte). Returns bytes written. */
static size_t vlq(unsigned char *out, uint32_t v) {
    unsigned char tmp[5];
    int i = 0;
    tmp[i++] = (unsigned char)(v & 0x7F);
    while ((v >>= 7)) tmp[i++] = (unsigned char)((v & 0x7F) | 0x80);
    size_t n = 0;
    while (i) out[n++] = tmp[--i];
    return n;
}

/* Lower phi to the hit grid + per-voice velocity, recording the loss. */
static void lower(const float phi[static NFEAT], unsigned char hit[CINM_RENDER_VOICES][CINM_RENDER_STEPS],
                  cinm_render_loss *L) {
    *L = (cinm_render_loss){ .note_count = 0, .quant_error = 0.0f };
    for (int v = 0; v < CINM_RENDER_VOICES; v++) {
        for (int s = 0; s < CINM_RENDER_STEPS; s++) hit[v][s] = 0;
        float d = clampf(phi[DENS_IDX[v]], 0.0f, 1.0f);
        int hits = (int)(d * (float)CINM_RENDER_STEPS + 0.5f);
        if (hits < FLOOR[v]) hits = FLOOR[v];                 /* groove backbone (see table) */
        for (int j = 0; j < hits; j++) hit[v][(j * CINM_RENDER_STEPS) / hits] = 1;  /* even spread, front-aligned */
        int placed = 0;
        for (int s = 0; s < CINM_RENDER_STEPS; s++) placed += hit[v][s];
        L->density_req[v] = d;
        L->hits[v] = placed;
        L->vel[v] = (int)clampf(40.0f + clampf(phi[VEL_IDX[v]], 0.0f, 1.0f) * 87.0f, 1.0f, 127.0f);
        L->note_count += placed;
        L->quant_error += absf(d - (float)placed / (float)CINM_RENDER_STEPS);
    }
}

size_t cinm_drum_render_encode(const float phi[static NFEAT], unsigned char *buf, size_t cap,
                               cinm_render_loss *loss) {
    unsigned char hit[CINM_RENDER_VOICES][CINM_RENDER_STEPS];
    cinm_render_loss L;
    lower(phi, hit, &L);

    /* track events: tempo (120 BPM = 500000 us/qn), then note on/off on the grid, then EOT. */
    unsigned char trk[CINM_RENDER_SMF_MAX];
    size_t tn = 0;
    const unsigned char tempo[] = { 0x00, 0xFF, 0x51, 0x03, 0x07, 0xA1, 0x20 };
    for (size_t i = 0; i < sizeof tempo; i++) trk[tn++] = tempo[i];

    uint32_t last = 0;
    for (uint32_t t = 0; t <= (uint32_t)(CINM_RENDER_STEPS * STEP_TICKS); t += GATE_TICKS) {
        bool onbeat = (t % (uint32_t)STEP_TICKS) == 0;
        int step = onbeat ? (int)(t / STEP_TICKS) : (int)((t - GATE_TICKS) / STEP_TICKS);
        if (step >= CINM_RENDER_STEPS) continue;
        for (int v = 0; v < CINM_RENDER_VOICES; v++) {
            if (!hit[v][step]) continue;
            tn += vlq(trk + tn, t - last);
            last = t;
            trk[tn++] = onbeat ? 0x99 : 0x89;                 /* note on / off, GM drum channel */
            trk[tn++] = (unsigned char)NOTE[v];
            trk[tn++] = onbeat ? (unsigned char)L.vel[v] : 0x40;
        }
    }
    const unsigned char eot[] = { 0x00, 0xFF, 0x2F, 0x00 };
    for (size_t i = 0; i < sizeof eot; i++) trk[tn++] = eot[i];

    size_t need = 14 + 8 + tn;
    if (need > cap) return 0;

    const unsigned char hdr[14] = {
        'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 0, 0, 1,
        (unsigned char)(CINM_RENDER_TPQN >> 8), (unsigned char)(CINM_RENDER_TPQN & 0xFF)
    };
    size_t n = 0;
    for (int i = 0; i < 14; i++) buf[n++] = hdr[i];
    buf[n++] = 'M'; buf[n++] = 'T'; buf[n++] = 'r'; buf[n++] = 'k';
    buf[n++] = (unsigned char)(tn >> 24); buf[n++] = (unsigned char)(tn >> 16);
    buf[n++] = (unsigned char)(tn >> 8);  buf[n++] = (unsigned char)(tn);
    for (size_t i = 0; i < tn; i++) buf[n++] = trk[i];

    if (loss) *loss = L;
    return n;
}

bool cinm_drum_render_write(const float phi[static NFEAT], const char *mid_path,
                            const char *lossmap_path) {
    unsigned char buf[CINM_RENDER_SMF_MAX];
    cinm_render_loss L;
    size_t n = cinm_drum_render_encode(phi, buf, sizeof buf, &L);
    if (n == 0) return false;

    FILE *f = fopen(mid_path, "wb");
    if (!f) return false;
    bool ok = fwrite(buf, 1, n, f) == n;
    fclose(f);
    if (!ok) return false;

    FILE *g = fopen(lossmap_path, "w");
    if (!g) return false;
    fprintf(g, "# midi-loss-map for %s\n", mid_path);
    fprintf(g, "schema = \"cim.drum_gm_grid.v1\"\n");
    fprintf(g, "# lossy lowering: continuous phi -> %d-step grid + 7-bit velocity (modeling act; may drift)\n",
            CINM_RENDER_STEPS);
    fprintf(g, "grid_steps = %d\nvoices = %d\ntpqn = %d\nnote_count = %d\nquant_error = %.4f\n",
            CINM_RENDER_STEPS, CINM_RENDER_VOICES, CINM_RENDER_TPQN, L.note_count, (double)L.quant_error);
    for (int v = 0; v < CINM_RENDER_VOICES; v++)
        fprintf(g, "voice.%s = { gm_note = %d, density_req = %.4f, hits = %d, realized = %.4f, velocity = %d }\n",
                NAME[v], NOTE[v], (double)L.density_req[v], L.hits[v],
                (double)L.hits[v] / (double)CINM_RENDER_STEPS, L.vel[v]);
    fclose(g);
    return true;
}
