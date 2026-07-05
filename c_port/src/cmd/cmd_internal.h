/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
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
bool cmd_flee(descriptor_t *d, const char *args);
bool cmd_say(descriptor_t *d, const char *args);
bool cmd_limbs(descriptor_t *d, const char *args);
bool cmd_help(descriptor_t *d, const char *args);
bool cmd_wizhelp(descriptor_t *d, const char *args);
bool cmd_goto(descriptor_t *d, const char *args);
bool cmd_promote(descriptor_t *d, const char *args);
bool cmd_hedit(descriptor_t *d, const char *args);
bool cmd_copyover(descriptor_t *d, const char *args);
bool cmd_north(descriptor_t *d, const char *args);
bool cmd_east(descriptor_t *d, const char *args);
bool cmd_south(descriptor_t *d, const char *args);
bool cmd_west(descriptor_t *d, const char *args);
bool cmd_up(descriptor_t *d, const char *args);
bool cmd_down(descriptor_t *d, const char *args);
bool cmd_northeast(descriptor_t *d, const char *args);
bool cmd_northwest(descriptor_t *d, const char *args);
bool cmd_southeast(descriptor_t *d, const char *args);
bool cmd_southwest(descriptor_t *d, const char *args);
bool cmd_edit(descriptor_t *d, const char *args);
bool cmd_log(descriptor_t *d, const char *args);
bool cmd_exits(descriptor_t *d, const char *args);
bool cmd_loadroom(descriptor_t *d, const char *args);
bool cmd_mortal(descriptor_t *d, const char *args);
bool cmd_immort(descriptor_t *d, const char *args);
bool cmd_prompt(descriptor_t *d, const char *args);
bool cmd_title(descriptor_t *d, const char *args);
bool cmd_toggle(descriptor_t *d, const char *args);
bool cmd_exec(descriptor_t *d, const char *args);
bool cmd_users(descriptor_t *d, const char *args);
bool cmd_news(descriptor_t *d, const char *args);
bool cmd_addnews(descriptor_t *d, const char *args);
bool cmd_stand(descriptor_t *d, const char *args);
bool cmd_sit(descriptor_t *d, const char *args);
bool cmd_rest(descriptor_t *d, const char *args);
bool cmd_sleep(descriptor_t *d, const char *args);
bool cmd_wake(descriptor_t *d, const char *args);
bool cmd_catchup(descriptor_t *d, const char *args);
bool cmd_wiznews(descriptor_t *d, const char *args);
bool cmd_edwiznews(descriptor_t *d, const char *args);
bool cmd_socials(descriptor_t *d, const char *args);
bool cmd_wiznet(descriptor_t *d, const char *args);
bool cmd_system(descriptor_t *d, const char *args);
bool cmd_mudstats(descriptor_t *d, const char *args);
bool cmd_multiplay(descriptor_t *d, const char *args);

/* `hedit`'s gate (user-specified): level 56+, i.e. senior "God"-tier
 * immortals and up, not every 51+ immortal. */
#define HELP_EDIT_MIN_LEVEL 56

/* `copyover` reboots the server binary in place -- the most consequential
 * command there is, so it's gated at Administrator (59) and up. */
#define COPYOVER_MIN_LEVEL 59

/* `exec` runs shell commands on the host box -- Implementor-only (60). */
#define EXEC_MIN_LEVEL 60

/* `redit` (the room builder): 51+ -- every immortal builds (user spec,
 * Session 21; future oedit/medit/zedit land at 51 too). Help editing
 * (hedit) stays at its own higher tier. */
#define BUILD_MIN_LEVEL 51

/* `log` (read/search/list the game log files): 54+; `log rotate` alone is
 * isolated to 59+ (both user-specified, Tier 3). */
#define LOG_MIN_LEVEL 54
#define LOG_ROTATE_MIN_LEVEL 59

/* `promote`: 58+ (user-specified, Tier 3 -- was 51+). */
#define PROMOTE_MIN_LEVEL 58

/* `users` (connection roster with IPs): 58+, user-specified. */
#define USERS_MIN_LEVEL 58

/* `addnews` (post a news item): 56+, user-specified. */
#define ADDNEWS_MIN_LEVEL 56

/* `multiplay` (toggle the mortal-multiplay game flag): 59+, user-specified. */
#define MULTIPLAY_MIN_LEVEL 59

/* One row of cmd_table.c's dispatch table -- shared with cmd_help.c so
 * `help`/`wizhelp` can enumerate it without duplicating the list.
 * min_level is ENFORCED by cmd_dispatch() as of Phase 2A: a command above
 * the caller's level is skipped during matching entirely, so to a mortal
 * an immortal command is indistinguishable from one that doesn't exist
 * ("Huh?!") -- same as the original's commandInfo::minLevel dispatch gate. */
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
