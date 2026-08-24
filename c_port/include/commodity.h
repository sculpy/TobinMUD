/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_COMMODITY_H
#define TOBIN_COMMODITY_H

#include <stdbool.h>

/* Commodities (Sneezy -> Tobin feature audit, "Commodities"; TODO.md
 * 2026-08-22): the real upstream ports a full live supply/demand
 * pricing engine (TCommodity::demandCurvePrice) around 200 materials.
 * Tobin already collapses the material system to 5 static tiers
 * (material.h) rather than porting that -- this module deliberately
 * does the same: the 182 raw-material/gemstone/organic prototypes
 * already seeded in the obj table (type 42/43/50) already carry a real
 * fixed price, so no new pricing model is needed, just a way to put
 * them into circulation via mob loot. See combat.c's combat_defeat(). */

/* One-time startup cache of every commodity prototype's vnum/price/
 * material, sorted by price descending. Call once at boot, alongside
 * the other cache_load()/init() calls in main.c. */
void commodity_cache_load(void);

/* Picks the highest-priced cached commodity whose price is <=
 * wealth_budget (mirrors upstream mob_loader.cc's mat_sort: prefer the
 * most valuable commodity a mob's skimmed wealth can afford). Returns
 * false (out params untouched) if wealth_budget is <= 0 or nothing
 * cached is that cheap. */
bool commodity_pick_for_wealth(int wealth_budget, int *out_vnum, int *out_price);

#endif
