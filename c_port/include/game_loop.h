#ifndef TOBIN_GAME_LOOP_H
#define TOBIN_GAME_LOOP_H

/* Opens a listening socket on `port` and runs the select()-driven
 * accept/read loop until interrupted (SIGINT). Returns 0 on clean
 * shutdown, nonzero on a fatal setup error (e.g. couldn't bind). */
int game_loop_run(int port);

#endif
