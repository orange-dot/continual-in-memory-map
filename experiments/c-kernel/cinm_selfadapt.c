/* cinm_selfadapt.c — operator archive (MAP-Elites shape). No kernel/domain dependency. */
#include "cinm_selfadapt.h"

void cinm_sa_init(cinm_sa_archive *a, float lo, float hi) {
    for (size_t i = 0; i < CINM_SA_NICHES; i++) a->niches[i].filled = false;
    a->lo = lo;
    a->hi = hi;
    a->tried = 0;
    a->discarded = 0;
}

size_t cinm_sa_niche_of(const cinm_sa_archive *a, const cinm_op *op) {
    float span = a->hi - a->lo;
    if (span <= 0.0f) return 0;
    float f = (op->val - a->lo) / span;
    if (f < 0.0f) f = 0.0f;
    if (f > 0.999999f) f = 0.999999f;
    return (size_t)(f * (float)CINM_SA_NICHES);
}

bool cinm_sa_offer(cinm_sa_archive *a, const cinm_op *op, double score) {
    a->tried++;
    size_t n = cinm_sa_niche_of(a, op);
    if (!a->niches[n].filled || score > a->niches[n].score) {
        a->niches[n].op = *op;
        a->niches[n].score = score;
        a->niches[n].filled = true;
        return true;
    }
    a->discarded++;
    return false;
}

bool cinm_sa_best(const cinm_sa_archive *a, cinm_op *op_out, double *score_out) {
    bool found = false;
    double best = 0.0;
    cinm_op bop = { .val = 0.0f };
    for (size_t i = 0; i < CINM_SA_NICHES; i++) {
        if (!a->niches[i].filled) continue;
        if (!found || a->niches[i].score > best) {
            found = true;
            best = a->niches[i].score;
            bop = a->niches[i].op;
        }
    }
    if (found) {
        if (op_out) *op_out = bop;
        if (score_out) *score_out = best;
    }
    return found;
}
