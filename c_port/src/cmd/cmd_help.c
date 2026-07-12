/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "being.h"
#include "help_repo.h"

static int cmp_name(const void *a, const void *b) {
    return strcasecmp(*(const char *const *)a, *(const char *const *)b);
}

/* Sends `names` (count `cnt`) sorted alphabetically in three columns, under
 * `header` and followed by `footer`. Shared by help and wizhelp. */
static void send_columns(descriptor_t *d, const char **names, int cnt,
                         const char *header, const char *footer) {
    qsort(names, (size_t)cnt, sizeof(names[0]), cmp_name);

    /* Sized to hold the whole command/topic list without truncating as the
     * game grows -- 8 KB fits several hundred entries (18 chars each). */
    char out[8192];
    size_t n = (size_t)snprintf(out, sizeof(out), "%s", header);
    for (int i = 0; i < cnt && n < sizeof(out); i++) {
        n += (size_t)snprintf(out + n, sizeof(out) - n, "  %-16s", names[i]);
        if (i % 3 == 2)
            n += (size_t)snprintf(out + n, sizeof(out) > n ? sizeof(out) - n : 0, "\r\n");
    }
    if (cnt % 3 != 0 && n < sizeof(out))
        n += (size_t)snprintf(out + n, sizeof(out) - n, "\r\n");
    if (footer && n < sizeof(out))
        snprintf(out + n, sizeof(out) - n, "%s", footer);
    descriptor_send(d, out);
}

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

            /* Alias resolution (user spec): the short forms land on the
             * canonical topic -- one help file per command family. */
            static const struct { const char *alias, *canon; } ALIASES[] = {
                { "ne", "northeast" }, { "nw", "northwest" },
                { "se", "southeast" }, { "sw", "southwest" },
                { "'", "say" },
            };
            for (size_t a = 0; a < sizeof(ALIASES) / sizeof(ALIASES[0]); a++) {
                if (strcmp(topic, ALIASES[a].alias) == 0) {
                    snprintf(topic, sizeof(topic), "%s", ALIASES[a].canon);
                    break;
                }
            }

            /* "help edit <noun>" (user 2026-07-11: one topic per editor
             * noun, e.g. "help edit room") -- `help` only reads the FIRST
             * whitespace token above, so without this, "edit room" would
             * silently collapse to just "edit". When the first token is
             * "edit" and a second token follows, fold them into a single
             * two-word topic name ("edit room") and look THAT up instead;
             * bare "help edit" (no noun) is untouched, still resolving to
             * the general overview topic. */
            if (strcmp(topic, "edit") == 0) {
                const char *after = args;
                while (*after && *after != ' ')
                    after++;
                while (*after == ' ')
                    after++;
                char noun[24];
                if (*after && sscanf(after, "%23s", noun) == 1) {
                    for (char *p = noun; *p; p++)
                        *p = (char)tolower((unsigned char)*p);
                    /* `trigger` keeps its own standalone topic name (predates
                     * this two-word scheme, already comprehensive) rather
                     * than a duplicate "edit trigger" -- so "help trigger"
                     * and "help edit trigger" both keep resolving to it. */
                    if (strcmp(noun, "trigger") == 0)
                        snprintf(topic, sizeof(topic), "trigger");
                    else
                        snprintf(topic, sizeof(topic), "edit %s", noun);
                }
            }

            char resolved[HELP_TOPIC_NAME_LEN];
            char body[HELP_BODY_MAX];
            if (help_topic_find(topic, resolved, sizeof(resolved), body, sizeof(body))) {
                /* Don't let a topic leak a command that's hidden from this
                 * caller (cmd_dispatch() hides over-level commands). */
                bool hidden = false;
                const cmd_entry_t *match = NULL;
                for (int i = 0; i < count; i++) {
                    if (strcasecmp(cmds[i].name, resolved) == 0) {
                        match = &cmds[i];
                        if (cmds[i].min_level > level)
                            hidden = true;
                        break;
                    }
                }
                if (!hidden) {
                    /* Title-case the command name in the header (user
                     * 2026-07-11: "proper case for the command") --
                     * `resolved` itself stays lowercase (it's also used for
                     * DB lookups/aliasing elsewhere), this only affects the
                     * displayed heading. */
                    char titled[HELP_TOPIC_NAME_LEN];
                    snprintf(titled, sizeof(titled), "%s", resolved);
                    if (titled[0])
                        titled[0] = (char)toupper((unsigned char)titled[0]);

                    char head[80];
                    snprintf(head, sizeof(head), "\r\n<c>-- Help: %s --<z>\r\n", titled);
                    descriptor_send(d, head);

                    /* Split a leading "Usage: <syntax>" line out of the body:
                     * the syntax goes into the colorized footer, the rest is
                     * the description (user-specified help format). */
                    char syntax[128];
                    const char *desc = body;
                    syntax[0] = '\0';
                    if (strncasecmp(body, "Usage:", 6) == 0) {
                        const char *nl = strchr(body, '\n');
                        const char *s = body + 6;
                        while (*s == ' ')
                            s++;
                        size_t slen = nl ? (size_t)(nl - s) : strlen(s);
                        while (slen > 0 && s[slen - 1] == '\r')
                            slen--;
                        if (slen >= sizeof(syntax))
                            slen = sizeof(syntax) - 1;
                        memcpy(syntax, s, slen);
                        syntax[slen] = '\0';
                        if (nl) {
                            desc = nl + 1;
                            while (*desc == '\r' || *desc == '\n')
                                desc++; /* skip the blank line after Usage */
                        } else {
                            desc = body + strlen(body);
                        }
                    }
                    if (syntax[0] == '\0' && match)
                        snprintf(syntax, sizeof(syntax), "%s", match->name);

                    /* Bright white description body (user 2026-07-11:
                     * "colorize help files with <W>" -- was magenta). The
                     * <W>...<z> pair MUST be in a single send: colorstring
                     * auto-appends a reset when a message ends mid-color, so
                     * splitting <W> off would reset it immediately. Trailing
                     * newlines trimmed to one. */
                    size_t dlen = strlen(desc);
                    while (dlen > 0 && (desc[dlen - 1] == '\n' || desc[dlen - 1] == '\r'))
                        dlen--;

                    /* Trailing "Related: topic topic ..." line (user
                     * 2026-07-11: "for help topics both wizhelp and help add
                     * a line at the end for related topics"). Same
                     * strip-a-directive-line convention as the leading
                     * "Usage:" line above, but at the END of the body
                     * instead of the start -- an author just types it as the
                     * last line in the same shared line editor. Only shown
                     * when present; most topics have none. */
                    char related[128];
                    related[0] = '\0';
                    {
                        size_t last_nl = dlen;
                        for (size_t i = dlen; i > 0; i--) {
                            if (desc[i - 1] == '\n') {
                                last_nl = i;
                                break;
                            }
                            if (i == 1)
                                last_nl = 0;
                        }
                        const char *last_line = desc + last_nl;
                        size_t last_line_len = dlen - last_nl;
                        if (last_line_len > 8 && strncasecmp(last_line, "Related:", 8) == 0) {
                            const char *r = last_line + 8;
                            while (*r == ' ')
                                r++;
                            size_t rlen = (size_t)(last_line + last_line_len - r);
                            if (rlen >= sizeof(related))
                                rlen = sizeof(related) - 1;
                            memcpy(related, r, rlen);
                            related[rlen] = '\0';
                            dlen = last_nl;
                            while (dlen > 0 && (desc[dlen - 1] == '\n' || desc[dlen - 1] == '\r'))
                                dlen--;
                        }
                    }

                    char shown[HELP_BODY_MAX + 32];
                    snprintf(shown, sizeof(shown), "<W>%.*s<z>\r\n", (int)dlen, desc);
                    descriptor_send(d, shown);

                    /* Cyan-labelled Syntax / Minimum Level footer -- commands
                     * only (prose topics have no table entry, so no footer).
                     * Labels right-aligned to 14 chars (user-specified). */
                    if (match) {
                        char footer[192];
                        snprintf(footer, sizeof(footer),
                                 "\r\n<c>       Syntax:<z> %s\r\n<c>Minimum Level:<z> %d\r\n",
                                 syntax, match->min_level);
                        descriptor_send(d, footer);
                    }
                    if (related[0]) {
                        char relfooter[192];
                        snprintf(relfooter, sizeof(relfooter), "%s<c>      Related:<z> %s\r\n",
                                 match ? "" : "\r\n", related);
                        descriptor_send(d, relfooter);
                    }
                    return true;
                }
            }
            char msg[80];
            snprintf(msg, sizeof(msg), "No help available on '%s'.\r\n", topic);
            descriptor_send(d, msg);
            return true;
        }
    }

    /* List only what this caller can actually use (over-level commands are
     * hidden entirely -- Phase 2A). Names only, sorted alphabetically, in
     * three columns; `help <command>` gives the details. */
    const char *names[512];
    int cnt = 0;
    for (int i = 0; i < count && cnt < 511; i++) { /* leave a slot for quit! */
        if (!cmds[i].help)
            continue; /* NULL help = deliberately unlisted (aliases, immort) */
        if (cmds[i].min_level <= level)
            names[cnt++] = cmds[i].name;
    }
    /* `quit!` is deliberately excluded from cmd_table.c's dispatch table, so
     * add it by hand -- otherwise it'd be missing from the list. */
    names[cnt++] = "quit!";

    send_columns(d, names, cnt, "\r\n-- Available commands --\r\n",
                 "\r\nType 'help <command>' for details on any of these.\r\n"
                 "New here? Type 'help playing' for an overview of the basics.\r\n");
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
    int level = d->character->progress.level;

    /* Secrecy rule (user requirement): an immortal only sees the immortal
     * commands their own level already grants -- what a future promotion
     * unlocks stays unknown until it happens. Names only, alphabetical, in
     * three columns; no level tag (user request). */
    const char *names[512];
    int cnt = 0;
    for (int i = 0; i < count && cnt < 512; i++) {
        if (!cmds[i].help)
            continue;
        if (cmds[i].min_level > MORTAL_LEVEL_MAX && cmds[i].min_level <= level)
            names[cnt++] = cmds[i].name;
    }

    if (cnt == 0) {
        descriptor_send(d,
            "\r\n-- Immortal-only commands --\r\n"
            "  (none yet -- no commands are currently immortal-only)\r\n");
        return true;
    }
    send_columns(d, names, cnt, "\r\n-- Immortal-only commands --\r\n",
                 "\r\nType 'help <command>' for details on any of these.\r\n"
                 "New immortal? Type 'help administration' for the why, not just the what.\r\n");
    return true;
}
