/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_HOSTNAME_RESOLVE_H
#define TOBIN_HOSTNAME_RESOLVE_H

/* Asynchronous reverse-DNS lookup for a connecting IP (user 2026-07-11:
 * "in messages and logs where IP address is displayed, make it a hostname
 * dns lookup instead"). getnameinfo() is a blocking, potentially slow
 * call -- doing it inline on accept() would stall every other connection
 * on the single-threaded select() loop, so each lookup runs in its own
 * detached thread and reports back through a small result queue that the
 * main loop drains once per tick. */

/* Kick off a background reverse-DNS lookup for `ip`, tagged with `fd` so
 * the result can be matched back to the right descriptor later (fd reuse
 * is guarded against by also comparing `ip` at apply time -- see
 * hostname_resolve_poll()). Best-effort: silently does nothing if the
 * thread can't be created. */
void hostname_resolve_start(int fd, const char *ip);

/* Applies any completed lookups to their matching (still-connected, same
 * fd+ip) descriptor's `hostname` field. Call once per game-loop tick. */
void hostname_resolve_poll(void);

#endif
