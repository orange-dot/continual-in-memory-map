/* cinm_ledger.c — in-RAM decision ledger: append-only receipts. No disk, no POSIX. */
#include "cinm_ledger.h"
#include <stdlib.h>

void cinm_ledger_init(cinm_ledger *l) {
    l->items = nullptr;
    l->len = 0;
    l->cap = 0;
}

void cinm_ledger_free(cinm_ledger *l) {
    free(l->items);
    l->items = nullptr;
    l->len = 0;
    l->cap = 0;
}

bool cinm_ledger_append(cinm_ledger *l, const cinm_decision *d) {
    if (l->len == l->cap) {
        if (l->cap > SIZE_MAX / 2) return false;                /* cap * 2 would wrap */
        size_t cap = l->cap ? l->cap * 2 : 64;
        if (cap > SIZE_MAX / sizeof *l->items) return false;    /* byte size would wrap */
        cinm_decision *grown = realloc(l->items, cap * sizeof *grown);
        if (!grown) return false;
        l->items = grown;
        l->cap = cap;
    }
    l->items[l->len++] = *d;
    return true;
}
