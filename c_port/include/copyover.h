/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_COPYOVER_H
#define TOBIN_COPYOVER_H

#include <stdbool.h>

/* Timed copyover (user, 2026-08-08: "i wanted a timer argument so we can
 * copyover in 300 seconds or whatever time frame we want converted into
 * messages sent to all announcing every minute the countdown has changed
 * until 5 seconds then a count every second ... also the -now argument in
 * case we need to do it immediately. this is for shutdown and copyover").
 * Same shape as shutdown.h's own timed countdown (shutdown.c) -- ticks via
 * the pulse scheduler (copyover_pulse_tick(), registered in main.c at
 * 1-second granularity) so the game keeps running normally while a long
 * countdown plays out, instead of the old flat 5-second sleep() that froze
 * the whole select loop for its entire duration (fine for 5 seconds, not
 * for 300). The actual copyover mechanics (recovery-file write, room-state
 * dump, exec) are unchanged -- copyover_execute() is the same work
 * cmd_copyover.c always did, just callable from either an immediate
 * `-now` or the countdown's own zero point. */

/* Schedules a copyover `seconds` from now (0 = immediately), broadcasting
 * the request to every connection as coming from `initiator`. Overwrites
 * any countdown already pending. A 0-second call runs the copyover before
 * returning (and does not return at all if the exec succeeds). */
void copyover_schedule(int seconds, const char *initiator);

/* Cancels a pending timed copyover, announcing it to everyone as coming
 * from `initiator`. Returns false (no announcement made) if no copyover
 * was pending. */
bool copyover_cancel(const char *initiator);

/* True while a timed copyover is counting down. */
bool copyover_is_pending(void);

/* Registered with pulse_register(10, ...) in main.c -- decrements the
 * pending countdown once a second, broadcasts at a curated set of
 * milestones (same list shutdown.c uses), and performs the actual
 * copyover once it reaches zero. A no-op whenever no copyover is
 * pending. */
void copyover_pulse_tick(long pulse_num);

/* Does the actual copyover: freezes input processing (single-threaded, so
 * this function running IS the freeze), writes the recovery file, dumps
 * loose room contents, flushes output, and exec()s the (possibly freshly
 * rebuilt) binary in place. Does not return on success. On failure (no
 * listening socket, can't open the recovery file, or the exec itself
 * fails), logs the error and returns, leaving the world running unchanged
 * -- there's no live command context to report back to once this is
 * reached via a countdown rather than typed directly. */
void copyover_execute(void);

#endif
