/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_SPELLCAST_H
#define TOBIN_SPELLCAST_H

#include <stdbool.h>

#include "being.h"
#include "descriptor.h"
#include "skill.h"

/* Multi-round `cast` delay (user 2026-08-09: "spell casting should take
 * 2-3 rounds before hitting with purple colored messaging about 2-3
 * lines per casting tick. druids should have modified messages that
 * mages have except those messages should have a forest flavor to them.
 * druid casting should take about the same amount of time" -- follow-up:
 * "druid messaging should be <y>"). Cleric's `pray` (cmd_pray.c) is
 * explicitly out of scope and stays instant.
 *
 * cmd_cast.c's cmd_cast() already does ALL the real gating (class/
 * level/discipline/mana/component/proficiency roll) exactly as before --
 * this only replaces the single instant `task_cast()` call at the very
 * end of that gate chain. Once the caster has committed (mana spent,
 * component about to be consumed, proficiency already rolled a success),
 * spellcast_start() stashes the spell name + resolved target on `ch`
 * (being.h's is_casting/cast_rounds_left/cast_spell_name/cast_target)
 * instead of resolving the effect immediately, shows round 1's flavor
 * text right away, and locks the caster out via being_set_wait() for the
 * whole delay (same lag/lockout convention every other action uses).
 * spellcast_tick_run(), registered every COMBAT_ROUND_PULSES (main.c,
 * alongside combat_process_run/meditate_tick_run), then shows each
 * subsequent round's flavor text and, once the countdown hits 0, calls
 * cmd_cast_resolve_effect() (cmd_cast.c) -- the exact same per-spell
 * dispatch chain `cast` always used (100% unchanged effect behavior),
 * just moved out from under the instant path. */
void spellcast_start(descriptor_t *d, being_t *ch, const skill_def_t *sk, being_t *target);

/* Delayed FUMBLE (user 2026-08-16: "a proficiency FUMBLE still fizzles
 * instantly for its full cost -- make that failure play out over a round
 * or two too"). Called from cmd_cast()'s failed-proficiency-roll branch
 * instead of the old instant fizzle: enters the same multi-round task (a
 * shorter 1-2 rounds) with being.h's cast_fumble set, so the caster
 * visibly strains through the botched incantation and pays mana per round
 * (an interrupted fumble costs less), then spellcast_tick_run() shows a
 * fizzle at the end rather than resolving any effect. No target is used
 * -- the cast is doomed regardless. */
void spellcast_start_fumble(descriptor_t *d, being_t *ch, const skill_def_t *sk);

void spellcast_tick_run(long pulse_num);

/* Distraction hook -- a disruptive maneuver (bash/kick/trip/grapple)
 * landed on a caster mid-`cast` adds `amt` to their distraction counter;
 * spellcast_tick_run() rolls it each round and may shatter the spell
 * (Sneezy spelltask parity, user 2026-08-10). No-op unless `ch` is
 * casting. Plain melee never calls this, matching upstream (concentration
 * is WIS-gated, not broken by every hit). See spellcast.c for the roll. */
void spellcast_distract(being_t *ch, int amt);

/* The real per-spell effect dispatch (cmd_cast.c) -- was `static void
 * task_cast(...)`, renamed and exposed here so spellcast_tick_run() can
 * invoke it once a delayed cast's countdown completes. Unchanged body/
 * behavior, just called from a different place and a beat later; see
 * cmd_cast.c's own doc comment on it for the full per-spell breakdown. */
void cmd_cast_resolve_effect(descriptor_t *d, being_t *ch, being_t *target, const skill_def_t *sk);

#endif
