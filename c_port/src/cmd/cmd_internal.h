#ifndef CMD_INTERNAL_H
#define CMD_INTERNAL_H

#include <stdbool.h>

#include "descriptor.h"

/* Internal wiring between cmd_table.c and each cmd_*.c handler -- not part
 * of the public include/ API surface. Each handler returns false to
 * request the connection be closed (only cmd_quit does this), true
 * otherwise. */

bool cmd_look(descriptor_t *d, const char *args);
bool cmd_who(descriptor_t *d, const char *args);
bool cmd_score(descriptor_t *d, const char *args);
bool cmd_quit(descriptor_t *d, const char *args);
bool cmd_color(descriptor_t *d, const char *args);
bool cmd_attack(descriptor_t *d, const char *args);
bool cmd_kill(descriptor_t *d, const char *args);
bool cmd_say(descriptor_t *d, const char *args);
bool cmd_limbs(descriptor_t *d, const char *args);
bool cmd_help(descriptor_t *d, const char *args);
bool cmd_wizhelp(descriptor_t *d, const char *args);

/* One row of cmd_table.c's dispatch table -- shared with cmd_help.c so
 * `help`/`wizhelp` can enumerate it without duplicating the list. `help`
 * is display-only metadata for now: min_level isn't enforced by
 * cmd_dispatch() (nothing currently needs it to be, since no command is
 * actually restricted to immortals the way the original's real
 * commandInfo::minLevel gates dispatch -- see STATUS.md). */
typedef struct {
    const char *name;
    bool (*fn)(descriptor_t *, const char *);
    const char *help;
    int min_level;
} cmd_entry_t;

/* Read-only view of cmd_table.c's COMMANDS[] for `help`/`wizhelp`
 * (cmd_help.c) to iterate. `quit!` is NOT included -- it's deliberately
 * excluded from the dispatch table entirely (see cmd_table.c), so
 * cmd_help.c lists it as a hardcoded extra line instead. */
const cmd_entry_t *cmd_table_entries(int *count);

#endif
