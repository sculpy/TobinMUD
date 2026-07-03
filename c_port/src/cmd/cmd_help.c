#include "cmd_internal.h"

#include <stdio.h>

#include "being.h"

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
    (void)args;

    int count;
    const cmd_entry_t *cmds = cmd_table_entries(&count);

    char out[2048];
    int n = snprintf(out, sizeof(out), "\r\n-- Available commands --\r\n");
    if (n < 0)
        n = 0;

    for (int i = 0; i < count && (size_t)n < sizeof(out); i++) {
        n += snprintf(out + n, sizeof(out) - (size_t)n, "  %-10s %s\r\n",
                      cmds[i].name, cmds[i].help);
    }
    /* `quit!` is deliberately excluded from cmd_table.c's dispatch table
     * (see its comment there) -- listed here as a hardcoded extra line so
     * it isn't missing from the list players actually see. */
    if ((size_t)n < sizeof(out))
        n += snprintf(out + n, sizeof(out) - (size_t)n, "  %-10s %s\r\n",
                      "quit!", "Leave your character, or disconnect (must be typed in full).");

    descriptor_send(d, out);
    return true;
}

/* `wizhelp`: lists commands restricted to immortals (min_level above
 * MORTAL_LEVEL_MAX). A genuine port of the original's TBeing::doWizhelp()
 * mechanism (cmd/cmd_help.cc) -- unlike `help`, that one really is a
 * command-table scan filtered by `commandArray[i]->minLevel > MAX_MORT`,
 * not a file lookup, so this is a direct match rather than a
 * simplification. No command in Tobin is min_level-gated to immortals yet
 * (see cmd_table.c's COMMANDS[]) -- min_level exists as display metadata
 * ahead of the first command that actually needs it, so this currently
 * prints an honest "none yet" rather than an empty/broken list. */
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

    bool any = false;
    for (int i = 0; i < count && (size_t)n < sizeof(out); i++) {
        if (cmds[i].min_level <= MORTAL_LEVEL_MAX)
            continue;
        any = true;
        n += snprintf(out + n, sizeof(out) - (size_t)n, "  %-10s %s\r\n",
                      cmds[i].name, cmds[i].help);
    }
    if (!any && (size_t)n < sizeof(out))
        n += snprintf(out + n, sizeof(out) - (size_t)n,
                      "  (none yet -- no commands are currently immortal-only)\r\n");

    descriptor_send(d, out);
    return true;
}
