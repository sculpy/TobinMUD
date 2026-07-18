/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>

/* `edit account <name>`: Administrator (58+, matching edplayer's tier) --
 * a menu-driven editor for any account: rename it, reset its password, or
 * list its characters (TODO.md). No self-service equivalent -- a player
 * manages their OWN account name/password only by asking an immortal to
 * do this, same as promote/edplayer aren't self-service either. */
bool cmd_edaccount(descriptor_t *d, const char *args) {
    if (!d->character)
        return true;

    char name[80];
    if (sscanf(args, "%79s", name) != 1) {
        descriptor_send(d, "Usage: edit account <name>\r\n");
        return true;
    }

    if (!descriptor_edaccount_begin(d, name)) {
        char msg[112];
        snprintf(msg, sizeof(msg), "No account named '%s' exists.\r\n", name);
        descriptor_send(d, msg);
    }
    return true;
}
