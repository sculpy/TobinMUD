/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_MEDITATE_H
#define TOBIN_MEDITATE_H

/* `yoginsa` (Monk) background meditation task -- see being.h's
 * `meditating` field doc comment for the full rationale (user
 * 2026-07-28: "yoginsa should be automatic, a task", reverting this
 * spell/skill audit item's original single-action scope-down back
 * toward real upstream's own recurring task_yoginsa() shape now that
 * the pattern exists via planting.c). No general task engine exists in
 * Tobin, so this is scoped to just this one task, same "one-off, not a
 * framework" precedent planting.c itself set. */

/* Advances every connected, currently-meditating character by one
 * heal roll, stopping (with a message) anyone who's no longer resting/
 * sitting or who started fighting. Pulse-registered in main.c at
 * REGEN_PULSES cadence, same rhythm as natural HP/Vit regen. */
void meditate_tick_run(long pulse_num);

#endif
