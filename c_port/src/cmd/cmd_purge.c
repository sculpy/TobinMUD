/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "log.h"
#include "obj.h"
#include "room.h"
#include "thing.h"
#include "world.h"

/* `purge` (user, 2026-07-09/2026-07-10: "add a purge command that is 51+
 * that will purge the contents of a room, add a linkdead argument that a
 * 58+ god can purge the game of all linkdead characters"). Scoped down
 * from the original's full purge (see lib/help/_immortal/purge in the
 * bundled sneezymud-master reference tree, which also covers purging a
 * single character/object and whole zones) to just the two forms
 * requested: bare `purge` clears the room's mobs and objects (never
 * players -- that's the original's separate, unrequested "purge
 * <character>" kick-from-game form), and `purge linkdead` force-removes
 * every linkdead PC in the world (the original's "purge ldead"). */
bool cmd_purge(descriptor_t *d, const char *args) {
    if (!d->character || !d->character->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    while (*args == ' ')
        args++;

    if (strcasecmp(args, "linkdead") == 0 || strcasecmp(args, "ldead") == 0) {
        if (d->character->progress.level < PURGE_LINKDEAD_MIN_LEVEL) {
            descriptor_send(d, "Huh?!\r\n");
            return true;
        }
        int count = world_purge_linkdead();
        char msg[96];
        snprintf(msg, sizeof(msg), "Purged %d linkdead character(s) from the game.\r\n", count);
        descriptor_send(d, msg);
        game_log(LOG_EDIT, "%s purged %d linkdead character(s). [%s]",
                 d->character->base.name, count, descriptor_display_host(d));
        return true;
    }

    if (*args) {
        descriptor_send(d, "Usage: purge (empties this room) or purge linkdead.\r\n");
        return true;
    }

    room_t *room = d->character->base.roomp;
    int destroyed = 0;
    thing_t *t = room->base.stuff_head;
    while (t) {
        thing_t *next = t->stuff_next; /* obj_destroy()/being_destroy() free t -- save next first */
        if (t->kind == THING_OBJ) {
            obj_destroy((obj_t *)t);
            destroyed++;
        } else if (t->kind == THING_MOB) {
            being_destroy((being_t *)t);
            destroyed++;
        }
        t = next;
    }

    char msg[96];
    snprintf(msg, sizeof(msg), "The room shudders -- %d thing(s) vanish.\r\n", destroyed);
    descriptor_send(d, msg);
    game_log(LOG_EDIT, "%s purged room %d (%d thing(s)). [%s]",
             d->character->base.name, room->vnum, destroyed, descriptor_display_host(d));
    return true;
}
