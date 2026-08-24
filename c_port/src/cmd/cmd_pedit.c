/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>

/* `pedit <name>`: Administrator (58+) only -- a menu-driven editor for
 * a player's level, experience, HP/max HP, attributes, gender, title,
 * load room, and handedness. Same menu-driven shape as edroom, and an
 * "admin superset of promote" (TODO.md) -- promote only ever sets level;
 * this can set everything promote can plus everything else a player
 * record persists. Works on any player, online or offline, by exact name
 * (player_load_admin() is not account-scoped, like promote). */
bool cmd_pedit(descriptor_t *d, const char *args) {
    if (!d->character)
        return true;

    char name[64];
    if (sscanf(args, "%63s", name) != 1) {
        { char __b[64]; snprintf(__b, sizeof(__b), "Usage: %s <name>\r\n", edit_verb_label(d, "pedit", "edit player")); descriptor_send(d, __b); }
        return true;
    }

    if (!descriptor_edplayer_begin(d, name)) {
        char msg[96];
        snprintf(msg, sizeof(msg), "No player named '%s' exists.\r\n", name);
        descriptor_send(d, msg);
    }
    return true;
}
