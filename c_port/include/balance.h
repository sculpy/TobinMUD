/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_BALANCE_H
#define TOBIN_BALANCE_H

#include "balance_repo.h"

/* In-memory cache of class_balance/race_balance (balance_repo.h) --
 * user 2026-07-12: "a balance command (60) where you take args:
 * balance <class|race> that is menu driven to adjust balance
 * numbers/modifiers that will apply gamewide to the class or race you
 * just balanced." being_calc_max_hp()/combat_strike()/being_total_ac()
 * read this cache directly (a plain array lookup) rather than hitting
 * the DB on every heartbeat/swing; `balance_cache_load()` populates it
 * once at startup (see main.c) and class_balance_set()/race_balance_set()
 * (called by cmd_balance.c on Save) refresh it immediately so a change
 * applies gamewide without a restart. */

void balance_cache_load(void);

const balance_mod_t *class_balance_get(player_class_t cls);
const balance_mod_t *race_balance_get(player_race_t race);

/* Persists `mods` to the DB AND updates the live cache -- the one path
 * cmd_balance.c/descriptor.c should use to change a value (never call
 * class_balance_save()/race_balance_save() directly, or the cache goes
 * stale until the next restart). Returns false if the DB write failed
 * (cache is left unchanged in that case). */
bool class_balance_set(player_class_t cls, const balance_mod_t *mods);
bool race_balance_set(player_race_t race, const balance_mod_t *mods);

#endif
