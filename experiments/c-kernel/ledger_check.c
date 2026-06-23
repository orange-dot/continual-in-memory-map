/* ledger_check.c — prove: the decision ledger is append-only and records WHY the
 * map changed (commit / rollback / consolidation), enough to explain a change but
 * not to reconstruct pre-change state (D018: cinm_decision holds no cell state). */
#include "cinm_ledger.h"
#include <stdio.h>

int main(void) {
    cinm_ledger led;
    cinm_ledger_init(&led);

    /* A self-adaptation transaction kept, then one reverted. */
    cinm_decision commit   = { .t = 10, .epoch = 0, .kind = CINM_DECISION_COMMIT,
                               .info_a = 7 };
    cinm_decision rollback = { .t = 11, .epoch = 0, .kind = CINM_DECISION_ROLLBACK,
                               .info_a = 8 };
    /* A lossy consolidation: evicted 5 dead cells, froze 2, advanced the epoch. */
    cinm_decision consolidate = { .t = 12, .epoch = 1, .kind = CINM_DECISION_CONSOLIDATE,
                                  .info_a = 5, .info_b = 2, .base_seq = 12 };

    bool appended = cinm_ledger_append(&led, &commit)
                 && cinm_ledger_append(&led, &rollback)
                 && cinm_ledger_append(&led, &consolidate);

    /* Append-only: every receipt landed, in order. */
    bool ordered = led.len == 3
                && led.items[0].kind == CINM_DECISION_COMMIT
                && led.items[1].kind == CINM_DECISION_ROLLBACK
                && led.items[2].kind == CINM_DECISION_CONSOLIDATE;

    /* Receipts explain: the consolidation says what it forgot and where the epoch went. */
    const cinm_decision *c = &led.items[2];
    bool explains = c->info_a == 5 && c->info_b == 2 && c->epoch == 1 && c->base_seq == 12;

    /* Each receipt references the logical clock it applies to. */
    bool referenced = led.items[0].t == 10 && led.items[1].t == 11 && led.items[2].t == 12;

    printf("ledger append-only and ordered: %s\n", (appended && ordered) ? "PASS" : "FAIL");
    printf("consolidation receipt explains the forgetting: %s\n", explains ? "PASS" : "FAIL");
    printf("receipts reference the logical clock: %s\n", referenced ? "PASS" : "FAIL");

    cinm_ledger_free(&led);
    return (appended && ordered && explains && referenced) ? 0 : 1;
}
