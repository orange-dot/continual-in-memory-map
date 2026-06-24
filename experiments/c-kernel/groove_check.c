/* groove_check.c — gate run-groove-archive: the N MAP-Elites groove generator (M2), driven
 * by the M1 taste model as the selection pressure. A "groove" is a phi vector; the archive
 * illuminates a 2-D behavior grid, one elite per niche. The taste map is trained from the
 * real-schema fixture through the L->M seam, so fitness = cinm_score under learned taste.
 * Proves: niches stay populated (no collapse), the population is bounded, selection beats
 * the random seeds, lineage is reconstructable, rejects are quarantined with a reason, and
 * the whole run is deterministic. No render here (that is run-drum-render); no Rust (D016). */
#include "cinm.h"
#include "cinm_drum_adapter.h"
#include "cinm_groove.h"
#include <stdio.h>
#include <stdint.h>

#define LEARN "testdata/drum_events.learn"
enum { FIT_CTX = 0, G_SEED = 64, G_ITERS = 6000 };
#define MUT 0.15f                 /* per-gene mutation jitter, clamped to [0,1]            */
#define COVER_MIN 32              /* filled niches required of 36: selection must illuminate */
#define ARCHIVE_SEED 0x0DDC0FFEE5EEDULL

static uint64_t rng = 0;
static uint64_t nx(void) {
    uint64_t z = (rng += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}
static float rnd01(void) { return (float)(nx() >> 40) / (float)(1u << 24); }

/* Train the taste map context-addressed through the adapter's events (the O-path). */
static void train(cinm_map *m, const cinm_drum_ingest *in) {
    cinm_init(m);
    for (size_t i = 0; i < in->train_len; i++) {
        size_t c = cinm_address(m, in->train[i].key, NULL);
        cinm_update_adaptive(m, c, in->train[i].dphi, in->train[i].reward, in->train[i].seq);
    }
}

/* Behavior descriptor: two interpretable gestures from the phi schema — bx the low-voice
 * (kick) density gene, by the high-voice (hat) density gene. Picking the niche axes from
 * phi is a modeling act (doc-17 density x syncopation) and may drift. */
static void behavior(const float phi[static NFEAT], float *bx, float *by) {
    *bx = phi[0];
    *by = phi[4];
}

static void mutate(const float src[static NFEAT], float dst[static NFEAT]) {
    for (int k = 0; k < NFEAT; k++)
        dst[k] = clampf(src[k] + (rnd01() * 2.0f - 1.0f) * MUT, 0.0f, 1.0f);
}
static void crossover(const float a[static NFEAT], const float b[static NFEAT], float dst[static NFEAT]) {
    for (int k = 0; k < NFEAT; k++) dst[k] = (rnd01() < 0.5f) ? a[k] : b[k];
}

static size_t pick_filled(const cinm_groove_archive *a) {
    size_t start = (size_t)(nx() % CINM_GROOVE_NICHES);
    for (size_t d = 0; d < CINM_GROOVE_NICHES; d++)
        if (a->niches[(start + d) % CINM_GROOVE_NICHES].filled) return (start + d) % CINM_GROOVE_NICHES;
    return CINM_GROOVE_NICHES;
}

/* One full MAP-Elites run from a fixed RNG seed: seed the archive with random grooves, then
 * evolve by mutation/crossover of existing elites. *seed_mean returns the mean seed fitness. */
static void run_archive(cinm_map *M, size_t cell0, uint64_t seed,
                        cinm_groove_archive *a, double *seed_mean) {
    rng = seed;
    cinm_groove_init(a, 0.0f, 1.0f);
    double ssum = 0.0;
    uint32_t t = 0;
    for (int s = 0; s < G_SEED; s++, t++) {
        float phi[NFEAT];
        for (int k = 0; k < NFEAT; k++) phi[k] = rnd01();
        float bx, by; behavior(phi, &bx, &by);
        double fit = (double)cinm_score(M, cell0, phi);
        ssum += fit;
        cinm_groove_offer(a, phi, bx, by, fit, CINM_GROOVE_NONE, CINM_GROOVE_NONE, t);
    }
    *seed_mean = ssum / (double)G_SEED;
    for (int it = 0; it < G_ITERS; it++, t++) {
        float child[NFEAT];
        size_t pa = pick_filled(a), pb = CINM_GROOVE_NONE;
        if (rnd01() < 0.5f) { pb = pick_filled(a); crossover(a->niches[pa].phi, a->niches[pb].phi, child); }
        else                  mutate(a->niches[pa].phi, child);
        float bx, by; behavior(child, &bx, &by);
        double fit = (double)cinm_score(M, cell0, child);
        cinm_groove_offer(a, child, bx, by, fit, (uint32_t)pa, (uint32_t)pb, t);
    }
}

static double elite_mean(const cinm_groove_archive *a) {
    double s = 0.0; size_t n = 0;
    for (size_t i = 0; i < CINM_GROOVE_NICHES; i++)
        if (a->niches[i].filled) { s += a->niches[i].fitness; n++; }
    return n ? s / (double)n : 0.0;
}

/* Determinism evidence: same count, offers, and fittest elite (down to the genome). */
static bool archive_equal(const cinm_groove_archive *x, const cinm_groove_archive *y) {
    if (cinm_groove_filled(x) != cinm_groove_filled(y)) return false;
    if (x->tried != y->tried || x->discarded != y->discarded) return false;
    cinm_groove_niche bx, by;
    bool fx = cinm_groove_best(x, &bx), fy = cinm_groove_best(y, &by);
    if (fx != fy) return false;
    if (fx && bx.fitness != by.fitness) return false;
    if (fx) for (int k = 0; k < NFEAT; k++) if (bx.phi[k] != by.phi[k]) return false;
    return true;
}

/* Lineage: each elite sits in the niche its behavior maps to, cites in-range (or NONE)
 * parent niches, and at least one elite is bred (selection ran beyond the seeds). The
 * seeds are the roots of the lineage tree; selection may overwrite all of them as current
 * elites, so depth — not surviving roots — is the reconstructable property. */
static bool lineage_ok(const cinm_groove_archive *a, size_t *bred) {
    size_t b = 0;
    for (size_t i = 0; i < CINM_GROOVE_NICHES; i++) {
        if (!a->niches[i].filled) continue;
        bool inr = true;
        if (cinm_groove_niche_of(a, a->niches[i].bx, a->niches[i].by, &inr) != i || !inr) return false;
        uint32_t pa = a->niches[i].parent_a, pb = a->niches[i].parent_b;
        if (pa != CINM_GROOVE_NONE) { if (pa >= CINM_GROOVE_NICHES) return false; b++; }
        if (pb != CINM_GROOVE_NONE && pb >= CINM_GROOVE_NICHES) return false;
    }
    if (bred) *bred = b;
    return b >= 1;
}

/* Every quarantined reject carries a known reason; dominated rejects name a valid niche. */
static bool quarantine_ok(const cinm_groove_archive *a, int *dominated) {
    size_t qn = cinm_groove_quarantine_len(a);
    int dom = 0;
    for (size_t i = 0; i < qn; i++) {
        int rs = a->quarantine[i].reason;
        if (rs != CINM_GROOVE_DOMINATED && rs != CINM_GROOVE_OUT_OF_RANGE) return false;
        if (rs == CINM_GROOVE_DOMINATED) { if (a->quarantine[i].niche >= CINM_GROOVE_NICHES) return false; dom++; }
    }
    if (dominated) *dominated = dom;
    return qn > 0;
}

int main(void) {
    int ln = 0;
    cinm_drum_ingest in;
    if (!cinm_drum_ingest_file(LEARN, &in, &ln)) {
        printf("groove ingest (line %d): FAIL\n", ln);
        return 1;
    }
    cinm_map M;
    train(&M, &in);
    size_t cell0 = cinm_address(&M, FIT_CTX, NULL);

    cinm_groove_archive a, b;
    double seed_mean = 0.0, seed_mean_b = 0.0;
    run_archive(&M, cell0, ARCHIVE_SEED, &a, &seed_mean);
    run_archive(&M, cell0, ARCHIVE_SEED, &b, &seed_mean_b);

    size_t filled = cinm_groove_filled(&a);
    double elite = elite_mean(&a);
    cinm_groove_niche best;
    cinm_groove_best(&a, &best);

    bool populated = filled >= COVER_MIN;
    bool bounded   = filled <= CINM_GROOVE_NICHES && a.tried == (uint32_t)(G_SEED + G_ITERS) && a.discarded > 0;
    bool selects   = elite > seed_mean;

    size_t bred = 0;
    bool lineage = lineage_ok(&a, &bred);

    int dominated = 0;
    bool quarantined = quarantine_ok(&a, &dominated);

    /* A deliberately out-of-range groove is quarantined as OUT_OF_RANGE (negative evidence). */
    cinm_groove_archive oor;
    cinm_groove_init(&oor, 0.0f, 1.0f);
    float g[NFEAT]; for (int k = 0; k < NFEAT; k++) g[k] = 0.5f;
    cinm_groove_offer(&oor, g, 0.5f, 0.5f, 0.0, CINM_GROOVE_NONE, CINM_GROOVE_NONE, 0);
    bool oor_rejected = !cinm_groove_offer(&oor, g, 2.0f, 0.5f, 0.0, CINM_GROOVE_NONE, CINM_GROOVE_NONE, 1);
    bool oor_reason = cinm_groove_quarantine_len(&oor) >= 1 &&
        oor.quarantine[(oor.discarded - 1) % CINM_GROOVE_QUARANTINE].reason == CINM_GROOVE_OUT_OF_RANGE;

    bool determ = archive_equal(&a, &b);

    printf("niches stay populated (no collapse): %s (filled=%zu/%d, need>=%d)\n",
           populated ? "PASS" : "FAIL", filled, CINM_GROOVE_NICHES, COVER_MIN);
    printf("population bounded by the niche grid: %s (tried=%u discarded=%u filled<=%d)\n",
           bounded ? "PASS" : "FAIL", a.tried, a.discarded, CINM_GROOVE_NICHES);
    printf("selection beats the random seeds: %s (elite=%.3f seed=%.3f)\n",
           selects ? "PASS" : "FAIL", elite, seed_mean);
    printf("lineage reconstructable: %s (bred=%zu, best niche parents=%u/%u fit=%.3f)\n",
           lineage ? "PASS" : "FAIL", bred, best.parent_a, best.parent_b, best.fitness);
    printf("rejects quarantined with a reason: %s (retained=%zu dominated=%d out-of-range=%s)\n",
           (quarantined && oor_rejected && oor_reason) ? "PASS" : "FAIL",
           cinm_groove_quarantine_len(&a), dominated, (oor_rejected && oor_reason) ? "yes" : "no");
    printf("groove run is deterministic: %s\n", determ ? "PASS" : "FAIL");

    bool ok = populated && bounded && selects && lineage && quarantined && oor_rejected && oor_reason && determ;
    return ok ? 0 : 1;
}
