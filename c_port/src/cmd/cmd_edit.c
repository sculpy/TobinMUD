/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include "room.h"

/* `redit [<vnum>]` -- opens the menu-driven room builder. With a leading
 * room number it edits that room from anywhere (like goto, but for editing);
 * otherwise it edits the room you're standing in. The whole editor -- the
 * menu, the flag/terrain/exit submenus, the working-copy Save/Quit model --
 * lives in descriptor.c's CONN_REDIT_* state machine (see
 * descriptor_redit_begin), mirroring how character creation is structured.
 *
 * This replaced the original one-shot command form (`redit name <text>`,
 * `redit exit <dir> <toroom>`, ...) at user request: all room editing now
 * goes through the menu, the same way the original SneezyMUD's redit used a
 * CON_REDITING menu mode (misc/create_rooms.cc). Gate: BUILD_MIN_LEVEL,
 * enforced by the command table. */
bool cmd_edit(descriptor_t *d, const char *args) {
    if (!d->character) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    int vnum;
    if (isdigit((unsigned char)args[0])) {
        vnum = atoi(args);
    } else {
        room_t *here = d->character->base.roomp;
        if (!here) {
            descriptor_send(d, "You are nowhere.\r\n");
            return true;
        }
        vnum = here->vnum;
    }

    if (!descriptor_redit_begin(d, vnum)) {
        char msg[96];
        snprintf(msg, sizeof(msg), "There is no room %d to edit.\r\n", vnum);
        descriptor_send(d, msg);
    }
    return true;
}
