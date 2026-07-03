#include "cmd_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "being.h"
#include "help_repo.h"

/* `help`: lists every available command with a one-line description.
 * Deliberately NOT a port of the original's `help` (TBeing::doHelp(),
 * cmd/cmd_help.cc) -- that's a full file-based prose-topic lookup system
 * (help/, help/_immortal, help/_skills, etc, with a rebuildable index and
 * per-topic .ansi variants), way out of scope for a command list. Tobin
 * has no help-file infrastructure and no plan to build one yet, so this
 * simplifies down to the same list-based pattern `wizhelp` genuinely uses
 * in the original (see below) -- listing what already exists in
 * cmd_table.c's metadata, nothing more. */
bool cmd_help(descriptor_t *d, const char *args) {
    int count;
    const cmd_entry_t *cmds = cmd_table_entries(&count);
    int level = d->character ? d->character->progress.level : MORTAL_LEVEL_MIN;

    /* `help <topic>`: DB-backed prose topics (db/sneezy/help_topic.sql,
     * editable in-game via `hedit`) -- a step back toward the original's
     * real file-based topic system, but stored in MariaDB like all other
     * Tobin content. Exact topic name first, then prefix. */
    if (*args) {
        char topic[HELP_TOPIC_NAME_LEN];
        if (sscanf(args, "%31s", topic) == 1) {
            for (char *p = topic; *p; p++)
                *p = (char)tolower((unsigned char)*p);

            char resolved[HELP_TOPIC_NAME_LEN];
            char body[HELP_BODY_MAX];
            if (help_topic_find(topic, resolved, sizeof(resolved), body, sizeof(body))) {
                /* Don't let a topic leak a command that's hidden from this
                 * caller (cmd_dispatch() hides over-level commands). */
                bool hidden = false;
                for (int i = 0; i < count; i++) {
                    if (strcasecmp(cmds[i].name, resolved) == 0
                        && cmds[i].min_level > level) {
                        hidden = true;
                        break;
                    }
                }
                if (!hidden) {
                    char head[64];
                    snprintf(head, sizeof(head), "\r\n-- Help: %s --\r\n", resolved);
                    descriptor_send(d, head);
                    descriptor_send(d, body);
                    if (body[0] && body[strlen(body) - 1] != '\n')
                        descriptor_send(d, "\r\n");
                    return true;
                }
            }
            char msg[80];
            snprintf(msg, sizeof(msg), "No help available on '%s'.\r\n", topic);
            descriptor_send(d, msg);
            return true;
        }
    }

    char out[2048];
    int n = snprintf(out, sizeof(out), "\r\n-- Available commands --\r\n");
    if (n < 0)
        n = 0;

    /* Only list what this caller can actually use -- cmd_dispatch() hides
     * over-level commands entirely (Phase 2A), so listing them here would
     * leak their existence to mortals. */
    for (int i = 0; i < count && (size_t)n < sizeof(out); i++) {
        if (cmds[i].min_level > level)
            continue;
        n += snprintf(out + n, sizeof(out) - (size_t)n, "  %-10s %s\r\n",
                      cmds[i].name, cmds[i].help);
    }
    /* `quit!` is deliberately excluded from cmd_table.c's dispatch table
     * (see its comment there) -- listed here as a hardcoded extra line so
     * it isn't missing from the list players actually see. */
    if ((size_t)n < sizeof(out))
        n += snprintf(out + n, sizeof(out) - (size_t)n, "  %-10s %s\r\n",
                      "quit!", "Leave your character, or disconnect (must be typed in full).");
    if ((size_t)n < sizeof(out))
        n += snprintf(out + n, sizeof(out) - (size_t)n,
                      "\r\nType 'help <command>' for details on any of these.\r\n");

    descriptor_send(d, out);
    return true;
}

/* `wizhelp`: lists commands restricted to immortals (min_level above
 * MORTAL_LEVEL_MAX). A genuine port of the original's TBeing::doWizhelp()
 * mechanism (cmd/cmd_help.cc) -- unlike `help`, that one really is a
 * command-table scan filtered by `commandArray[i]->minLevel > MAX_MORT`,
 * not a file lookup, so this is a direct match rather than a
 * simplification. Real immortal-only commands exist as of Phase 2A
 * (`goto`, `promote`); the "none yet" line below survives only as a
 * fallback should the table ever have none again. */
bool cmd_wizhelp(descriptor_t *d, const char *args) {
    (void)args;

    if (!d->character || !being_is_immortal(d->character)) {
        descriptor_send(d, "You are not privileged enough to use that command.\r\n");
        return true;
    }

    int count;
    const cmd_entry_t *cmds = cmd_table_entries(&count);

    char out[1024];
    int n = snprintf(out, sizeof(out), "\r\n-- Immortal-only commands --\r\n");
    if (n < 0)
        n = 0;

    /* Same secrecy rule as help/cmd_dispatch (user requirement): an
     * immortal only sees the commands their own level already grants --
     * what the next promotion unlocks stays unknown until it happens. The
     * [N+] tag therefore only ever shows levels at or below the caller's. */
    int level = d->character->progress.level;
    bool any = false;
    for (int i = 0; i < count && (size_t)n < sizeof(out); i++) {
        if (cmds[i].min_level <= MORTAL_LEVEL_MAX || cmds[i].min_level > level)
            continue;
        any = true;
        n += snprintf(out + n, sizeof(out) - (size_t)n, "  %-10s [%d+] %s\r\n",
                      cmds[i].name, cmds[i].min_level, cmds[i].help);
    }
    if (!any && (size_t)n < sizeof(out))
        n += snprintf(out + n, sizeof(out) - (size_t)n,
                      "  (none yet -- no commands are currently immortal-only)\r\n");

    descriptor_send(d, out);
    return true;
}
