/* drum_render_check.c — gate run-drum-render (Q render, M2): lower a generated groove to a
 * real type-0 Standard MIDI File and decide. Evolves the N archive with the M1 taste model
 * as fitness, renders the best taste-adapted elite and a generic baseline to .mid + loss
 * map under runs/cim-drum-m2-render/, and checks: the SMF re-parses, the same phi is
 * byte-deterministic, each .mid ships a loss map, and the adapted groove out-scores the
 * baseline under the taste model (the doc-18 margin proxy). The human blind-A/B "sounds
 * good" claim, real ADG->aig-pc4-midi export, and sample-backend audition stay deferred to
 * D017 (doc 21: proxy != sounds-good); summary.md lists checked vs deferred. */
#include "cinm.h"
#include "cinm_drum_adapter.h"
#include "cinm_groove.h"
#include "cinm_drum_render.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define LEARN  "testdata/drum_events.learn"
#define RUNDIR "runs/cim-drum-m2-render"
enum { FIT_CTX = 0, G_SEED = 64, G_ITERS = 6000 };
constexpr float MUT = 0.15f;
constexpr float MARGIN_MIN = 0.10f;          /* taste-adapted must beat baseline by at least this     */
constexpr uint64_t ARCHIVE_SEED = 0x0DDC0FFEE5EEDULL;

static uint64_t rng = 0;
static uint64_t nx(void) {
    uint64_t z = (rng += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}
static float rnd01(void) { return (float)(nx() >> 40) / (float)(1u << 24); }

static void train(cinm_map *m, const cinm_drum_ingest *in) {
    cinm_init(m);
    for (size_t i = 0; i < in->train_len; i++) {
        size_t c = cinm_address(m, in->train[i].key, nullptr);
        cinm_update_adaptive(m, c, in->train[i].dphi, in->train[i].reward, in->train[i].seq);
    }
}

/* behavior axes: kick-density gene and hat-density gene (see groove_check / the lowering). */
static void behavior(const float phi[static NFEAT], float *bx, float *by) { *bx = phi[0]; *by = phi[4]; }

static size_t pick_filled(const cinm_groove_archive *a) {
    size_t start = (size_t)(nx() % CINM_GROOVE_NICHES);
    for (size_t d = 0; d < CINM_GROOVE_NICHES; d++)
        if (a->niches[(start + d) % CINM_GROOVE_NICHES].filled) return (start + d) % CINM_GROOVE_NICHES;
    return CINM_GROOVE_NICHES;
}

/* Evolve the taste-driven archive and return the fittest elite's genome in adapted. */
static size_t evolve(cinm_map *M, size_t cell0, float adapted[static NFEAT]) {
    rng = ARCHIVE_SEED;
    cinm_groove_archive a;
    cinm_groove_init(&a, 0.0f, 1.0f);
    uint32_t t = 0;
    for (int s = 0; s < G_SEED; s++, t++) {
        float phi[NFEAT];
        for (int k = 0; k < NFEAT; k++) phi[k] = rnd01();
        float bx, by; behavior(phi, &bx, &by);
        cinm_groove_offer(&a, phi, bx, by, (double)cinm_score(M, cell0, phi),
                          CINM_GROOVE_NONE, CINM_GROOVE_NONE, t);
    }
    for (int it = 0; it < G_ITERS; it++, t++) {
        float child[NFEAT];
        size_t pa = pick_filled(&a), pb = CINM_GROOVE_NONE;
        if (rnd01() < 0.5f) {
            pb = pick_filled(&a);
            for (int k = 0; k < NFEAT; k++) child[k] = (rnd01() < 0.5f) ? a.niches[pa].phi[k] : a.niches[pb].phi[k];
        } else {
            for (int k = 0; k < NFEAT; k++)
                child[k] = clampf(a.niches[pa].phi[k] + (rnd01() * 2.0f - 1.0f) * MUT, 0.0f, 1.0f);
        }
        float bx, by; behavior(child, &bx, &by);
        cinm_groove_offer(&a, child, bx, by, (double)cinm_score(M, cell0, child), (uint32_t)pa, (uint32_t)pb, t);
    }
    cinm_groove_niche best;
    cinm_groove_best(&a, &best);
    for (int k = 0; k < NFEAT; k++) adapted[k] = best.phi[k];
    return cinm_groove_filled(&a);
}

/* type-0 SMF container check: magic, format 0, one track, division, length, end-of-track. */
static bool smf_valid(const unsigned char *b, size_t n) {
    if (n < 22) return false;
    if (b[0]!='M'||b[1]!='T'||b[2]!='h'||b[3]!='d') return false;
    if (b[4]||b[5]||b[6]||b[7]!=6) return false;             /* header length 6 */
    if (b[8]||b[9]) return false;                            /* format 0        */
    if (b[10]||b[11]!=1) return false;                       /* one track       */
    if ((((uint32_t)b[12]<<8)|b[13]) != CINM_RENDER_TPQN) return false;
    if (b[14]!='M'||b[15]!='T'||b[16]!='r'||b[17]!='k') return false;
    uint32_t tlen = ((uint32_t)b[18]<<24)|((uint32_t)b[19]<<16)|((uint32_t)b[20]<<8)|b[21];
    if (22 + (size_t)tlen != n) return false;
    return b[n-3]==0xFF && b[n-2]==0x2F && b[n-1]==0x00;
}

static FILE *open_run(const char *name) {
    char path[256];
    snprintf(path, sizeof path, "%s/%s", RUNDIR, name);
    return fopen(path, "w");
}
static long run_fsize(const char *name) {
    char path[256];
    snprintf(path, sizeof path, "%s/%s", RUNDIR, name);
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long s = ftell(f);
    fclose(f);
    return s;
}

struct verdict {
    double adapted, baseline, margin;
    size_t smf_bytes, niches;
    cinm_render_loss loss;
    long adapted_loss, baseline_loss;
    bool valid, determ, lossmap, margin_ok, accept;
};

static void write_config(FILE *f) {
    fprintf(f,
        "task = \"cim-drum-m2-render\"\nmilestone = \"M2\"\n"
        "feature_schema_id = \"cim.drum_gesture_phi.v1\"\nlowering = \"cim.drum_gm_grid.v1\"\n"
        "grid_steps = %d\nvoices = %d\ntpqn = %d\n"
        "train_fixture = \"%s\"\nfitness_context = %d\n"
        "baseline = \"generic-flat-0.5\"\nmargin_threshold = %.2f\nniches = %d\n",
        CINM_RENDER_STEPS, CINM_RENDER_VOICES, CINM_RENDER_TPQN, LEARN, FIT_CTX,
        (double)MARGIN_MIN, CINM_GROOVE_NICHES);
}

static void write_metrics(FILE *f, const struct verdict *v) {
    static const char *NAME[CINM_RENDER_VOICES] = { "kick", "snare", "closed_hat", "open_hat" };
    fprintf(f,
        "{\n  \"adapted_score\": %.4f,\n  \"baseline_score\": %.4f,\n  \"margin\": %.4f,\n"
        "  \"smf_bytes\": %zu,\n  \"note_count\": %d,\n  \"quant_error\": %.4f,\n  \"niches_filled\": %zu,\n",
        v->adapted, v->baseline, v->margin, v->smf_bytes, v->loss.note_count,
        (double)v->loss.quant_error, v->niches);
    fprintf(f, "  \"voices\": {\n");
    for (int i = 0; i < CINM_RENDER_VOICES; i++)
        fprintf(f, "    \"%s\": { \"hits\": %d, \"velocity\": %d }%s\n",
                NAME[i], v->loss.hits[i], v->loss.vel[i], i + 1 < CINM_RENDER_VOICES ? "," : "");
    fprintf(f, "  }\n}\n");
}

static void write_decisions(FILE *f, const struct verdict *v) {
    fprintf(f, "{\"gate\":\"taste_margin\",\"pass\":%s,\"adapted\":%.4f,\"baseline\":%.4f,\"margin\":%.4f,\"threshold\":%.2f}\n",
            v->margin_ok ? "true" : "false", v->adapted, v->baseline, v->margin, (double)MARGIN_MIN);
    fprintf(f, "{\"gate\":\"smf_valid\",\"pass\":%s,\"bytes\":%zu,\"note_count\":%d}\n",
            v->valid ? "true" : "false", v->smf_bytes, v->loss.note_count);
    fprintf(f, "{\"gate\":\"deterministic\",\"pass\":%s}\n", v->determ ? "true" : "false");
    fprintf(f, "{\"gate\":\"lossmap_present\",\"pass\":%s,\"adapted_bytes\":%ld,\"baseline_bytes\":%ld}\n",
            v->lossmap ? "true" : "false", v->adapted_loss, v->baseline_loss);
    fprintf(f, "{\"gate\":\"no_runtime_authority\",\"pass\":true,\"basis\":\"file emit only (doc 19)\"}\n");
    fprintf(f, "{\"decision\":\"%s\",\"run\":\"cim-drum-m2-render\",\"reason_code\":\"%s\"}\n",
            v->accept ? "accept" : "reject", v->accept ? "" : "render_gate_fail");
}

static void write_summary(FILE *f, const struct verdict *v) {
    fprintf(f, "# cim-drum-m2-render — M2 render gate\n\nDecision: %s\n\n", v->accept ? "ACCEPT" : "REJECT");
    fprintf(f, "Renders the fittest taste-adapted elite from the N archive (run-groove-archive) to a real\n"
               "type-0 Standard MIDI File on the GM drum channel, beside a generic baseline groove.\n\n");
    fprintf(f, "## Checked (this gate)\n");
    fprintf(f, "- real type-0 SMF emitted and re-parsed (MThd/MTrk, format 0, one track, length consistent, ends FF 2F 00): %s\n",
            v->valid ? "PASS" : "FAIL");
    fprintf(f, "- byte-determinism: same phi -> byte-identical .mid: %s\n", v->determ ? "PASS" : "FAIL");
    fprintf(f, "- every exported .mid ships a *.midi-loss-map (present, non-empty): %s\n", v->lossmap ? "PASS" : "FAIL");
    fprintf(f, "- taste-margin proxy: adapted out-scores baseline under the M1 model (%.3f vs %.3f, margin %.3f >= %.2f): %s\n",
            v->adapted, v->baseline, v->margin, (double)MARGIN_MIN, v->margin_ok ? "PASS" : "FAIL");
    fprintf(f, "- zero runtime authority: file emit only, no pc4_transport / composition.json (doc 19): by construction\n\n");
    fprintf(f, "## Deferred to D017 (real upstream)\n");
    fprintf(f, "- human blind A/B \"sounds good\" (M2 acceptance criterion 1): proxy only here, not musical quality (doc 21)\n");
    fprintf(f, "- real ADG -> aig-pc4-midi lossy export: this gate writes a direct phi -> GM-grid SMF (modeling act, cim.drum_gm_grid.v1; may drift)\n");
    fprintf(f, "- sample-backend audition (EGMD / Virtuosity / Karoryfer)\n\n");
    fprintf(f, "## Inherited (not re-run here)\n");
    fprintf(f, "- diversity / no collapse: run-groove-archive (%zu/%d niches)\n", v->niches, CINM_GROOVE_NICHES);
    fprintf(f, "- reversibility / undo-N: run-txn, run-undo\n- forgetting / retention: run-consolidate, run-taste-loop\n\n");
    fprintf(f, "## Artifacts\n- adapted.mid + adapted.midi-loss-map (taste-adapted elite)\n");
    fprintf(f, "- baseline.mid + baseline.midi-loss-map (generic flat groove)\n");
    fprintf(f, "Both .mid files are playable in any GM-compatible player/DAW.\n");
}

static bool write_bundle(const struct verdict *v) {
    FILE *f;
    if ((f = open_run("config.toml")))     { write_config(f);       fclose(f); } else return false;
    if ((f = open_run("metrics.json")))    { write_metrics(f, v);   fclose(f); } else return false;
    if ((f = open_run("decisions.jsonl"))) { write_decisions(f, v); fclose(f); } else return false;
    if ((f = open_run("summary.md")))      { write_summary(f, v);   fclose(f); } else return false;
    return true;
}

int main(void) {
    int ln = 0;
    cinm_drum_ingest in;
    if (!cinm_drum_ingest_file(LEARN, &in, &ln)) {
        printf("render ingest (line %d): FAIL\n", ln);
        return 1;
    }
    cinm_map M;
    train(&M, &in);
    size_t cell0 = cinm_address(&M, FIT_CTX, nullptr);

    float adapted[NFEAT], baseline[NFEAT];
    for (int k = 0; k < NFEAT; k++) baseline[k] = 0.5f;   /* generic flat "session" groove */
    struct verdict v = { .niches = evolve(&M, cell0, adapted) };

    v.adapted  = (double)cinm_score(&M, cell0, adapted);
    v.baseline = (double)cinm_score(&M, cell0, baseline);
    v.margin   = v.adapted - v.baseline;
    v.margin_ok = v.margin >= (double)MARGIN_MIN;

    /* encode + re-parse + determinism */
    unsigned char b1[CINM_RENDER_SMF_MAX], b2[CINM_RENDER_SMF_MAX];
    size_t n1 = cinm_drum_render_encode(adapted, b1, sizeof b1, &v.loss);
    size_t n2 = cinm_drum_render_encode(adapted, b2, sizeof b2, nullptr);
    v.smf_bytes = n1;
    v.valid  = smf_valid(b1, n1) && v.loss.note_count > 0;
    v.determ = n1 > 0 && n1 == n2 && memcmp(b1, b2, n1) == 0;

    /* write the artifacts, then confirm each .mid has a non-empty loss map */
    bool wrote = cinm_drum_render_write(adapted,  RUNDIR "/adapted.mid",  RUNDIR "/adapted.midi-loss-map") &&
                 cinm_drum_render_write(baseline, RUNDIR "/baseline.mid", RUNDIR "/baseline.midi-loss-map");
    v.adapted_loss  = run_fsize("adapted.midi-loss-map");
    v.baseline_loss = run_fsize("baseline.midi-loss-map");
    v.lossmap = wrote && v.adapted_loss > 0 && v.baseline_loss > 0 &&
                run_fsize("adapted.mid") > 0 && run_fsize("baseline.mid") > 0;

    v.accept = v.valid && v.determ && v.lossmap && v.margin_ok;
    bool bundle = write_bundle(&v);

    printf("type-0 SMF container is valid (re-parsed): %s (bytes=%zu notes=%d)\n",
           v.valid ? "PASS" : "FAIL", v.smf_bytes, v.loss.note_count);
    printf("render is byte-deterministic (same phi -> same .mid): %s\n", v.determ ? "PASS" : "FAIL");
    printf("each .mid ships a non-empty *.midi-loss-map: %s (adapted=%ldB baseline=%ldB)\n",
           v.lossmap ? "PASS" : "FAIL", v.adapted_loss, v.baseline_loss);
    printf("taste shows up in the render (margin proxy, blind-A/B deferred D017): %s (adapted=%.3f base=%.3f margin=%.3f)\n",
           v.margin_ok ? "PASS" : "FAIL", v.adapted, v.baseline, v.margin);
    printf("doc-18 bundle written under %s/: %s\n", RUNDIR, bundle ? "PASS" : "FAIL");
    printf("render decision: %s\n", v.accept ? "ACCEPT" : "REJECT");

    return (v.accept && bundle) ? 0 : 1;
}
