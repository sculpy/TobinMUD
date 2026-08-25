/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "being.h"
#include "cmd.h"
#include "dragon_route_repo.h"
#include "log.h"
#include "room.h"
#include "room_repo.h"
#include "thing.h"
#include "world.h"

/* `fly` -- dragon-ride long-haul transport (user, 2026-08-25: "implement
 * a dragon ride system to take players from one area to another distant
 * area for a fee"). New Tobin feature, no SneezyMUD precedent (checked
 * sneezymud-master -- no dragon/griffon ride system there); documented as
 * a deliberate deviation, see STATUS.md's decisions table.
 *
 * Deliberately NOT layered on the existing `ride`/cmd_ride.c mount
 * system -- that command mounts any HORSE-race mob for a movement-cost
 * discount while walking (mob_race_is_rideable()), it doesn't teleport,
 * and `ride` was already taken by that real command. This is a distinct
 * mechanic: pay a flat fee at a fixed "dragon roost" room to be flown
 * directly to a distant roost, same shape as cmd_shop.c's SPEC_TICKET_GUY
 * (`buy ticket`) but with a keyed room-to-room route table instead of one
 * hardcoded destination, so more roosts/routes can be added later purely
 * in data (dragon_route, db/tobin/dragon_ride.sql) with no code change.
 *
 * `fly` alone: lists routes departing the current room (empty list if
 * the room isn't a roost at all). `fly <destination>` matches a route's
 * dest_name by prefix (case-insensitive), same abbreviation spirit as
 * the rest of the command table. Charges progress.gold up front; no
 * partial charge or state change on any refusal. */
bool cmd_fly(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    dragon_route_t routes[DRAGON_ROUTE_MAX];
    int count = 0;
    dragon_route_repo_list(ch->base.roomp->vnum, routes, DRAGON_ROUTE_MAX, &count);

    if (count == 0) {
        descriptor_send(d, "There's nothing to fly on from here.\r\n");
        return true;
    }

    if (!*args) {
        char out[1024];
        int n = snprintf(out, sizeof(out), "\r\nA dragon-keeper offers passage to:\r\n");
        for (int i = 0; i < count && (size_t)n < sizeof(out); i++) {
            n += snprintf(out + n, sizeof(out) - (size_t)n, " %-30s %d gold\r\n",
                          routes[i].dest_name, routes[i].fee);
        }
        if ((size_t)n < sizeof(out))
            n += snprintf(out + n, sizeof(out) - (size_t)n, "\r\n(fly <destination>)\r\n");
        descriptor_send(d, out);
        return true;
    }

    dragon_route_t *route = NULL;
    size_t len = strlen(args);
    for (int i = 0; i < count; i++) {
        if (strncasecmp(routes[i].dest_name, args, len) == 0) {
            route = &routes[i];
            break;
        }
    }
    if (!route) {
        descriptor_send(d, "A dragon-keeper tells you, \"I don't fly there.\"\r\n");
        return true;
    }

    if (ch->progress.gold < route->fee) {
        char msg[160];
        snprintf(msg, sizeof(msg),
                 "A dragon-keeper tells you, \"Passage to %s costs %d gold.\"\r\n",
                 route->dest_name, route->fee);
        descriptor_send(d, msg);
        return true;
    }

    room_t *dest = world_get_room(route->to_room);
    if (!dest) {
        dest = room_repo_load(route->to_room);
        if (dest)
            world_register_room(dest);
    }
    if (!dest) {
        descriptor_send(d, "A dragon-keeper tells you, \"That roost seems to have vanished -- try again later.\"\r\n");
        return true;
    }

    ch->progress.gold -= route->fee;

    char ch_name_cap[128];
    being_display_name_cap(ch, ch_name_cap, sizeof(ch_name_cap));
    room_t *old_room = ch->base.roomp;

    char depart_msg[192];
    snprintf(depart_msg, sizeof(depart_msg),
             "%s climbs onto a dragon's back and leaps skyward!\r\n", ch_name_cap);
    descriptor_room_echo(old_room, ch, depart_msg);

    descriptor_send(d, "You climb onto the dragon's back. It leaps skyward, wings beating hard,\r\n"
                        "and the ground falls away beneath you...\r\n");

    thing_set_room(&ch->base, dest);

    char arrive_msg[192];
    snprintf(arrive_msg, sizeof(arrive_msg),
             "%s swoops in on a dragon and dismounts.\r\n", ch_name_cap);
    descriptor_room_echo(dest, ch, arrive_msg);

    descriptor_send(d, "...until the dragon banks and lands, talons gripping the roost.\r\n");
    cmd_dispatch(d, "look");

    game_log(LOG_GAME, "%s flew by dragon to room %d for %d gold.",
             ch->base.name, dest->vnum, route->fee);
    return true;
}
