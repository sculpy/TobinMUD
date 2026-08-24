/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_BALANCE_REPO_H
#define TOBIN_BALANCE_REPO_H

#include <stdbool.h>

#include "being.h"

/* DB access for the `class_balance`/`race_balance` tables
 * (db/tobin/balance.sql, Tobin-specific -- see balance.h for the
 * in-memory cache these back and cmd_balance.c for the in-game
 * editor). Every row is seeded at creation (neutral 1.0/1.0/0/0), one
 * per player_class_t/player_race_t value, so a load should always
 * succeed in practice; the *_default() fallbacks only matter if the
 * seed migration hasn't run. */

typedef struct {
    float hp_mult;   /* multiplies class_hp_per_level() in being_calc_max_hp() */
    float dmg_mult;  /* multiplies combat_strike()'s raw damage roll */
    int tohit_mod;   /* added to combat_strike()'s hit-roll modifier */
    int ac_mod;      /* added to being_total_ac() */
    /* PC-race perk system (docs/RACE_PERKS.md, 2026-08-10). Race-only in
     * practice (class rows leave these neutral); stored on both tables so
     * one struct/repo path serves both. Neutral = mults 1.0, ints 0. */
    float mana_mult;   /* multiplies being_calc_max_mana()  (Tier 0) */
    float move_mult;   /* multiplies being_calc_max_vit()   (Tier 0) */
    float food_mult;   /* hunger decay rate, vitals.c        (Tier 0) */
    float drink_mult;  /* thirst decay rate, vitals.c        (Tier 0) */
    int resist_poison;     /* % chance to shrug off, 0-100   (Tier 1) */
    int resist_charm;      /* % */
    int resist_sleep;      /* % */
    int resist_paralysis;  /* % */
    int resist_energy;     /* % */
    int resist_heat;       /* % */
    int resist_cold;       /* % */
    int infravision;   /* 0/1 innate dark-vision, room_is_dark_for (Tier 2) */
    int talent;        /* race_talent enum, 0 = none              (Tier 3) */
} balance_mod_t;

bool class_balance_load(player_class_t cls, balance_mod_t *out);
bool class_balance_save(player_class_t cls, const balance_mod_t *in);
bool race_balance_load(player_race_t race, balance_mod_t *out);
bool race_balance_save(player_race_t race, const balance_mod_t *in);

#endif
