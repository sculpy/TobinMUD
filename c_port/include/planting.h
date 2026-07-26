/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_PLANTING_H
#define TOBIN_PLANTING_H

/* Seed farming's dig-hole/sow-seeds/cover-hole task (Planting, Sneezy ->
 * Tobin feature audit) -- see being.h's planting_seed/planting_ticks_left/
 * planting_type/planting_room doc comment for the fields this drives, and
 * cmd_plant.c for where a task gets started. No general task engine exists
 * in Tobin yet, so this is deliberately scoped to just this one task
 * (same "one-off, not a framework" precedent as the pet-assist pass added
 * directly to combat.c rather than a generic pet-AI system). */

/* Advances every connected character's in-progress planting task by one
 * step (dig hole -> sow seeds, consuming the seed sack -> cover hole ->
 * plant appears), aborting with a message if the character has moved
 * rooms, lost the seed sack, or is now fighting -- same safety checks
 * task_plant() itself makes. Pulse-registered in main.c; also forced by
 * `aitick` for deterministic testing. */
void planting_tick_run(long pulse_num);

#endif
