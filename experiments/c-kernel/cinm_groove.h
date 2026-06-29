/* cinm_groove.h — MAP-Elites groove archive (vertical N, M2). Lifts the cinm_selfadapt
 * archive shape from 1-D operator space to a 2-D behavior grid whose genome is a groove
 * (a phi vector in the kernel's representation). One elite per niche, lineage by parent
 * niche, a quarantine ring of rejected grooves as negative evidence. Neutral: the archive
 * treats phi as an opaque payload and never calls the kernel — the caller supplies the
 * behavior descriptor and the fitness (the M1 taste model is the selection pressure). */
#ifndef CINM_GROOVE_H
#define CINM_GROOVE_H

#include "cinm.h"        /* NFEAT (genome width) and clampf only — no kernel call */
#include <stdint.h>
#include <stddef.h>

enum { CINM_GROOVE_BINS = 6,                                   /* bins per behavior axis  */
       CINM_GROOVE_NICHES = CINM_GROOVE_BINS * CINM_GROOVE_BINS,
       CINM_GROOVE_QUARANTINE = 64 };                          /* reject ring capacity    */
constexpr uint32_t CINM_GROOVE_NONE = 0xFFFFFFFFu;                           /* "no parent" / seeded     */

_Static_assert(CINM_GROOVE_BINS >= 2, "a behavior axis needs at least two bins");

/* Why an offer did not become an elite. KEPT is the non-reject placeholder. */
enum cinm_groove_reason {
    CINM_GROOVE_KEPT = 0,
    CINM_GROOVE_DOMINATED = 1,      /* the target niche already holds a fitter elite      */
    CINM_GROOVE_OUT_OF_RANGE = 2,   /* the behavior descriptor fell outside [lo, hi]      */
};

/* An elite: the winning groove for one niche, with the lineage that produced it. */
typedef struct {
    float    phi[NFEAT];            /* genome (opaque to the archive)                     */
    float    bx, by;               /* behavior descriptor that placed it                 */
    double   fitness;
    uint32_t parent_a, parent_b;   /* parent niche indices (CINM_GROOVE_NONE if seeded)  */
    uint32_t born;                 /* logical step at which it was placed                 */
    bool     filled;
} cinm_groove_niche;

/* A rejected groove, retained as negative evidence with its reason. */
typedef struct {
    float    phi[NFEAT];
    float    bx, by;
    double   fitness;
    uint32_t niche;                /* niche it lost to (CINM_GROOVE_NONE if out of range) */
    int      reason;              /* enum cinm_groove_reason                             */
} cinm_groove_reject;

typedef struct {
    cinm_groove_niche  niches[CINM_GROOVE_NICHES];
    cinm_groove_reject quarantine[CINM_GROOVE_QUARANTINE];   /* ring; oldest overwritten */
    float    lo, hi;             /* behavior bounds, shared by both axes                 */
    uint32_t tried;              /* total offers                                          */
    uint32_t discarded;          /* offers that did not become an elite (== quarantined)  */
} cinm_groove_archive;

void   cinm_groove_init(cinm_groove_archive *a, float lo, float hi);

/* Niche index for a behavior point; *in_range is false when (bx,by) is outside [lo,hi]. */
size_t cinm_groove_niche_of(const cinm_groove_archive *a, float bx, float by, bool *in_range);

/* Offer an evaluated groove. Returns true if it became/replaced its niche elite (commit),
 * false if it was quarantined (out of range, or dominated by a fitter elite). Always
 * counts toward `tried`; a reject is appended to the quarantine ring and counts toward
 * `discarded`. parent_a/parent_b record lineage (CINM_GROOVE_NONE for a seeded groove). */
bool   cinm_groove_offer(cinm_groove_archive *a, const float phi[static NFEAT],
                         float bx, float by, double fitness,
                         uint32_t parent_a, uint32_t parent_b, uint32_t t);

size_t cinm_groove_filled(const cinm_groove_archive *a);

/* Retrievable reject count: min(discarded, ring capacity). */
size_t cinm_groove_quarantine_len(const cinm_groove_archive *a);

/* Fittest elite across all niches. Returns false if the archive is empty. */
bool   cinm_groove_best(const cinm_groove_archive *a, cinm_groove_niche *out);

#endif /* CINM_GROOVE_H */
