#include "log.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

static FILE *g_log_file = NULL;
static char g_log_path[LOG_PATH_MAX] = "";

static void log_line(FILE *out, const char *level, const char *fmt, va_list ap) {
    time_t now = time(NULL);
    struct tm tm_buf;
    localtime_r(&now, &tm_buf);

    char stamp[32];
    strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", &tm_buf);

    /* File first (if open), then console -- same line to both. */
    if (g_log_file) {
        va_list ap2;
        va_copy(ap2, ap);
        fprintf(g_log_file, "[%s] %s: ", stamp, level);
        vfprintf(g_log_file, fmt, ap2);
        fputc('\n', g_log_file);
        fflush(g_log_file);
        va_end(ap2);
    }

    fprintf(out, "[%s] %s: ", stamp, level);
    vfprintf(out, fmt, ap);
    fputc('\n', out);
    fflush(out); /* stdout is fully buffered when redirected to a file/pipe */
}

void log_info(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    log_line(stdout, "INFO", fmt, ap);
    va_end(ap);
}

void log_error(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    log_line(stderr, "ERROR", fmt, ap);
    va_end(ap);
}

bool log_open(void) {
    mkdir(LOG_DIR, 0755); /* EEXIST is fine */

    time_t now = time(NULL);
    struct tm tm_buf;
    localtime_r(&now, &tm_buf);
    /* DDMMYY.HHMM<AM/PM>.log, e.g. 030726.0921AM.log (user spec). A
     * second open within the same minute (rotate, quick restart) gets a
     * -2/-3/... suffix so rotation always starts a genuinely fresh file. */
    char stamp[32];
    strftime(stamp, sizeof(stamp), "%d%m%y.%I%M%p", &tm_buf);

    char path[LOG_PATH_MAX];
    snprintf(path, sizeof(path), "%s/%s.log", LOG_DIR, stamp);
    for (int seq = 2; access(path, F_OK) == 0 && seq < 1000; seq++)
        snprintf(path, sizeof(path), "%s/%s-%d.log", LOG_DIR, stamp, seq);

    FILE *f = fopen(path, "a");
    if (!f) {
        log_error("cannot open log file '%s' -- console logging only", path);
        return false;
    }

    if (g_log_file)
        fclose(g_log_file);
    g_log_file = f;
    snprintf(g_log_path, sizeof(g_log_path), "%s", path);
    log_info("Logging to %s", g_log_path);
    return true;
}

bool log_rotate(void) {
    /* log_open() guarantees a fresh filename even within the same minute
     * (the -N suffix), so a rotate is always a real rotation. */
    char old_path[LOG_PATH_MAX];
    snprintf(old_path, sizeof(old_path), "%s", g_log_path);
    if (!log_open())
        return false;
    log_info("Rotated from %s", old_path[0] ? old_path : "(console only)");
    return true;
}

const char *log_current_path(void) {
    return g_log_path;
}
