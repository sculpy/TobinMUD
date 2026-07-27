/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_PULSE_H
#define TOBIN_PULSE_H

/* C replacement for sys/{process,scheduler}.{h,cc}'s TBaseProcess/
 * TScheduler -- trimmed to what's needed so far: a flat array of
 * registered global processes (TProcess equivalent only, no
 * TCharProcess/TObjProcess registry yet, since the only per-character
 * recurring behavior so far -- combat rounds and the wait-pulse decrement
 * -- both just iterate g_descriptors directly rather than going through a
 * generic per-character scheduler). Revisit if a second/third such
 * behavior (regen, crafting tasks) shows up and the duplication is no
 * longer worth it.
 *
 * A pulse is 100ms (OPT_USEC in game_loop.c), directly matching the
 * original's literal pulse unit. Processes fire via modulus, exactly like
 * TBaseProcess::should_run(): !(pulse_num % trigger_pulse). */

typedef void (*pulse_fn_t)(long pulse_num);

/* Registers a process to run every trigger_pulse pulses. Call during
 * startup (main.c), before game_loop_run() -- fixed-size internal table,
 * no need for dynamic registration/unregistration yet. */
void pulse_register(int trigger_pulse, pulse_fn_t fn);

/* Runs every registered process whose trigger fires for pulse_num. Called
 * once per game_loop_run() iteration, whether or not select() returned
 * ready sockets. */
void pulse_scheduler_run(long pulse_num);

/* COMBAT_ROUND_PULSES = 12 (~1.2s at 100ms/pulse) -- mirrors the
 * original's Pulse::COMBAT. */
#define COMBAT_ROUND_PULSES 12

#endif
