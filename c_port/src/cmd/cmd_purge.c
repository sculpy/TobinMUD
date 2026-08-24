/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include <stdlib.h>

#include "being.h"
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
 * every linkdead PC in the world (the original's "purge ldead").
 *
 * `purge <target>` (user, 2026-08-02: "add purge target to purge a single
 * target") -- the one form from the original's fuller purge that WAS
 * requested: destroy one named mob or object in the room instead of
 * everything in it. Same "never a player" rule as bare purge -- a THING_PC
 * is never a valid match, same spirit as the original's separate
 * kick-from-game form staying out of scope. Name matching follows the
 * same "N.name" ordinal + prefix-match convention as combat_find_room_
 * target() (combat.c) so "2.rat" reaches the second rat if more than one
 * is present. */
/* Destroys every loose mob/object in room `r` (never a player) --
 * shared by bare `purge` (the current room) and `purge <low>-<high>`
 * (every currently LOADED room in that vnum range) below, so the two
 * forms can never drift on what "purge a room" actually does. */
static int purge_room_contents(room_t *r) {
    int destroyed = 0;
    thing_t *t = r->base.stuff_head;
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
    return destroyed;
}

bool cmd_purge(descriptor_t *d, const char *args) {
    if (!d->character || !d->character->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    while (*args == ' ')
        args++;

    /* `purge <low>-<high>` (user, 2026-08-04: asked for a one-shot way to
     * clean up the ad hoc test-sandbox rooms/mobs left behind by
     * smoke-test runs, e.g. TODO.md's "leftover-mob accumulation" follow-
     * up -- those tests bootstrap sandbox rooms in high vnum ranges like
     * 900000+ and mostly clean up after themselves, but an interrupted
     * run leaves the room (and whatever's loose in it) live forever,
     * getting re-dumped/restored by every future copyover. 59+ (one tier
     * above `purge linkdead`'s 58 -- user, 2026-08-04) since a mistyped
     * range could sweep real zone content, not just test rooms. Only
     * touches rooms already LOADED in memory (world_get_room(), no lazy
     * room_repo_load()) -- an unloaded room by definition has nothing
     * loose in it worth purging, and loading one just to find that out
     * would only add to the very room-bloat problem this exists to fix. */
    {
        const char *dash = strchr(args, '-');
        if (dash && dash != args) {
            char lowbuf[16];
            size_t lowlen = (size_t)(dash - args);
            bool all_digits = lowlen > 0 && lowlen < sizeof(lowbuf);
            for (size_t i = 0; all_digits && i < lowlen; i++)
                if (args[i] < '0' || args[i] > '9')
                    all_digits = false;
            for (const char *p = dash + 1; all_digits && *p; p++)
                if (*p < '0' || *p > '9')
                    all_digits = false;
            if (all_digits && dash[1] != '\0') {
                if (d->character->progress.level < PURGE_RANGE_MIN_LEVEL) {
                    descriptor_send(d, "Command not found, maybe submit an idea if you believe TobinMUD should have it.\r\n");
                    return true;
                }
                snprintf(lowbuf, sizeof(lowbuf), "%.*s", (int)lowlen, args);
                int low = atoi(lowbuf);
                int high = atoi(dash + 1);
                if (low > high) {
                    int tmp = low; low = high; high = tmp;
                }
                int rooms_hit = 0, things_destroyed = 0;
                for (int v = low; v <= high; v++) {
                    room_t *r = world_get_room(v);
                    if (!r)
                        continue;
                    int n = purge_room_contents(r);
                    if (n > 0) {
                        rooms_hit++;
                        things_destroyed += n;
                    }
                }
                char msg[128];
                snprintf(msg, sizeof(msg), "Purged %d thing(s) across %d loaded room(s) in range %d-%d.\r\n",
                         things_destroyed, rooms_hit, low, high);
                descriptor_send(d, msg);
                game_log(LOG_EDIT, "%s purged range %d-%d (%d thing(s), %d room(s)). [%s]",
                         d->character->base.name, low, high, things_destroyed, rooms_hit,
                         descriptor_display_host(d));
                return true;
            }
        }
    }

    if (strcasecmp(args, "linkdead") == 0 || strcasecmp(args, "ldead") == 0) {
        if (d->character->progress.level < PURGE_LINKDEAD_MIN_LEVEL) {
            descriptor_send(d, "Command not found, maybe submit an idea if you believe TobinMUD should have it.\r\n");
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

    room_t *room = d->character->base.roomp;

    if (*args) {
        const char *rest;
        int ordinal = thing_parse_ordinal(args, &rest);
        size_t len = strlen(rest);
        int seen = 0;
        thing_t *target = NULL;
        for (thing_t *t = room->base.stuff_head; t; t = t->stuff_next) {
            if (t->kind != THING_OBJ && t->kind != THING_MOB)
                continue; /* never a player -- same rule as bare purge */
            if (thing_name_matches(t->name, rest, len)) {
                seen++;
                if (seen == ordinal) {
                    target = t;
                    break;
                }
            }
        }
        if (!target) {
            descriptor_send(d, "Nothing here matches that.\r\n");
            return true;
        }

        char label[128];
        snprintf(label, sizeof(label), "%s", target->short_descr[0] ? target->short_descr : target->name);
        char msg[192];
        snprintf(msg, sizeof(msg), "You purge %s.\r\n", label);
        descriptor_send(d, msg);
        game_log(LOG_EDIT, "%s purged %s (%s, vnum %d) in room %d. [%s]",
                 d->character->base.name, label, target->kind == THING_MOB ? "mob" : "obj",
                 target->id, room->vnum, descriptor_display_host(d));

        if (target->kind == THING_OBJ)
            obj_destroy((obj_t *)target);
        else
            being_destroy((being_t *)target);
        return true;
    }

    int destroyed = purge_room_contents(room);

    char msg[96];
    snprintf(msg, sizeof(msg), "The room shudders -- %d thing(s) vanish.\r\n", destroyed);
    descriptor_send(d, msg);
    game_log(LOG_EDIT, "%s purged room %d (%d thing(s)). [%s]",
             d->character->base.name, room->vnum, destroyed, descriptor_display_host(d));
    return true;
}
