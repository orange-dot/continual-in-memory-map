/* drum_gate_check.c — gate run-drum-gate (Q, M1 decision gate). Trains taste on the
 * bootstrap (phase 0) of a transcribed drummer fixture, then accepts/rejects on held-out
 * (phase 1): hard gates (bounds, capacity, rollback-availability, anti-hack) + a
 * GOLDEN_SET held-out check (ideal > bad, catch trials, revision flips) + an ANTI_PATTERNS
 * critic. The GOLDEN_SET / ANTI_PATTERNS content is a transcription (snapshot) of the
 * pc4-microkit-studio drummer authority docs; see testdata/golden_set.sample. Each run
 * writes a doc-18 named bundle under runs/. Real human bundles are deferred (D017). */
#include "cinm.h"
#include "cinm_drum_adapter.h"
#include <stdio.h>
#include <string.h>

#define FIXTURE "testdata/golden_set.sample"
#define RUNDIR  "runs/cim-drum-m1-gate"
#define N_GOLD  10
#define N_SLATE 12
#define N_AP    6
enum { REVISION_CORRECTIONS = 40 };   /* in-gate corrections that must flip a revision */

/* The six named modes of ANTI_PATTERNS.md, verbatim. In M1 the kernel sees only
 * phi/dphi+reward and generates no grooves, so two modes that need dialogue/generation
 * are honestly DEFERRED to M2 (step N), not silently dropped (doc 18). */
enum ap_mode { AP_CHECKED, AP_DEFERRED };
static const struct { const char *name; enum ap_mode mode; const char *note; } AP[N_AP] = {
    { "density-inflation",       AP_CHECKED,  "over-dense masquerading as effort" },
    { "fill-spam",              AP_CHECKED,  "constant fills; no silence law" },
    { "visible-odd-meter",      AP_CHECKED,  "odd meter as a badge, not a body" },
    { "session-genericity",     AP_CHECKED,  "tasteful but anonymous" },
    { "counterproposal-theater", AP_DEFERRED, "needs dialogue/generation (M2/N)" },
    { "source-imitation",       AP_DEFERRED, "needs source-similarity (M2/N)" },
};

/* One held-out slate label, 1:1 with the phase-1 rows of FIXTURE (see its CONTRACT
 * note). kind selects how the row is scored. */
enum gkind { GOLD, REVISION, CATCH_TIE, CATCH_DEGRADED };
static const struct { const char *id; const char *cls; const char *ap; enum gkind kind; } CATALOG[N_SLATE] = {
    { "g0", "atmosphere", "fill-spam",          GOLD },
    { "g1", "atmosphere", "density-inflation",  GOLD },
    { "g2", "atmosphere", "session-genericity", GOLD },
    { "g3", "meter",      "visible-odd-meter",  GOLD },
    { "g4", "section",    "density-inflation",  GOLD },
    { "g5", "meter",      "visible-odd-meter",  GOLD },
    { "g6", "cell",       "session-genericity", GOLD },
    { "g7", "cell",       "density-inflation",  GOLD },
    { "g8", "revision",   "density-inflation",  REVISION },
    { "g9", "revision",   "session-genericity", REVISION },
    { "c_aa",   "catch",  "-",                  CATCH_TIE },
    { "c_degr", "catch",  "-",                  CATCH_DEGRADED },
};
_Static_assert(sizeof CATALOG / sizeof CATALOG[0] == N_SLATE, "CATALOG size");
_Static_assert(sizeof AP / sizeof AP[0] == N_AP, "AP size");

struct verdict {
    bool bounds, capacity, rollback, anti_hack;
    int  gold_correct, gold_total, rev_flips, rev_total;
    bool catch_tie, catch_degr, pass[N_SLATE];
    int  ap_total[N_AP], ap_correct[N_AP];
    size_t cells;
    float conflict_rate;
    uint32_t max_clamp, updates;
    bool accept;
    const char *reason;
};

static void train(cinm_map *m, const cinm_drum_ingest *in, struct verdict *v) {
    cinm_init(m);
    for (size_t i = 0; i < in->train_len; i++) {
        size_t c = cinm_address(m, in->train[i].key, NULL);
        cinm_update_result r = cinm_update_adaptive(m, c, in->train[i].dphi,
                                                    in->train[i].reward, in->train[i].seq);
        v->updates++;
        if (r.conflict) v->conflict_rate += 1.0f;
        if (r.clamp_count > v->max_clamp) v->max_clamp = r.clamp_count;
    }
    if (v->updates) v->conflict_rate /= (float)v->updates;
}

/* Margin of cell(key) against dphi. Every golden/catch key was bootstrapped, so address
 * resolves an existing cell and does not mutate the map. */
static float margin_at(cinm_map *m, uint32_t key, const float dphi[static NFEAT]) {
    return cinm_margin(m, cinm_address(m, key, NULL), dphi);
}

static bool dphi_zero(const cinm_event *e) {
    for (int k = 0; k < NFEAT; k++) if (e->dphi[k] != 0.0f) return false;
    return true;
}

static bool bounds_ok(const cinm_map *m) {
    if (m->count > MAX_CELLS) return false;
    for (size_t i = 0; i < m->count; i++)
        for (int k = 0; k < NFEAT; k++)
            if (absf(m->w[i][k]) > W_MAX) return false;
    return true;
}

/* Rollback-availability (reuses run-txn semantics): a snapshot must restore byte-exact
 * after a real mutation. */
static bool rollback_ok(const cinm_map *base) {
    cinm_map work = *base, snap;
    cinm_snapshot(&work, &snap);
    cinm_decay(&work, 0.5f);
    bool changed = !cinm_equal(&work, &snap);
    cinm_restore(&work, &snap);
    return changed && cinm_equal(&work, base);
}

static int ap_index(const char *name) {
    for (int j = 0; j < N_AP; j++) if (strcmp(AP[j].name, name) == 0) return j;
    return -1;
}

/* --- doc-18 named-run bundle (hand-emitted; no JSON/TOML library) --------------- */

static FILE *open_run(const char *name) {
    char path[256];
    snprintf(path, sizeof path, "%s/%s", RUNDIR, name);
    return fopen(path, "w");
}

static void emit_event(FILE *f, const char *phase, const cinm_event *e) {
    fprintf(f, "{\"phase\":\"%s\",\"seq\":%u,\"key\":%u,\"reward\":%.3f,\"dphi\":[",
            phase, e->seq, e->key, (double)e->reward);
    for (int k = 0; k < NFEAT; k++) fprintf(f, "%s%.3f", k ? "," : "", (double)e->dphi[k]);
    fprintf(f, "]}\n");
}

static void write_config(FILE *f, const cinm_drum_ingest *in) {
    fprintf(f,
        "task = \"cim-drum-m1-gate\"\n"
        "feature_schema_id = \"cim.drum_gesture_phi.v1\"\n"
        "fixture = \"%s\"\n"
        "baseline = \"held-out phase split (proposer != grader)\"\n"
        "seed = \"golden_set.sample @ splitmix64 0x60D5E7B17A57E11\"\n"
        "train_window = %zu\n"
        "slate_window = %zu\n"
        "threshold_goldens = \"all ideal>bad\"\n"
        "threshold_revision_corrections = %d\n",
        FIXTURE, in->train_len, in->slate_len, REVISION_CORRECTIONS);
}

static void write_events(FILE *f, const cinm_drum_ingest *in) {
    for (size_t i = 0; i < in->train_len; i++) emit_event(f, "train", &in->train[i]);
    for (size_t i = 0; i < in->slate_len; i++) emit_event(f, "slate", &in->slate[i]);
}

static void write_metrics(FILE *f, const struct verdict *v) {
    fprintf(f,
        "{\n"
        "  \"golden_winrate\": %.3f,\n"
        "  \"goldens_correct\": %d,\n"
        "  \"goldens_total\": %d,\n"
        "  \"revision_flips\": %d,\n"
        "  \"revision_total\": %d,\n"
        "  \"catch_tie\": %s,\n"
        "  \"catch_degraded\": %s,\n"
        "  \"cells\": %zu,\n"
        "  \"max_cells\": %d,\n"
        "  \"conflict_rate\": %.3f,\n"
        "  \"max_clamp\": %u,\n"
        "  \"anti_patterns\": [\n",
        v->gold_total ? (double)v->gold_correct / (double)v->gold_total : 0.0,
        v->gold_correct, v->gold_total, v->rev_flips, v->rev_total,
        v->catch_tie ? "true" : "false", v->catch_degr ? "true" : "false",
        v->cells, MAX_CELLS, (double)v->conflict_rate, v->max_clamp);
    for (int j = 0; j < N_AP; j++)
        fprintf(f, "    {\"name\":\"%s\",\"status\":\"%s\",\"goldens\":%d,\"correct\":%d}%s\n",
                AP[j].name, AP[j].mode == AP_CHECKED ? "checked" : "deferred-m2",
                v->ap_total[j], v->ap_correct[j], j + 1 < N_AP ? "," : "");
    fprintf(f, "  ]\n}\n");
}

static void write_decisions(FILE *f, const struct verdict *v) {
    fprintf(f, "{\"gate\":\"bounds\",\"pass\":%s}\n",   v->bounds ? "true" : "false");
    fprintf(f, "{\"gate\":\"capacity\",\"pass\":%s}\n", v->capacity ? "true" : "false");
    fprintf(f, "{\"gate\":\"rollback\",\"pass\":%s}\n", v->rollback ? "true" : "false");
    fprintf(f, "{\"gate\":\"anti_hack\",\"pass\":%s}\n", v->anti_hack ? "true" : "false");
    fprintf(f, "{\"gate\":\"golden_set\",\"pass\":%s,\"correct\":%d,\"total\":%d}\n",
            v->gold_correct == v->gold_total ? "true" : "false", v->gold_correct, v->gold_total);
    fprintf(f, "{\"gate\":\"catch\",\"pass\":%s}\n",
            (v->catch_tie && v->catch_degr) ? "true" : "false");
    fprintf(f, "{\"gate\":\"revision\",\"pass\":%s,\"flips\":%d,\"total\":%d}\n",
            v->rev_flips == v->rev_total ? "true" : "false", v->rev_flips, v->rev_total);
    fprintf(f, "{\"decision\":\"%s\",\"reason_code\":\"%s\"}\n",
            v->accept ? "accept" : "reject", v->reason);
}

static void write_summary(FILE *f, const struct verdict *v) {
    fprintf(f, "# cim-drum-m1-gate run summary\n\n");
    fprintf(f, "Decision: **%s**%s%s\n\n", v->accept ? "ACCEPT" : "REJECT",
            v->accept ? "" : " — reason_code=", v->accept ? "" : v->reason);
    fprintf(f, "Measured on held-out phase-1 (slate) only; trained on phase-0 bootstrap.\n");
    fprintf(f, "Baseline: held-out phase split (proposer != grader, doc 18).\n\n");
    fprintf(f, "- goldens ideal>bad: %d/%d\n", v->gold_correct, v->gold_total);
    fprintf(f, "- revision flips (wrong before correction, right after): %d/%d\n",
            v->rev_flips, v->rev_total);
    fprintf(f, "- catch trials: identical_aa=%s degraded=%s\n",
            v->catch_tie ? "tie-honored" : "FAIL", v->catch_degr ? "ok" : "FAIL");
    fprintf(f, "- footprint: cells=%zu / %d, conflict_rate=%.3f, max_clamp=%u\n\n",
            v->cells, MAX_CELLS, (double)v->conflict_rate, v->max_clamp);
    fprintf(f, "## ANTI_PATTERNS coverage (transcribed from drummer authority)\n\n");
    for (int j = 0; j < N_AP; j++) {
        if (AP[j].mode == AP_CHECKED)
            fprintf(f, "- CHECKED  %-22s %d/%d goldens avoided — %s\n",
                    AP[j].name, v->ap_correct[j], v->ap_total[j], AP[j].note);
        else
            fprintf(f, "- DEFERRED %-22s (M2/N) — %s\n", AP[j].name, AP[j].note);
    }
    fprintf(f, "\n## Inherited M1 gates (not re-run here)\n\n");
    fprintf(f, "- forgetting probe: see `make run-consolidate`\n");
    fprintf(f, "- reversibility/replay: see `make run-txn`, `make run-replay`\n");
}

static bool write_bundle(const cinm_drum_ingest *in, const struct verdict *v) {
    FILE *f;
    if ((f = open_run("config.toml")))    { write_config(f, in);    fclose(f); } else return false;
    if ((f = open_run("events.jsonl")))   { write_events(f, in);    fclose(f); } else return false;
    if ((f = open_run("metrics.json")))   { write_metrics(f, v);    fclose(f); } else return false;
    if ((f = open_run("decisions.jsonl"))){ write_decisions(f, v);  fclose(f); } else return false;
    if ((f = open_run("summary.md")))     { write_summary(f, v);    fclose(f); } else return false;
    return true;
}

int main(void) {
    cinm_drum_ingest in;
    int ln = 0;
    if (!cinm_drum_ingest_file(FIXTURE, &in, &ln)) {
        printf("ingest (line %d): FAIL\n", ln);
        return 1;
    }
    if (in.slate_len != N_SLATE) {
        printf("slate_len %zu != %d (CATALOG drift): FAIL\n", in.slate_len, N_SLATE);
        return 1;
    }

    struct verdict v = { .reason = "" };
    cinm_map M;
    train(&M, &in, &v);
    v.cells = M.count;

    cinm_map grader;
    cinm_snapshot(&M, &grader);             /* the evaluator must not change during scoring */

    for (int i = 0; i < N_SLATE; i++) {
        const cinm_event *e = &in.slate[i];
        switch (CATALOG[i].kind) {
        case GOLD:
            v.pass[i] = margin_at(&M, e->key, e->dphi) > 0.0f;
            v.gold_correct += v.pass[i]; v.gold_total++;
            break;
        case CATCH_TIE:
            v.pass[i] = dphi_zero(e) && e->reward == 0.0f;   /* tie mapped to a no-op */
            v.catch_tie = v.pass[i];
            break;
        case CATCH_DEGRADED:
            v.pass[i] = margin_at(&M, e->key, e->dphi) > 0.0f;
            v.catch_degr = v.pass[i];
            break;
        case REVISION: {
            float pre = margin_at(&M, e->key, e->dphi);      /* wrong before correction */
            cinm_map w = M;
            for (int n = 0; n < REVISION_CORRECTIONS; n++)
                cinm_update_adaptive(&w, cinm_address(&w, e->key, NULL), e->dphi, 0.9f, M.t + (uint32_t)n);
            float post = margin_at(&w, e->key, e->dphi);     /* right after */
            v.pass[i] = pre <= 0.0f && post > 0.0f;
            v.rev_flips += v.pass[i]; v.rev_total++;
            break;
        }
        }
        int j = ap_index(CATALOG[i].ap);
        if (j >= 0 && (CATALOG[i].kind == GOLD || CATALOG[i].kind == REVISION)) {
            v.ap_total[j]++; v.ap_correct[j] += v.pass[i];
        }
    }

    v.anti_hack = cinm_equal(&M, &grader);  /* read-only scoring left the grader untouched */
    v.bounds    = bounds_ok(&M);
    v.capacity  = M.count == N_GOLD && M.count < (size_t)MAX_CELLS;
    v.rollback  = rollback_ok(&M);

    if      (!v.bounds)    v.reason = "bounds";
    else if (!v.capacity)  v.reason = "capacity";
    else if (!v.rollback)  v.reason = "rollback";
    else if (!v.anti_hack) v.reason = "anti_hack";
    else if (v.gold_correct != v.gold_total) v.reason = "golden_set";
    else if (!v.catch_tie || !v.catch_degr)  v.reason = "catch";
    else if (v.rev_flips != v.rev_total)     v.reason = "revision";
    v.accept = v.reason[0] == '\0';

    int checked = 0, deferred = 0;
    for (int j = 0; j < N_AP; j++) {
        if (AP[j].mode == AP_CHECKED) checked++; else deferred++;
    }

    printf("hard gates (bounds/capacity/rollback/anti-hack): %s\n",
           (v.bounds && v.capacity && v.rollback && v.anti_hack) ? "PASS" : "FAIL");
    printf("GOLDEN_SET held-out (%d/%d ideal>bad, %d/%d revision flips, catch %s/%s): %s\n",
           v.gold_correct, v.gold_total, v.rev_flips, v.rev_total,
           v.catch_tie ? "tie" : "X", v.catch_degr ? "ok" : "X",
           (v.gold_correct == v.gold_total && v.rev_flips == v.rev_total
            && v.catch_tie && v.catch_degr) ? "PASS" : "FAIL");
    printf("ANTI_PATTERNS critic (%d checked, %d deferred-M2): %s\n",
           checked, deferred, v.accept ? "PASS" : "FAIL");

    bool wrote = write_bundle(&in, &v);
    printf("M1 decision: %s%s%s\n", v.accept ? "ACCEPT" : "REJECT",
           v.accept ? "" : " reason_code=", v.accept ? "" : v.reason);
    printf("run bundle (%s): %s\n", RUNDIR,
           wrote ? "config.toml events.jsonl metrics.json decisions.jsonl summary.md" : "WRITE FAILED (run via make run-drum-gate)");

    return (v.accept && wrote) ? 0 : 1;
}
