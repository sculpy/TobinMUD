/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_SPELL_MANA_H
#define TOBIN_SPELL_MANA_H

/* Real per-spell mana cost, extracted from real SneezyMUD's own
 * discArray[] (sneezymud-master/code/code/misc/spell_info.cc's
 * MANA_<n> constructor argument -- MANA_54 costs 54 mana, etc.), case-
 * insensitive match on the spell's name. Falls back to a generic
 * level-scaled cost for any spell in Tobin's roster the original never
 * assigned a real MANA_<n> value to (many of the original's own spells
 * are MANA_0 too, e.g. Shaman-lineage ones folded into Druid -- those
 * genuinely cost nothing there either, not a gap in this lookup) or
 * that doesn't exist in the original at all (a handful of Tobin-
 * original mechanics). User 2026-08-06: "implement it just like
 * sneezy" -- this is the real data, not an invented flat formula. */
int spell_mana_cost(const char *name, int min_level);

#endif
