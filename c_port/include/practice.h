/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_PRACTICE_H
#define TOBIN_PRACTICE_H

#include "being.h"

/* Practice-system support (redesign 2026-07-13). The per-level award of
 * practice points is
 *     random(6,8) + round(wisdom_bonus * wisdom_practice_modifier)
 * where wisdom_bonus = floor((wisdom - ATTR_BASE) / 10). The modifier is a
 * gamewide scalar persisted in game_config (key 'wisdom_practice_modifier',
 * default 1.0), viewed/changed live with `balance wisdom [<value>]` -- same
 * load/cache/set shape as multiplay.c. See cmd_practice.c for how the
 * points are spent. */

double wisdom_practice_modifier(void);           /* cached gamewide scalar */
void wisdom_practice_modifier_set(double value);  /* updates cache + DB */
void wisdom_practice_load(void);                  /* loads the cache at boot */

/* Practice points a character with this wisdom earns for ONE level gained.
 * Uses rand(), so call it once per level. Never negative. */
int practice_points_for_level(const being_t *ch);

/* The three guildmaster tiers are told apart by mob.level. */
#define GUILD_LEVEL_BASIC    51
#define GUILD_LEVEL_COMBAT   80
#define GUILD_LEVEL_ADVANCED 100

#endif
