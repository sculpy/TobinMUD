/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_BALANCE_REPO_H
#define TOBIN_BALANCE_REPO_H

#include <stdbool.h>

#include "being.h"

/* DB access for the `class_balance`/`race_balance` tables
 * (db/sneezy/balance.sql, Tobin-specific -- see balance.h for the
 * in-memory cache these back and cmd_balance.c for the in-game
 * editor). Every row is seeded at creation (neutral 1.0/1.0/0/0), one
 * per player_class_t/player_race_t value, so a load should always
 * succeed in practice; the *_default() fallbacks only matter if the
 * seed migration hasn't run. */

typedef struct {
    float hp_mult;   /* multiplies class_hp_scale() in being_calc_max_hp() */
    float dmg_mult;  /* multiplies combat_strike()'s raw damage roll */
    int tohit_mod;   /* added to combat_strike()'s hit-roll modifier */
    int ac_mod;      /* added to being_total_ac() */
} balance_mod_t;

bool class_balance_load(player_class_t cls, balance_mod_t *out);
bool class_balance_save(player_class_t cls, const balance_mod_t *in);
bool race_balance_load(player_race_t race, balance_mod_t *out);
bool race_balance_save(player_race_t race, const balance_mod_t *in);

#endif
