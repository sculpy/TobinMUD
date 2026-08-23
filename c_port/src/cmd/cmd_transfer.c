/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
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
#include "log.h"
#include "room.h"
#include "room_repo.h"
#include "thing.h"
#include "world.h"

/* `transfer <name>` (user, 2026-07-10: "add a transfer command that will
 * take a target and transfer them into the same room as the transfer
 * command was issued in ... also transfer name vnum to transfer the
 * target to the room that matches vnum"). Mirrors the original's `trans`
 * (see lib/help/_immortal/transfer in the bundled sneezymud-master
 * reference tree: "moves a character into the same room with the
 * wizard"), plus the user's own explicit room-vnum variant. Online PCs
 * only -- unlike `goto`, which can also target a bare vnum, transfer's
 * first argument is always a person; mobs aren't addressable here (no
 * numbered-index mob syntax like the original's "trans 4.chicken"). */
bool cmd_transfer(descriptor_t *d, const char *args) {
    if (!d->character || !d->character->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    char tok[64] = "";
    char vnum_arg[32] = "";
    int got = sscanf(args, "%63s %31s", tok, vnum_arg);
    if (got < 1) {
        descriptor_send(d, "Usage: transfer <name> [room vnum]\r\n");
        return true;
    }

    size_t len = strlen(tok);
    being_t *target = NULL;
    for (descriptor_t *it = g_descriptors; it; it = it->next) {
        if (it->character && it->character->base.roomp
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
    if (target == d->character) {
        descriptor_send(d, "You can't transfer yourself -- try goto.\r\n");
        return true;
    }

    room_t *dest;
    if (got >= 2) {
        if (!isdigit((unsigned char)vnum_arg[0])) {
            descriptor_send(d, "Usage: transfer <name> [room vnum]\r\n");
            return true;
        }
        int vnum = atoi(vnum_arg);
        dest = world_get_room(vnum);
        if (!dest) {
            dest = room_repo_load(vnum);
            if (dest)
                world_register_room(dest);
        }
        if (!dest) {
            char msg[80];
            snprintf(msg, sizeof(msg), "No room with vnum %d exists.\r\n", vnum);
            descriptor_send(d, msg);
            return true;
        }
    } else {
        dest = d->character->base.roomp;
    }

    room_t *old_room = target->base.roomp;
    char depart_msg[128]; /* base.name is up to 64 chars (thing.h) + the fixed message text */
    snprintf(depart_msg, sizeof(depart_msg), "%s disappears in a puff of smoke.\r\n", target->base.name);
    if (old_room)
        descriptor_room_echo(old_room, target, depart_msg);

    thing_set_room(&target->base, dest);

    /* Drag the target's mount and rider along, matching SneezyMUD's
     * doTrans() (`victim->riding`/`victim->rider` chain moved with the
     * transferred target) -- transfer does NOT drag followers, only this
     * mount/rider pair; goto (cmd_goto.c) is the one that drags immortal
     * followers. */
    if (target->mount && target->mount->base.roomp == old_room)
        thing_set_room(&target->mount->base, dest);
    if (target->rider && target->rider->base.roomp == old_room)
        thing_set_room(&target->rider->base, dest);

    char arrive_msg[128];
    snprintf(arrive_msg, sizeof(arrive_msg), "%s arrives in a puff of smoke.\r\n", target->base.name);
    descriptor_room_echo(dest, target, arrive_msg);

    if (target->desc) {
        descriptor_send(target->desc, "You are yanked through space by a will not your own!\r\n");
        cmd_dispatch(target->desc, "look");
    }

    char msg[128];
    snprintf(msg, sizeof(msg), "You transfer %s to room %d.\r\n", target->base.name, dest->vnum);
    descriptor_send(d, msg);
    game_log(LOG_EDIT, "%s transferred %s to room %d. [%s]",
             d->character->base.name, target->base.name, dest->vnum,
             descriptor_display_host(d));
    return true;
}
