/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#include "being.h"
#include "log.h"
#include "obj.h"
#include "room.h"
#include "room_repo.h"
#include "thing.h"
#include "world.h"
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
/* `edit room reclaim <low>-<high>` (user, 2026-08-04: "room reclaim for
 * just rooms" -- splitting `zone reclaim`'s all-at-once room+obj+mob
 * sweep, cmd_zone.c, into a per-noun form under each editor's own `edit
 * <noun>` entry point). Deletes only room/roomexit/roomextra rows in
 * range (room_repo_delete_range()) -- objects and mobs in the same range
 * are untouched, use `edit object reclaim`/`edit mob reclaim` for those.
 * Same 59+ gate and other-player-in-range refusal as `zone reclaim`. */
static bool edroom_reclaim(descriptor_t *d, const char *rangearg) {
    if (d->character->progress.level < EDROOM_RECLAIM_MIN_LEVEL) {
        descriptor_send(d, "Command not found, maybe submit an idea if you believe TobinMUD should have it.\r\n");
        return true;
    }
    int low, high;
    if (sscanf(rangearg, "%d-%d", &low, &high) != 2) {
        descriptor_send(d, "Usage: edit room reclaim <low vnum>-<high vnum>\r\n");
        return true;
    }
    if (low > high) {
        int tmp = low; low = high; high = tmp;
    }

    for (int v = low; v <= high; v++) {
        room_t *r = world_get_room(v);
        if (!r)
            continue;
        for (thing_t *t = r->base.stuff_head; t; t = t->stuff_next) {
            if (t->kind == THING_PC && t != &d->character->base) {
                descriptor_send(d, "A player is currently standing in a room within that range -- refused.\r\n");
                return true;
            }
        }
    }

    int things_purged = 0;
    for (int v = low; v <= high; v++) {
        room_t *r = world_get_room(v);
        if (!r)
            continue;
        thing_t *t = r->base.stuff_head;
        while (t) {
            thing_t *next = t->stuff_next;
            if (t->kind == THING_OBJ) {
                obj_destroy((obj_t *)t);
                things_purged++;
            } else if (t->kind == THING_MOB) {
                being_destroy((being_t *)t);
                things_purged++;
            }
            t = next;
        }
    }

    int rooms_deleted = room_repo_delete_range(low, high);
    char msg[128];
    snprintf(msg, sizeof(msg), "Reclaimed range %d-%d: %d room(s) deleted; %d loose thing(s) purged from live memory.\r\n",
             low, high, rooms_deleted, things_purged);
    descriptor_send(d, msg);
    game_log(LOG_EDIT, "%s reclaimed room range %d-%d (%d room(s)). [%s]",
             d->character->base.name, low, high, rooms_deleted, descriptor_display_host(d));
    return true;
}

bool cmd_edroom(descriptor_t *d, const char *args) {
    if (!d->character) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    if (strncasecmp(args, "reclaim", 7) == 0 && (args[7] == ' ' || args[7] == '\0')) {
        const char *rest = args + 7;
        while (*rest == ' ')
            rest++;
        return edroom_reclaim(d, rest);
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
