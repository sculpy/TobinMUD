/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "log.h"

#include <dirent.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#define LOG_RETENTION_DAYS 21

static FILE *g_log_file = NULL;
static char g_log_path[LOG_PATH_MAX] = "";

/* Deletes any *.log in LOG_DIR not modified within LOG_RETENTION_DAYS days --
 * so we keep three weeks of daily logs and no more. Called at each open. */
static void log_prune_old(void) {
    DIR *dir = opendir(LOG_DIR);
    if (!dir)
        return;
    time_t now = time(NULL);
    struct dirent *de;
    while ((de = readdir(dir)) != NULL) {
        size_t len = strlen(de->d_name);
        if (len < 5 || strcmp(de->d_name + len - 4, ".log") != 0)
            continue;
        char p[LOG_PATH_MAX + 260]; /* room for LOG_DIR + '/' + a 255-char name */
        snprintf(p, sizeof(p), "%s/%s", LOG_DIR, de->d_name);
        struct stat st;
        if (stat(p, &st) == 0
            && now - st.st_mtime > (time_t)LOG_RETENTION_DAYS * 86400)
            unlink(p);
    }
    closedir(dir);
}

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

const char *log_type_name(log_type_t type) {
    switch (type) {
        case LOG_SILENT: return "SILENT";
        case LOG_PIO:    return "PIO";
        case LOG_COMBAT: return "COMBAT";
        case LOG_BUG:    return "BUG";
        case LOG_DB:     return "DB";
        case LOG_EDIT:   return "EDIT";
        case LOG_JESUS:  return "JESUS";
        case LOG_GAME:
        default:         return "GAME";
    }
}

const char *log_type_personal_name(log_type_t type) {
    return type == LOG_JESUS ? "Jesus" : NULL;
}

bool log_open(void) {
    mkdir(LOG_DIR, 0755); /* EEXIST is fine */
    log_prune_old();

    time_t now = time(NULL);
    struct tm tm_buf;
    localtime_r(&now, &tm_buf);
    /* One <YYYY-MM-DD>.log per calendar day (user spec) -- every reboot,
     * copyover, or rotate on the same day appends to the same file, so the
     * day's log is never fragmented and there's no time in the name. */
    char stamp[16];
    strftime(stamp, sizeof(stamp), "%Y-%m-%d", &tm_buf);

    char path[LOG_PATH_MAX];
    snprintf(path, sizeof(path), "%s/%s.log", LOG_DIR, stamp);

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
    /* Daily log files (one <date>.log per calendar day) are managed
     * automatically, so a manual rotate just re-opens the current day's file
     * and prunes anything past the retention window -- it no longer starts a
     * separate file (that would fragment the day's log, contrary to spec). */
    if (!log_open())
        return false;
    log_info("Log re-opened; daily file is %s.", g_log_path);
    return true;
}

const char *log_current_path(void) {
    return g_log_path;
}
