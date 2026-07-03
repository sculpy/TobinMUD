#include "log.h"

#include <stdarg.h>
#include <stdio.h>
#include <time.h>

static void log_line(FILE *out, const char *level, const char *fmt, va_list ap) {
    time_t now = time(NULL);
    struct tm tm_buf;
    localtime_r(&now, &tm_buf);

    char stamp[32];
    strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", &tm_buf);

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
