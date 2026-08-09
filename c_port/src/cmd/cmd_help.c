/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
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
#include "skill.h"

/* qsort() comparator for an array of `const char *` names -- case-insensitive
 * alphabetical order, used to sort help-topic name listings. */
static int cmp_name(const void *a, const void *b) {
    return strcasecmp(*(const char *const *)a, *(const char *const *)b);
}

/* Strips a trailing "<label> <value>" directive line off the END of
 * `desc[0..*dlen)` (in place, by shrinking *dlen), same convention as the
 * original "Related:" line below -- an author just types it as the last
 * line of the topic in the shared line editor. Returns true and fills
 * `out` if the topic's current last line starts with `label`; leaves
 * everything untouched otherwise. Called once per directive, working
 * backward from the end, so directives stack in the order authored
 * (bottommost line stripped first). */
static bool extract_trailing_directive(const char *desc, size_t *dlen, const char *label,
                                       char *out, size_t outsz) {
    size_t last_nl = *dlen;
    for (size_t i = *dlen; i > 0; i--) {
        if (desc[i - 1] == '\n') {
            last_nl = i;
            break;
        }
        if (i == 1)
            last_nl = 0;
    }
    const char *last_line = desc + last_nl;
    size_t last_line_len = *dlen - last_nl;
    size_t label_len = strlen(label);
    if (last_line_len <= label_len || strncasecmp(last_line, label, label_len) != 0)
        return false;

    const char *v = last_line + label_len;
    while (*v == ' ')
        v++;
    size_t vlen = (size_t)(last_line + last_line_len - v);
    if (vlen >= outsz)
        vlen = outsz - 1;
    memcpy(out, v, vlen);
    out[vlen] = '\0';

    *dlen = last_nl;
    while (*dlen > 0 && (desc[*dlen - 1] == '\n' || desc[*dlen - 1] == '\r'))
        (*dlen)--;
    return true;
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
    descriptor_page_start(d, out, 0);
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

    /* `help <topic>`: DB-backed prose topics (db/tobin/help_topic.sql,
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

            /* Multi-word topic names (user 2026-07-18: skill/spell help
             * topics like "cure poison", "dual wield", "set trap (door)")
             * -- try the FULL raw argument as an exact topic name first,
             * lowercased/trimmed/truncated. Without this, `help` only ever
             * looked up the single FIRST token (see the sscanf above), so
             * "help cure poison" would look up just "cure" -- and since
             * several topics now share that prefix (cure blindness/
             * disease/poison/serious/...), help_topic_find()'s `LIKE
             * 'cure%' ORDER BY name LIMIT 1` prefix fallback would
             * silently resolve to the alphabetically-first one instead of
             * the one actually asked for. Skipped when the edit-noun
             * folding above already built a multi-word `topic` itself
             * (starts with "edit "), to avoid a redundant duplicate
             * lookup of that exact same string. */
            bool topic_found = false;
            if (strchr(args, ' ') && strncmp(topic, "edit ", 5) != 0) {
                char full[HELP_TOPIC_NAME_LEN];
                size_t flen = 0;
                for (const char *p = args; *p && flen < sizeof(full) - 1; p++)
                    full[flen++] = (char)tolower((unsigned char)*p);
                while (flen > 0 && full[flen - 1] == ' ')
                    flen--;
                full[flen] = '\0';
                if (flen > 0) {
                    topic_found = help_topic_find(full, resolved, sizeof(resolved), body, sizeof(body));
                    if (topic_found)
                        snprintf(topic, sizeof(topic), "%s", full);
                }
            }
            if (!topic_found)
                topic_found = help_topic_find(topic, resolved, sizeof(resolved), body, sizeof(body));

            if (topic_found) {
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
                     * a line at the end for related topics"). Only shown
                     * when present; most topics have none. "Requires: ..."
                     * (user 2026-07-18: "spell components should be listed
                     * in the footer before related") is the same
                     * trailing-directive convention, authored one line
                     * further up (right above Related) -- so Related, the
                     * true last line, is peeled off first, exposing
                     * Requires as the new last line for the second peel.
                     * Used by skill/spell topics to name what `cast`/`pray`
                     * needs on hand (a component, a holy symbol, or
                     * nothing/not yet a real command, for the many
                     * still-placeholder physical skills). "Approx. Level:"
                     * and "Classes:" (skill_help.sql redesign, 2026-07-22,
                     * user: "line up the : to make it more readable and
                     * colorize appropriate") are the same trailing-
                     * directive convention too, authored in this order
                     * (top to bottom): Requires, Related, Approx. Level,
                     * Classes -- Classes is the true LAST line, peeled
                     * first below. Both are skill/spell-topic-only (no
                     * real `cmd_entry_t` to read a genuine minimum level
                     * or class list from), pulled out of body prose and
                     * into the same aligned/colorized footer as Syntax/
                     * Requires/Related instead of staying plain white
                     * body text, so every label in the footer lines up on
                     * the same colon column. */
                    char related[128];
                    related[0] = '\0';
                    char requires[128];
                    requires[0] = '\0';
                    char approx_level[32];
                    approx_level[0] = '\0';
                    char classes[128];
                    classes[0] = '\0';
                    char discipline[16];
                    discipline[0] = '\0';
                    /* Peel bottom-up in the order these are actually
                     * authored (see skill_help.sql's own generator note):
                     * Classes (true last line) -> Discipline (present only
                     * when the upstream source had a real value for this
                     * spell -- peeling a directive that isn't actually
                     * there is a harmless no-op, so this order is safe
                     * whether or not it's present) -> Approx. Level ->
                     * Related -> Requires. */
                    (void)extract_trailing_directive(desc, &dlen, "Classes:", classes, sizeof(classes));
                    (void)extract_trailing_directive(desc, &dlen, "Discipline:", discipline, sizeof(discipline));
                    (void)extract_trailing_directive(desc, &dlen, "Approx. Level:", approx_level, sizeof(approx_level));
                    (void)extract_trailing_directive(desc, &dlen, "Related:", related, sizeof(related));
                    (void)extract_trailing_directive(desc, &dlen, "Requires:", requires, sizeof(requires));

                    char shown[HELP_BODY_MAX + 32];
                    snprintf(shown, sizeof(shown), "<W>%.*s<z>\r\n", (int)dlen, desc);
                    descriptor_send(d, shown);

                    /* Cyan-labelled Syntax / Minimum Level (or Approx.
                     * Level) / Requires / Related footer. Syntax shows
                     * whenever a syntax string exists -- a real command
                     * (`match`) always has one (falls back to its own
                     * name); a skill/spell topic has one only if its body
                     * authored a leading "Usage:" line (cast/pray-
                     * reachable ones do; still-placeholder physical
                     * skills don't, so they get no Syntax line at all).
                     * Minimum Level is real-command-only (from the actual
                     * table); Approx. Level is its skill/spell-topic
                     * analogue, authored directly in the body since these
                     * have no table entry to read a real minimum from.
                     * Requires/Related show on any topic that authored
                     * one. Every label -- Classes/Syntax/Minimum Level/
                     * Approx. Level/Requires/Related -- right-aligns its
                     * OWN colon to the same 14-char column (user:
                     * "line up the : to make it more readable"), all in
                     * the same cyan (user: "colorize appropriate", same
                     * <c> tag every other footer label already used). A
                     * leading blank line separates the footer from the
                     * description body, added once, before whichever
                     * piece ends up first. */
                    bool footer_started = false;
                    if (classes[0]) {
                        char clsfooter[192];
                        snprintf(clsfooter, sizeof(clsfooter), "\r\n<c>      Classes:<z> %s\r\n", classes);
                        descriptor_send(d, clsfooter);
                        footer_started = true;
                    }
                    if (syntax[0]) {
                        char footer[192];
                        snprintf(footer, sizeof(footer), "%s<c>       Syntax:<z> %s\r\n",
                                 footer_started ? "" : "\r\n", syntax);
                        descriptor_send(d, footer);
                        footer_started = true;
                    }
                    if (match) {
                        char lvlfooter[64];
                        snprintf(lvlfooter, sizeof(lvlfooter), "%s<c>Minimum Level:<z> %d\r\n",
                                 footer_started ? "" : "\r\n", match->min_level);
                        descriptor_send(d, lvlfooter);
                        footer_started = true;
                    }
                    if (requires[0]) {
                        char reqfooter[192];
                        snprintf(reqfooter, sizeof(reqfooter), "%s<c>     Requires:<z> %s\r\n",
                                 footer_started ? "" : "\r\n", requires);
                        descriptor_send(d, reqfooter);
                        footer_started = true;
                    }
                    if (related[0]) {
                        char relfooter[192];
                        snprintf(relfooter, sizeof(relfooter), "%s<c>      Related:<z> %s\r\n",
                                 footer_started ? "" : "\r\n", related);
                        descriptor_send(d, relfooter);
                        footer_started = true;
                    }
                    if (approx_level[0]) {
                        char alfooter[64];
                        snprintf(alfooter, sizeof(alfooter), "%s<c>Approx. Level:<z> %s\r\n",
                                 footer_started ? "" : "\r\n", approx_level);
                        descriptor_send(d, alfooter);
                        footer_started = true;
                    }
                    if (discipline[0]) {
                        /* User, 2026-08-09: "helpfiles should report
                         * discipline as basic combat or advanced, not a
                         * %" -- the raw upstream discArray[] 0-100 value
                         * (still parsed above so it's stripped out of the
                         * body) never mapped onto anything a Tobin player
                         * could act on, since discipline here is a real
                         * basic_disc_pct/combat_disc_pct/advanced_disc_pct
                         * TIER gate (see `skills`, cmd_skills.c), not a
                         * per-spell percentage. Look the topic's own
                         * skill_def_t up (first class match is a fine
                         * approximation -- a skill's tier basically never
                         * differs across the classes that share it) and
                         * show its real tier instead: Basic/Combat/
                         * Advanced, same three words `skills`/practice
                         * refusal messages already use. */
                        const char *disc_word = NULL;
                        const skill_def_t *dsk = skill_find(CLASS_WARRIOR, resolved, true);
                        if (dsk) {
                            switch (dsk->tier) {
                                case SKILL_TIER_COMBAT:   disc_word = "Combat";   break;
                                case SKILL_TIER_ADVANCED: disc_word = "Advanced"; break;
                                case SKILL_TIER_CLASS:
                                default:                  disc_word = "Basic";   break;
                            }
                        }
                        if (disc_word) {
                            char discfooter[64];
                            snprintf(discfooter, sizeof(discfooter), "%s<c>   Discipline:<z> %s\r\n",
                                     footer_started ? "" : "\r\n", disc_word);
                            descriptor_send(d, discfooter);
                        }
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
