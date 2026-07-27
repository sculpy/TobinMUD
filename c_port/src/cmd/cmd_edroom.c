/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include "room.h"
#include "room_repo.h"
#include "zone.h"

/* `edit room [<vnum>]` (formerly the standalone `edroom`, folded into the
 * unified `edit <noun> ...` dispatcher per user request, 2026-07-11 --
 * see cmd_edit.c) -- opens the menu-driven room builder. With a leading
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
 * enforced by the command table (via `edit`'s own gate, since room/zone
 * share BUILD_MIN_LEVEL with the dispatcher itself). */
bool cmd_edroom(descriptor_t *d, const char *args) {
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

    /* Zone identity (Session 43): a builder (51-54) can only edit a room
     * in a zone they're assigned to (zoneassign, 55+ only); 55+ edits
     * anything. Unzoned rooms (no `room.zone`) are unrestricted. */
    if (!zone_can_edit(d->character, room_repo_get_zone(vnum))) {
        descriptor_send(d, "You aren't assigned to that zone.\r\n");
        return true;
    }

    if (!descriptor_redit_begin(d, vnum)) {
        char msg[96];
        snprintf(msg, sizeof(msg), "There is no room %d to edit.\r\n", vnum);
        descriptor_send(d, msg);
    }
    return true;
}
