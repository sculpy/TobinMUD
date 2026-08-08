/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_TROPHY_H
#define TOBIN_TROPHY_H

#include "being.h"

/* Trophy system (TODO.md, user: "implement trophy system from Sneezy")
 * -- ported from SneezyMUD's TTrophy class (cmd/cmd_trophy.cc/.h). Tracks
 * a per-player, per-mob-vnum decaying kill count; farming the same mob
 * over and over shrinks the XP it's worth (down to a floor, never to
 * zero), nudging players toward variety instead of parking on one easy
 * spawn. The count decays back up over time (trophy_pulse_tick()) if the
 * player leaves that mob alone for a while.
 *
 * Disclosed scope-downs from the original: no `trophy wipe()` (character
 * deletion already cascades every player-keyed table the same way, see
 * being_destroy()/account deletion); no zone-grouped kill-percentage
 * browsing in the `trophy` command (cmd_trophy.c) -- Sneezy's own
 * doTrophy() walks every zone counting `doesLoad` mobs to compute "you've
 * killed N% of this zone", which needs a full mob-census-per-zone repo
 * function Tobin doesn't have yet; this port keeps just the functionally
 * important half, a flat per-mob XP-modifier listing. */

/* Returns the XP multiplier (0.3-1.0) for a kill against `mob_vnum` given
 * the player's current trophy `count` for that vnum. Exact port of
 * getExpModVal()'s formula: full reward for the first `free_kills`
 * (8) kills, then linearly steps down `step_mod`/`num_steps` (0.5/14)
 * per kill past that, floored at `min_mod` (0.3). `count` is normalized
 * by the mob's `max_exist` (world-wide instance cap, Tobin's analog of
 * Sneezy's `numberLoad`) when that's known and positive, so a mob with
 * many concurrent spawns decays more slowly per-kill than a unique one. */
double trophy_exp_mod(int mob_vnum, double count);

/* Short text label for a mod value (same 5-tier wording as Sneezy's
 * getExpModDescr(): full/much/a fair amount of/some/little), colorized
 * with Tobin's own lowercase-is-dim tag convention. */
const char *trophy_exp_mod_descr(double mod);

/* Records one kill of `mob_vnum` by `winner` (adds 1.0 to their trophy
 * count for that vnum) -- called once per real kill from combat_defeat().
 * No-op for a non-PC or immortal winner (immortals don't accrue XP at
 * all, see combat_award_hit_xp(), so a trophy count for them would never
 * be read back anyway). */
void trophy_record_kill(being_t *winner, int mob_vnum);

/* Pulse-registered (main.c, ~60s): decays every player's every trophy
 * count by 0.25, same flat rate and floor-at-zero guard as Sneezy's
 * procTrophyDecay -- see trophy_repo_decay_all()'s own doc comment for
 * why this is one global query instead of Sneezy's per-online-player
 * loop. */
void trophy_pulse_tick(long pulse_num);

#endif
