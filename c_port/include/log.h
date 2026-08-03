/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_LOG_H
#define TOBIN_LOG_H

#include <stdbool.h>

void log_info(const char *fmt, ...);
void log_error(const char *fmt, ...);

/* Typed game logs (user spec, inspired by Sneezy's logTypeT). Every game
 * event log has a type; the type names the log line both in the file and in
 * the colored [TYPE] tag echoed to online immortals. LOG_GAME is the generic
 * bucket (Sneezy's LOG_MISC). LOG_SILENT is recorded to the file but never
 * echoed (anti-spam). Personalized types (LOG_JESUS -- the one character-name
 * log kept, per user) echo ONLY to the immortal of that name; add more the
 * same way. */
typedef enum {
    LOG_SILENT = -2, /* file only, never echoed to immortals */
    LOG_GAME   = 0,  /* generic -- anything not otherwise typed */
    LOG_PIO,         /* player login/logout/link events */
    LOG_COMBAT,      /* combat and deaths */
    LOG_BUG,         /* bug reports */
    LOG_IDEA,        /* player feature requests */
    LOG_TYPO,        /* typo/text reports */
    LOG_DB,          /* database */
    LOG_EDIT,        /* in-game building/editing */
    LOG_JESUS,       /* personalized: only the immortal named "Jesus" sees it */
    LOG_TEST         /* smoke-test announce ("<x> test running"); harness only */
} log_type_t;

/* Default value of being_t.severity (see cmd_setsev.c / game_log()): every
 * real (non-silent) type ON, so a fresh login sees everything an immortal
 * saw before per-type opt-out existed. LOG_TEST is included so smoke-test
 * announcements show by default; it is deliberately left out of `setsev`'s
 * toggle list so it can't be silenced. */
#define LOG_SEVERITY_DEFAULT ((1 << (LOG_TEST + 1)) - 1)

/* Display tag for a log type ("GAME", "PIO", ...); used for the file line and
 * the [TAG] echoed to immortals. */
const char *log_type_name(log_type_t type);

/* If `type` is a personalized log, the immortal name it is scoped to (only
 * that immortal sees the echo); NULL for a general type. */
const char *log_type_personal_name(log_type_t type);

/* Logs a typed game event: writes it to the log file (tagged with the type)
 * and echoes it, prefixed with a cyan <c>[TYPE]<z> tag, to every online
 * immortal who isn't mid-editor -- except LOG_SILENT (file only) and
 * personalized types (only the named immortal). Implemented in descriptor.c
 * where the connection list lives. */
void game_log(log_type_t type, const char *fmt, ...);

/* Game log files (Session 21, user requirement): every log line also goes
 * to a file under LOG_DIR, named <DDMMYY>.<HHMM AM/PM>.log (e.g.
 * 030726.0921AM.log) from its creation time. Console (stdout/stderr)
 * output is unchanged -- the file is in addition, so the nohup capture
 * still works. */
#define LOG_DIR "logs"
#define LOG_PATH_MAX 128

/* Creates LOG_DIR if needed and opens a fresh timestamped log file.
 * Called once at startup (and by each copyover successor, which naturally
 * begins a new file). Returns false if the file couldn't be opened --
 * logging then continues console-only. */
bool log_open(void);

/* Closes the current file and opens a new timestamped one -- the in-game
 * `log rotate`. Returns false on failure (console logging continues). */
bool log_rotate(void);

/* Path of the current log file ("" if none open). */
const char *log_current_path(void);

/* Tracks the name of whatever smoke test is currently running, set by the
 * `@test <name>` / `@test done <name>` loopback hook (descriptor.c) --
 * backs the `test` command (58+, user: "add a test command that will list
 * whatever smoke test is currently running"). "" if nothing is running. */
void log_test_set_running(const char *name);
void log_test_clear_running(void);
const char *log_test_current_name(void);

#endif
