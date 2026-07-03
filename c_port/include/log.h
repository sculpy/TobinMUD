#ifndef TOBIN_LOG_H
#define TOBIN_LOG_H

#include <stdbool.h>

void log_info(const char *fmt, ...);
void log_error(const char *fmt, ...);

/* Game log files (Session 21, user requirement): every log line also goes
 * to a file under LOG_DIR, named <YYYY-MM-DD_HH-MM-SS>.game.log from its
 * creation time. Console (stdout/stderr) output is unchanged -- the file
 * is in addition, so the nohup capture still works. */
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

#endif
