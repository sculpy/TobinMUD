/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "being.h"
#include "cmd.h"
#include "room.h"
#include "room_repo.h"
#include "thing.h"
#include "world.h"

/* `goto <vnum>` or `goto <player>`: immortal-only teleport. A number goes
 * straight to that room; a name goes to that online being's current room
 * (players now; mobs once they exist). Mirrors the original doGoto's
 * vnum-or-name behavior (cmd/cmd_goto.cc). Room lookup mirrors enter_world()'s
 * lazy load-and-register pattern. */
bool cmd_goto(descriptor_t *d, const char *args) {
    if (!d->character) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }
    if (!*args) {
        descriptor_send(d, "Usage: goto <room vnum | player name>\r\n");
        return true;
    }

    room_t *r = NULL;

    if (isdigit((unsigned char)args[0])) {
        int vnum = atoi(args);
        r = world_get_room(vnum);
        if (!r) {
            r = room_repo_load(vnum);
            if (r)
                world_register_room(r);
        }
        if (!r) {
            char msg[80];
            snprintf(msg, sizeof(msg), "No room with vnum %d exists.\r\n", vnum);
            descriptor_send(d, msg);
            return true;
        }
    } else {
        /* Teleport to an online being by name (case-insensitive prefix). */
        char tok[64];
        sscanf(args, "%63s", tok);
        size_t len = strlen(tok);
        being_t *target = NULL;
        for (descriptor_t *it = g_descriptors; it; it = it->next) {
            if (it->character && it->character != d->character
                && it->character->base.roomp
                && strncasecmp(it->character->base.name, tok, len) == 0) {
                target = it->character;
                break;
            }
        }
        if (!target) {
            char msg[128];
            snprintf(msg, sizeof(msg), "No one named '%s' is in the game.\r\n", tok);
            descriptor_send(d, msg);
            return true;
        }
        r = target->base.roomp;
    }

    thing_set_room(&d->character->base, r);
    descriptor_send(d, "You vanish in a puff of smoke.\r\n");
    return cmd_dispatch(d, "look");
}
