/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_SHUTDOWN_H
#define TOBIN_SHUTDOWN_H

#include <stdbool.h>

/* Graceful ("kindly") MUD shutdown -- immediate or timed. User, 2026-07-17:
 * "write a shutdown command to kill the mud kindly along with a time
 * function that will shutdown in <X> seconds". "Kindly" means every
 * connected player is warned and their character is saved before the
 * process exits, same spirit as cmd_copyover.c's save step but ending the
 * process instead of exec()ing a new one.
 *
 * A timed shutdown counts down via the pulse scheduler
 * (shutdown_pulse_tick(), registered in main.c at 1-second granularity)
 * rather than blocking the game loop the way copyover's 5-second sleep()
 * does -- copyover's blocking wait is fine for 5 seconds, but a shutdown
 * countdown can reasonably run for minutes, and players must be able to
 * keep playing normally while it counts down. */

/* Schedules a graceful shutdown `seconds` from now (0 = immediately),
 * broadcasting the request to every connection as coming from
 * `initiator`. Overwrites any countdown already pending. A 0-second call
 * shuts the MUD down before returning. */
void shutdown_schedule(int seconds, const char *initiator);

/* Cancels a pending timed shutdown, announcing it to everyone as coming
 * from `initiator`. Returns false (no announcement made) if no shutdown
 * was pending. */
bool shutdown_cancel(const char *initiator);

/* True while a timed shutdown is counting down (false once it has fired,
 * been cancelled, or never scheduled). */
bool shutdown_is_pending(void);

/* How many seconds remain in a pending countdown (undefined if
 * shutdown_is_pending() is false). */
int shutdown_seconds_remaining(void);

/* Registered with pulse_register(10, ...) in main.c (10 pulses = 1 second
 * at 100ms/pulse) -- decrements the pending countdown once a second,
 * broadcasts at a curated set of milestones so a long countdown doesn't
 * spam a line every single second, and performs the actual save+exit once
 * it reaches zero. A no-op whenever no shutdown is pending. */
void shutdown_pulse_tick(long pulse_num);

#endif
