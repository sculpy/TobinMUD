/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_REGEN_H
#define TOBIN_REGEN_H

/* C replacement for the shape of TPerson::hitGain() (misc/limits.cc),
 * called every pulse there via addToHit(hitGain()) -- trimmed to a flat
 * placeholder rate (not the original's level/CON/hospital-room/drunk/camp
 * -weighted formula). Like the original, gain is entirely withheld while
 * fighting (`if (fight()) gain = 0;` there; here, `being->fighting != NULL`
 * skips the character for this tick) -- healing only happens at rest.
 * Register with pulse_register(REGEN_PULSES, regen_tick_run) from main.c. */
void regen_tick_run(long pulse_num);
void mana_piety_regen_tick_run(long pulse_num);

/* REGEN_PULSES = 50 (~5s at 100ms/pulse) -- how often a resting character
 * heals a small amount, on both their overall HP and every limb. */
#define REGEN_PULSES 50

/* MANA_REGEN_PULSES = 360 (~36s at 100ms/pulse) -- the slower cadence
 * mana and piety recover on, matching real SneezyMUD's Pulse::UPDATE
 * (updateHalfTickStuff). Register with pulse_register(MANA_REGEN_PULSES,
 * mana_piety_regen_tick_run) from main.c. */
#define MANA_REGEN_PULSES 360

#endif
