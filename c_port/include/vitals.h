/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_VITALS_H
#define TOBIN_VITALS_H

/* Hunger/thirst drain (Sneezy → Tobin feature audit, "Vital statistics").
 * Trimmed hard from the original's terrain-weighted, two-stage-random
 * foodNDrink() (misc/periodic.cc) -- every connected, non-immortal PC not
 * currently fighting drains 1 hunger + 1 thirst (of 100, see being.h's
 * progress_t field comment) every tick, flat rate, no terrain factor (that
 * belongs to the still-open "Terrain movement cost" audit item, not this
 * one). At 0, starvation/dehydration costs 1 HP that tick, floored at 1 HP
 * (same "never lethal outside real combat" precedent cmd_sip.c's poison
 * roll already established) -- real death from starvation is deferred to
 * whenever "Death processing" builds a real non-combat death path.
 * Persists each affected character immediately (player_progress_save()),
 * same "don't let a crash roll back real progress" reasoning as
 * combat.c's mid-fight HP persistence.
 * Register with pulse_register(VITALS_PULSES, vitals_tick_run) from
 * main.c. */
void vitals_tick_run(long pulse_num);

/* `aitick`'s forced-tick variant (cmd_aitick.c) -- does NOT touch any
 * connected player's hunger/thirst/HP (see vitals_tick_run() above for
 * that). User, 2026-08-03, after a real live incident: forcing 1000
 * vitals ticks via repeated `aitick 100` calls (testing a newly-ported
 * mob spec-proc) silently starved every OTHER connected player's hunger/
 * thirst to 0 and chip-damaged their HP down to nearly nothing, since
 * vitals_tick_run() runs for every connected non-immortal PC, not just
 * whatever the immortal running `aitick` is actually testing -- "dont
 * have aitick affect players hps" / "aitick should be artificial to
 * players." `aitick` still forces every OTHER world-state tick
 * (mob AI, object decay/growth, weather, gametime, triggers) exactly as
 * before -- only the player-vitals side effect is cut. */
void vitals_tick_force_world_only(long pulse_num);

/* VITALS_PULSES = 600 (~60s at 100ms/pulse) -- same "once a minute" cadence
 * as zone aging/gametime/mob AI/obj decay (main.c). At 1 point/tick, a
 * fully-fed-and-hydrated character takes ~100 minutes of continuous play
 * to reach 0 without eating or drinking -- present, not naggy. */
#define VITALS_PULSES 600

struct being;

/* Applies a real intoxication gain to `ch` -- `raw_value` is the
 * liquid's own per-unit `drunk` field (liquids.h), already multiplied
 * by however many units were drunk, same convention cmd_drink.c/
 * cmd_sip.c already use for thirst/hunger. Dampened by the real
 * SKILL_ALCOHOLISM formula (limits.cc's own gainCondition() DRUNK case:
 * `value *= (105 - skill) / 100` -- a trained drinker gets less drunk
 * per drink) and clamped to progress.drunk's 0-100 range. Trains the
 * skill on any POSITIVE gain (real upstream's own `if (getLiqDrunk() >
 * 0) ch->bSuccess(SKILL_ALCOHOLISM);`, obj_food.cc) -- a sobering drink
 * (raw_value <= 0, e.g. coffee) doesn't train it, same as the real
 * rule. No-op for immortals (callers already gate on
 * being_is_immortal() before calling this, but it stays self-contained
 * in case a future caller doesn't). */
void being_gain_drunk(struct being *ch, int raw_value);

#endif
