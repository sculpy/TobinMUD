/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_CRASH_HANDLER_H
#define TOBIN_CRASH_HANDLER_H

/* Installs handlers for the crash-causing signals (SIGSEGV, SIGABRT,
 * SIGFPE, SIGBUS, SIGILL) that write a short, human-readable diagnostic
 * (timestamp, signal, active connection count) to its own file under
 * logs/crashes/ before letting the crash proceed normally -- the OS's
 * own crash-dump mechanism (systemd-coredump on Fedora, already active
 * and unlimited via `ulimit -c`/`core_pattern`) still produces the full
 * core dump for gdb-level post-mortem; this just adds the app-level
 * context a raw core dump doesn't carry on its own (how many players
 * were connected, how long the process had been up). Call once, early
 * in main(), before opening the listening socket. */
void crash_handler_install(void);

#endif
