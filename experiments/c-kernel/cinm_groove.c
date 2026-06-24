/* cinm_groove.c — MAP-Elites groove archive (N). Pure diversity bookkeeping over an opaque
 * phi genome; no kernel call, no domain knowledge. Mirrors cinm_selfadapt's commit/discard. */
#include "cinm_groove.h"

void cinm_groove_init(cinm_groove_archive *a, float lo, float hi) {
    for (size_t i = 0; i < CINM_GROOVE_NICHES; i++) a->niches[i].filled = false;
    a->lo = lo;
    a->hi = hi;
    a->tried = 0;
    a->discarded = 0;
}

/* Bucket one axis value into [0, BINS); reports range membership for the pair via *axis_ok. */
static size_t bin_of(float v, float lo, float hi, bool *axis_ok) {
    float span = hi - lo;
    if (span <= 0.0f) { *axis_ok = false; return 0; }
    if (v < lo || v > hi) { *axis_ok = false; }
    float f = clampf((v - lo) / span, 0.0f, 0.999999f);
    return (size_t)(f * (float)CINM_GROOVE_BINS);
}

size_t cinm_groove_niche_of(const cinm_groove_archive *a, float bx, float by, bool *in_range) {
    bool ok = true;
    size_t ix = bin_of(bx, a->lo, a->hi, &ok);
    size_t iy = bin_of(by, a->lo, a->hi, &ok);
    if (in_range) *in_range = ok;
    return ix * CINM_GROOVE_BINS + iy;
}

static void quarantine_push(cinm_groove_archive *a, const float phi[static NFEAT],
                            float bx, float by, double fitness, uint32_t niche, int reason) {
    cinm_groove_reject *r = &a->quarantine[a->discarded % CINM_GROOVE_QUARANTINE];
    *r = (cinm_groove_reject){ .bx = bx, .by = by, .fitness = fitness,
                               .niche = niche, .reason = reason };
    for (int k = 0; k < NFEAT; k++) r->phi[k] = phi[k];
    a->discarded++;
}

bool cinm_groove_offer(cinm_groove_archive *a, const float phi[static NFEAT],
                       float bx, float by, double fitness,
                       uint32_t parent_a, uint32_t parent_b, uint32_t t) {
    a->tried++;
    bool in_range = true;
    size_t n = cinm_groove_niche_of(a, bx, by, &in_range);
    if (!in_range) {
        quarantine_push(a, phi, bx, by, fitness, CINM_GROOVE_NONE, CINM_GROOVE_OUT_OF_RANGE);
        return false;
    }
    cinm_groove_niche *cell = &a->niches[n];
    if (!cell->filled || fitness > cell->fitness) {
        *cell = (cinm_groove_niche){ .bx = bx, .by = by, .fitness = fitness,
                                     .parent_a = parent_a, .parent_b = parent_b,
                                     .born = t, .filled = true };
        for (int k = 0; k < NFEAT; k++) cell->phi[k] = phi[k];
        return true;
    }
    quarantine_push(a, phi, bx, by, fitness, (uint32_t)n, CINM_GROOVE_DOMINATED);
    return false;
}

size_t cinm_groove_filled(const cinm_groove_archive *a) {
    size_t n = 0;
    for (size_t i = 0; i < CINM_GROOVE_NICHES; i++) if (a->niches[i].filled) n++;
    return n;
}

size_t cinm_groove_quarantine_len(const cinm_groove_archive *a) {
    return a->discarded < CINM_GROOVE_QUARANTINE ? a->discarded : CINM_GROOVE_QUARANTINE;
}

bool cinm_groove_best(const cinm_groove_archive *a, cinm_groove_niche *out) {
    bool found = false;
    double best = 0.0;
    for (size_t i = 0; i < CINM_GROOVE_NICHES; i++) {
        if (!a->niches[i].filled) continue;
        if (!found || a->niches[i].fitness > best) {
            found = true;
            best = a->niches[i].fitness;
            if (out) *out = a->niches[i];
        }
    }
    return found;
}
