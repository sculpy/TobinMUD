/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
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

/* VITALS_PULSES = 600 (~60s at 100ms/pulse) -- same "once a minute" cadence
 * as zone aging/gametime/mob AI/obj decay (main.c). At 1 point/tick, a
 * fully-fed-and-hydrated character takes ~100 minutes of continuous play
 * to reach 0 without eating or drinking -- present, not naggy. */
#define VITALS_PULSES 600

#endif
