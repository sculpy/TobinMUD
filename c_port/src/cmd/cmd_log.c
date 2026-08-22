/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "log.h"

/* `log [n] | search <text> | rotate | list` -- in-game access to the game
 * log files for level 59+ (user requirement, Session 21). Reads the
 * CURRENT file (logs/<datetime>.game.log); `list` shows what else is in
 * the log directory, `rotate` closes the current file and starts a fresh
 * one. New-for-Tobin: the original's logging went to zone-less stderr and
 * a syslog-style file, with no in-game reader. */

#define LOG_TAIL_DEFAULT 20
#define LOG_TAIL_MAX 100
#define LOG_MATCH_MAX 20
#define LOG_LINE_MAX 512

/* Sends the last `want` lines of the current log. Reads only the file's
 * final 32KB -- plenty for a tail, bounded regardless of file size. */
static void log_tail(descriptor_t *d, int want) {
    FILE *f = fopen(log_current_path(), "r");
    if (!f) {
        descriptor_send(d, "No log file is open.\r\n");
        return;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    long start = size > 32768 ? size - 32768 : 0;
    fseek(f, start, SEEK_SET);
    if (start > 0) {
        char skip[LOG_LINE_MAX];
        if (!fgets(skip, sizeof(skip), f)) { /* drop the partial first line */
            fclose(f);
            descriptor_send(d, "The log is empty.\r\n");
            return;
        }
    }

    /* Ring buffer of the last `want` lines. */
    char (*ring)[LOG_LINE_MAX] = calloc((size_t)want, LOG_LINE_MAX);
    if (!ring) {
        fclose(f);
        descriptor_send(d, "Out of memory.\r\n");
        return;
    }
    int count = 0;
    while (fgets(ring[count % want], LOG_LINE_MAX, f))
        count++;
    fclose(f);

    char head[96];
    snprintf(head, sizeof(head), "\r\n-- %s (last %d line%s) --\r\n",
             log_current_path(), count < want ? count : want,
             (count < want ? count : want) == 1 ? "" : "s");
    descriptor_send(d, head);
    if (count == 0)
        descriptor_send(d, "(empty)\r\n");
    int first = count > want ? count - want : 0;
    for (int i = first; i < count; i++)
        descriptor_send(d, ring[i % want]); /* lines keep their own \n */
    free(ring);
}

/* `log search <text>`: case-insensitive substring scan of the current log
 * file, keeping (and printing) only the LOG_MATCH_MAX most recent hits. */
static void log_search(descriptor_t *d, const char *needle) {
    FILE *f = fopen(log_current_path(), "r");
    if (!f) {
        descriptor_send(d, "No log file is open.\r\n");
        return;
    }

    /* Keep the LAST `LOG_MATCH_MAX` matches (recent beats ancient). */
    char (*ring)[LOG_LINE_MAX] = calloc(LOG_MATCH_MAX, LOG_LINE_MAX);
    if (!ring) {
        fclose(f);
        descriptor_send(d, "Out of memory.\r\n");
        return;
    }
    int matches = 0;
    char line[LOG_LINE_MAX];
    while (fgets(line, sizeof(line), f)) {
        /* Case-insensitive substring search, strcasestr being GNU-only
         * notwithstanding -- do it by hand for portability. */
        size_t nlen = strlen(needle);
        bool hit = false;
        for (const char *p = line; *p && !hit; p++) {
            if (strncasecmp(p, needle, nlen) == 0)
                hit = true;
        }
        if (hit) {
            snprintf(ring[matches % LOG_MATCH_MAX], LOG_LINE_MAX, "%s", line);
            matches++;
        }
    }
    fclose(f);

    char head[160];
    snprintf(head, sizeof(head), "\r\n-- %d match%s for '%s' in %s%s --\r\n",
             matches, matches == 1 ? "" : "es", needle, log_current_path(),
             matches > LOG_MATCH_MAX ? " (showing the most recent)" : "");
    descriptor_send(d, head);
    int first = matches > LOG_MATCH_MAX ? matches - LOG_MATCH_MAX : 0;
    for (int i = first; i < matches; i++)
        descriptor_send(d, ring[i % LOG_MATCH_MAX]);
    free(ring);
}

/* `log list`: pages every *.log file in LOG_DIR, marking which one is
 * currently open for writing. */
static void log_list(descriptor_t *d) {
    DIR *dir = opendir(LOG_DIR);
    if (!dir) {
        descriptor_send(d, "No log directory exists yet.\r\n");
        return;
    }
    const char *cur = strrchr(log_current_path(), '/');
    cur = cur ? cur + 1 : log_current_path();

    char out[2048];
    int n = snprintf(out, sizeof(out), "\r\n-- %s/ --\r\n", LOG_DIR);
    struct dirent *e;
    int shown = 0;
    while ((e = readdir(dir)) && (size_t)n < sizeof(out) - 96) {
        size_t nl = strlen(e->d_name);
        if (nl < 4 || strcmp(e->d_name + nl - 4, ".log") != 0)
            continue;
        n += snprintf(out + n, sizeof(out) - (size_t)n, "  %s%s\r\n", e->d_name,
                      strcmp(e->d_name, cur) == 0 ? "   <- current" : "");
        shown++;
    }
    closedir(dir);
    if (shown == 0)
        n += snprintf(out + n, sizeof(out) - (size_t)n, "  (no log files)\r\n");
    descriptor_page_start(d, out, 0);
}

/* The `log` command: bare or `log <n>` tails the current log file;
 * `search`/`rotate`/`list` dispatch to the helpers above. `rotate` is
 * gated to a higher level than the rest -- see the inline comment below. */
bool cmd_log(descriptor_t *d, const char *args) {
    if (!*args) {
        log_tail(d, LOG_TAIL_DEFAULT);
        return true;
    }

    if (isdigit((unsigned char)args[0])) {
        int want = atoi(args);
        if (want < 1)
            want = 1;
        if (want > LOG_TAIL_MAX)
            want = LOG_TAIL_MAX;
        log_tail(d, want);
        return true;
    }

    char sub[16];
    if (sscanf(args, "%15s", sub) != 1) {
        descriptor_send(d, "Usage: log [lines] | log search <text> | log rotate | log list\r\n");
        return true;
    }
    const char *rest = args + strlen(sub);
    while (*rest == ' ')
        rest++;

    size_t slen = strlen(sub);
    if (strncasecmp("search", sub, slen) == 0) {
        if (!*rest) {
            descriptor_send(d, "Search for what? Usage: log search <text>\r\n");
            return true;
        }
        log_search(d, rest);
        return true;
    }
    if (strncasecmp("rotate", sub, slen) == 0 && slen >= 1) {
        /* Rotation is isolated above the rest of the log command (user
         * spec, Tier 3): 54+ can read/search/list, only 59+ may rotate. */
        if (!d->character || d->character->progress.level < LOG_ROTATE_MIN_LEVEL) {
            descriptor_send(d, "Log rotation requires level 59.\r\n");
            return true;
        }
        if (log_rotate()) {
            char msg[LOG_PATH_MAX + 64];
            snprintf(msg, sizeof(msg),
                     "Logs are one file per day now; re-opened %s.\r\n",
                     log_current_path());
            descriptor_send(d, msg);
        } else {
            descriptor_send(d, "Rotation failed -- see the console.\r\n");
        }
        return true;
    }
    if (strncasecmp("list", sub, slen) == 0) {
        log_list(d);
        return true;
    }

    descriptor_send(d, "Usage: log [lines] | log search <text> | log rotate | log list\r\n");
    return true;
}
