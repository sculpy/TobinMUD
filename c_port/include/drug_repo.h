#ifndef TOBIN_DRUG_REPO_H
#define TOBIN_DRUG_REPO_H

#include <stdbool.h>

#include "drug.h"

/* Persistence for drug.h's per-being tracking -- only first_use/
 * last_use/total_consumed are saved; effect_ticks_left/applied[]/
 * withdrawal_applied[] deliberately aren't (see drug.h's own doc
 * comment) -- unlike active_affect_t (affect_repo.h, persisted as of
 * 2026-07-26), a drug dose's own in-progress effect is genuinely
 * transient bookkeeping the original itself doesn't round-trip either.
 * One row per (player_id, drug_type). */

/* Loads every drug row this player has into `states` (indexed by
 * drug_type_t) -- rows with no history simply stay however the caller
 * already initialized them (a fresh being_t is calloc'd to all-zero,
 * which is exactly "never used"). */
void drug_repo_load_all(long player_id, drug_state_t states[DRUG_COUNT]);

/* Upserts one drug's first_use/last_use/total_consumed. */
bool drug_repo_save(long player_id, drug_type_t type, const drug_state_t *st);

#endif
