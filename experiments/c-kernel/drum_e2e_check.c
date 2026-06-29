/* drum_e2e_check.c — gate run-drum-e2e: the L->M->O seam end to end on real-schema
 * files. (1) composition smoke on the committed golden fixture (the seam composes,
 * deterministically, with no drum-symbol leak); (2) scale learning on drum_events.learn
 * (context-addressed taste beats a context-blind baseline on a held-out slate). The
 * taste loop reads ONLY the adapter's cinm_events, never a drum symbol. Real human
 * bundles are the deferred upstream (D017). */
#include "cinm.h"
#include "cinm_drum_adapter.h"
#include <stdio.h>

#define SMOKE "testdata/drum_events.sample"
#define LEARN "testdata/drum_events.learn"

enum { NCTX_LEARN = 4 };       /* drum_events.learn has 4 contexts (keys 0..3) */
constexpr float MIN_WINRATE = 0.70f;      /* held-out floor that marks real learning, not noise */

/* Train a fresh map on the ingest's train set via the adaptive (margin-gated) update,
 * the O-path (doc 17). blind=true folds every context into the shared cell (key 0),
 * ignoring the law-axis address — the context-blind baseline. */
static void train(cinm_map *m, const cinm_drum_ingest *in, bool blind) {
    cinm_init(m);
    for (size_t i = 0; i < in->train_len; i++) {
        size_t c = cinm_address(m, blind ? 0u : in->train[i].key, nullptr);
        cinm_update_adaptive(m, c, in->train[i].dphi, in->train[i].reward, in->train[i].seq);
    }
}

/* Held-out win-rate: a slate event is correct when the addressed cell ranks the winner
 * above the loser, i.e. the margin against dphi (= phi_win - phi_los) is positive.
 * dphi == 0 (tie/neither) carries no preference and stays out of the denominator. All
 * slate keys exist in the train set, so addressing never allocates here. */
static float winrate(cinm_map *m, const cinm_drum_ingest *in, bool blind) {
    int correct = 0, scored = 0;
    for (size_t i = 0; i < in->slate_len; i++) {
        const float *dphi = in->slate[i].dphi;
        float n2 = 0.0f;
        for (int k = 0; k < NFEAT; k++) n2 += dphi[k] * dphi[k];
        if (n2 == 0.0f) continue;
        size_t c = cinm_address(m, blind ? 0u : in->slate[i].key, nullptr);
        if (cinm_margin(m, c, dphi) > 0.0f) correct++;
        scored++;
    }
    return scored ? (float)correct / (float)scored : 0.0f;
}

int main(void) {
    int ln = 0;

    /* (1) composition smoke on the committed golden fixture: same file ingested and
     * trained twice yields a byte-identical map, and nothing but neutral cinm_events
     * reaches the kernel. */
    cinm_drum_ingest s1, s2;
    if (!cinm_drum_ingest_file(SMOKE, &s1, &ln) || !cinm_drum_ingest_file(SMOKE, &s2, &ln)) {
        printf("e2e smoke ingest (line %d): FAIL\n", ln);
        return 1;
    }
    cinm_map m1, m2;
    train(&m1, &s1, false);
    train(&m2, &s2, false);
    bool neutral = true;
    for (size_t i = 0; i < s1.train_len; i++) if (s1.train[i].type != 0) neutral = false;
    for (size_t i = 0; i < s1.slate_len; i++) if (s1.slate[i].type != 0) neutral = false;
    bool smoke = cinm_equal(&m1, &m2) && neutral && s1.train_len > 0 && s1.slate_len > 0;
    printf("L->M->O seam composes (deterministic, no drum-symbol leak): %s "
           "(train=%zu slate=%zu cells=%zu)\n",
           smoke ? "PASS" : "FAIL", s1.train_len, s1.slate_len, m1.count);

    /* (2) scale learning: context-addressed taste beats the context-blind baseline on
     * a held-out slate, driven entirely through the file -> adapter seam. */
    cinm_drum_ingest in;
    if (!cinm_drum_ingest_file(LEARN, &in, &ln)) {
        printf("e2e learn ingest (line %d): FAIL\n", ln);
        return 1;
    }
    cinm_map M, Bl;
    train(&M, &in, false);
    train(&Bl, &in, true);
    float wr_cim  = winrate(&M,  &in, false);
    float wr_base = winrate(&Bl, &in, true);
    bool learns = wr_cim > wr_base + 0.05f && wr_cim >= MIN_WINRATE;
    bool sparse = M.count == NCTX_LEARN;

    printf("real-schema taste learns through the seam: %s (cim=%.3f base=%.3f)\n",
           learns ? "PASS" : "FAIL", (double)wr_cim, (double)wr_base);
    printf("sparse context activation (cells=%zu == contexts=%d): %s\n",
           M.count, NCTX_LEARN, sparse ? "PASS" : "FAIL");

    return (smoke && learns && sparse) ? 0 : 1;
}
