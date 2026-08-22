/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_GAME_LOOP_H
#define TOBIN_GAME_LOOP_H

#include <stdbool.h>
#include <time.h>

/* Opens a listening socket on `port` and runs the select()-driven
 * accept/read loop until interrupted (SIGINT). Returns 0 on clean
 * shutdown, nonzero on a fatal setup error (e.g. couldn't bind).
 *
 * If `copyover_file` is non-NULL (the --copyover startup flag), the loop
 * instead adopts the listening socket and every player connection listed
 * in that file -- written by cmd_copyover.c just before it exec()'d this
 * binary -- so a copyover reboot keeps everyone connected. Falls back to
 * a fresh socket if the file is missing/unreadable. */
int game_loop_run(int port, const char *copyover_file);

/* Opens the listening socket (or, for a copyover, adopts the inherited one
 * straight from the recovery file's "listen" line) as early as possible in
 * main()'s startup -- BEFORE the DB probe / world-load work that otherwise
 * leaves anyone connecting during that window sitting in the kernel's
 * accept backlog in total silence until game_loop_run() finally starts
 * (user 2026-07-26: "when connecting during a reboot, we should accept
 * the connection and give some booting information"). Must be called
 * before any game_loop_boot_poll()/game_loop_run() call. Returns false on
 * a fatal socket error (bind/listen failure). */
bool game_loop_boot_open(int port, const char *copyover_file);

/* Call between each slow setup step in main()'s boot sequence. Sends
 * `message` once to (a) every existing player connection listed in the
 * copyover recovery file, if this is a copyover reboot, the first time
 * this is called, and (b) any brand-new connection accepted on the
 * listening socket since the last call. New connections are held (their
 * fd remembered, nothing else) until game_loop_run() starts and hands
 * them a real descriptor_t, exactly as if they'd been accepted in its
 * first loop iteration. Returns how many distinct sockets were pinged
 * this call, purely so main() can log a meaningful boot-status summary. */
int game_loop_boot_poll(const char *message);

/* The live listening socket's fd (-1 before game_loop_run sets it up) --
 * cmd_copyover.c writes it into the recovery file so the next exec can
 * keep accepting connections without rebinding the port. */
int game_loop_listen_fd(void);

/* Requests a clean stop of the main loop -- same path SIGINT already takes
 * (handle_sigint() sets the same internal flag), just triggered from
 * in-game instead of a signal. The loop finishes its current iteration,
 * then closes every descriptor and the listening socket before
 * game_loop_run() returns. Used by shutdown.c once it has finished
 * broadcasting/saving; NOT a substitute for that -- calling this alone
 * skips the "kindly" warning and save step entirely. */
void game_loop_request_shutdown(void);

/* Absolute path of this server binary, resolved from argv[0] at startup
 * (main.c). cmd_copyover.c execs THIS PATH, deliberately not
 * /proc/self/exe: after a rebuild replaces the file, /proc/self/exe still
 * points at the deleted old inode and a copyover would relaunch the OLD
 * code -- the path resolves to whatever is freshly built there instead. */
const char *tobin_binary_path(void);

/* Wall-clock time this server generation started (main.c, set once near
 * the top of main() -- unconditionally, so both a cold boot AND a
 * copyover successor each get their own fresh timestamp, since neither
 * preserves any other in-memory state either; see zone.h's "copyover
 * doesn't survive world state" doc). Backs the `uptime` command
 * (`uptime` command, TODO.md priority item, 2026-08-02). */
time_t tobin_boot_time(void);

#endif
