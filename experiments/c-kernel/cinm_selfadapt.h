/* cinm_selfadapt.h — Gödel-Darwin operator self-adaptation (D012, vertical P). An operator
 * is a proposed value for a learning hyperparameter (here: the decay / forgetting factor).
 * The archive is MAP-Elites-shaped: niches over the operator space, one elite per niche,
 * with a discarded count as lineage. Neutral: no kernel or domain dependency. */
#ifndef CINM_SELFADAPT_H
#define CINM_SELFADAPT_H

#include <stdint.h>
#include <stddef.h>

/* A candidate operator: a proposed hyperparameter value (e.g. a decay factor). */
typedef struct {
    float val;
} cinm_op;

enum { CINM_SA_NICHES = 8 };   /* value-bucket niches across [lo, hi] */

typedef struct {
    cinm_op op;
    double  score;
    bool    filled;
} cinm_sa_niche;

typedef struct {
    cinm_sa_niche niches[CINM_SA_NICHES];
    float    lo, hi;
    uint32_t tried;
    uint32_t discarded;
} cinm_sa_archive;

void   cinm_sa_init(cinm_sa_archive *a, float lo, float hi);

/* Niche index for an operator (by value bucket, clamped to [0, CINM_SA_NICHES-1]). */
size_t cinm_sa_niche_of(const cinm_sa_archive *a, const cinm_op *op);

/* Offer an evaluated operator. Returns true if it became/replaced its niche elite (a new
 * local best — "commit"), false if discarded ("rollback"). Always counts toward `tried`. */
bool   cinm_sa_offer(cinm_sa_archive *a, const cinm_op *op, double score);

/* Best elite across all niches. Returns false if the archive is empty. */
bool   cinm_sa_best(const cinm_sa_archive *a, cinm_op *op_out, double *score_out);

#endif /* CINM_SELFADAPT_H */
