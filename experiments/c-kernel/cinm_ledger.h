/* cinm_ledger.h — in-RAM decision ledger: append-only receipts that record WHY the
 * map changed (operator commit/rollback, lossy consolidation). Separate from the
 * event log so the hot event stays lean; cold, read for audit/undo only, never on
 * the hot path. A receipt explains a change without being enough to reconstruct
 * pre-change state (D018). No disk, no persistence across process restart. */
#ifndef CINM_LEDGER_H
#define CINM_LEDGER_H

#include <stdint.h>
#include <stddef.h>

typedef enum {
    CINM_DECISION_COMMIT      = 0,  /* a speculative change was kept */
    CINM_DECISION_ROLLBACK    = 1,  /* a speculative change was reverted */
    CINM_DECISION_CONSOLIDATE = 2,  /* a lossy consolidation advanced the epoch */
} cinm_decision_kind;

/* One audit receipt. info_a..info_d are kind-specific:
 *   CONSOLIDATE       info_a = evicted, info_b = frozen (promoted), info_c = merged, info_d = split
 *   COMMIT / ROLLBACK info_a = candidate id, info_b/info_c/info_d = 0
 * All four structural counts are recorded so the receipt honestly accounts for every
 * lossy operation a consolidation performed (merge and split were previously dropped).
 * base_seq is the epoch base after the decision (consolidation moves it). */
typedef struct {
    uint32_t t;          /* logical clock at the decision */
    uint32_t epoch;      /* epoch in effect after the decision */
    uint32_t kind;       /* cinm_decision_kind */
    uint32_t info_a;
    uint32_t info_b;
    uint32_t info_c;
    uint32_t info_d;
    uint32_t base_seq;
} cinm_decision;

/* Append-only ledger, growable, RAM-resident. */
typedef struct {
    cinm_decision *items;
    size_t         len;
    size_t         cap;
} cinm_ledger;

void cinm_ledger_init(cinm_ledger *l);
void cinm_ledger_free(cinm_ledger *l);

/* Append one receipt (copied in). Returns false only on allocation failure. */
bool cinm_ledger_append(cinm_ledger *l, const cinm_decision *d);

#endif /* CINM_LEDGER_H */
