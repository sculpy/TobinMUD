/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_RENT_H
#define TOBIN_RENT_H

#include "being.h"

/* Gamewide rent-cost settings, cached like practice.c's wisdom scalar and
 * persisted in game_config (keys 'rent_tax_at_max'/'rent_free_level'), so
 * an immortal's `balance rent` change survives a reboot. Rent cost is a
 * SneezyMUD port (misc/rent.cc charge_rent_tax): tax_at_max * level^3 /
 * MORTAL_LEVEL_MAX^3, i.e. the cube of level scaled so a max-level mortal
 * pays tax_at_max; levels at/below free_level (and immortals) pay nothing.
 * See cmd_rent.c (charge) and cmd_balance.c (`balance rent`, the editor). */
#define RENT_TAX_AT_MAX_DEFAULT 2000   /* gold a level-50 mortal pays */
#define RENT_FREE_LEVEL_DEFAULT 5      /* levels 1..this rent for free */
#define RENT_INNKEEPER_PCT_DEFAULT 10  /* %% of each rent tax the innkeeper keeps */

int rent_tax_at_max(void);
int rent_free_level(void);
void rent_tax_at_max_set(int value);
void rent_free_level_set(int value);
int rent_innkeeper_pct(void);      /* innkeeper's cut of each rent tax, 0-100 */
void rent_innkeeper_pct_set(int value);
void rent_config_load(void);

/* Full gold cost for `ch` to rent right now: 0 for an immortal or a
 * character at or below the free level; otherwise rent_tax_at_max() *
 * level^3 / MORTAL_LEVEL_MAX^3. NOT capped at the character's funds --
 * rent_apply_charge() draws wallet-then-bank and takes what is there if
 * the character is short. */
int rent_cost_for(const being_t *ch);

/* Charges `cost` gold to `ch`: the carried wallet (progress.gold) first,
 * then the bank (progress.bank_gold) to cover any shortfall, never below
 * zero. Returns the amount actually taken (wallet + bank; less than `cost`
 * only if the character is broke) and, when `bank_used` is non-NULL, sets
 * it to how much of that came from the bank. */
int rent_apply_charge(being_t *ch, int cost, int *bank_used);

#endif
