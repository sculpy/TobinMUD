#ifndef TOBIN_GAME_LOOP_H
#define TOBIN_GAME_LOOP_H

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

/* The live listening socket's fd (-1 before game_loop_run sets it up) --
 * cmd_copyover.c writes it into the recovery file so the next exec can
 * keep accepting connections without rebinding the port. */
int game_loop_listen_fd(void);

/* Absolute path of this server binary, resolved from argv[0] at startup
 * (main.c). cmd_copyover.c execs THIS PATH, deliberately not
 * /proc/self/exe: after a rebuild replaces the file, /proc/self/exe still
 * points at the deleted old inode and a copyover would relaunch the OLD
 * code -- the path resolves to whatever is freshly built there instead. */
const char *tobin_binary_path(void);

#endif
