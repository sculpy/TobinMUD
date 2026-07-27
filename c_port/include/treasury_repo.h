/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_TREASURY_REPO_H
#define TOBIN_TREASURY_REPO_H

#include <stdbool.h>

/* DB access for the singleton `world_treasury` row (Money system v2,
 * Sneezy → Tobin feature audit). Accumulated sales-tax revenue,
 * immortal-visible via the `treasury` command -- a simpler stand-in for
 * the real upstream's per-shop tax-office accounts and full double-entry
 * ledger, which only pay off once Tobin has player-owned shops to fund. */

/* Current treasury balance. Returns 0 if the singleton row is somehow
 * missing (should never happen post-migration). */
int treasury_repo_get_gold(void);

/* Adds `delta` gold to the treasury (negative to spend, once something
 * exists to spend it on). Returns false on a DB error. */
bool treasury_repo_add_gold(int delta);

#endif
