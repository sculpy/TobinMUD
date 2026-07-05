/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include "player_repo.h"
#include "room.h"
#include "room_repo.h"
#include "world.h"

/* `loadroom [vnum]`: an immortal sets their own load room -- where their
 * character enters the world at login (`player.load_room`, read by
 * enter_world() in descriptor.c). Bare `loadroom` shows the current
 * setting. The target room must exist. New-for-Tobin convenience
 * (user request, Session 21) -- immortals working on a distant zone
 * shouldn't have to goto their way back after every login. */
bool cmd_loadroom(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch)
        return true;

    char msg[160];
    if (!*args) {
        int cur = player_load_room(ch->base.name, d->account.account_id);
        snprintf(msg, sizeof(msg), "You currently load into room %d. "
                 "Usage: loadroom <vnum>\r\n", cur);
        descriptor_send(d, msg);
        return true;
    }
    if (!isdigit((unsigned char)args[0])) {
        descriptor_send(d, "Usage: loadroom <room vnum>\r\n");
        return true;
    }

    int vnum = atoi(args);
    room_t *r = world_get_room(vnum);
    if (!r) {
        r = room_repo_load(vnum);
        if (r)
            world_register_room(r);
    }
    if (!r) {
        snprintf(msg, sizeof(msg), "No room with vnum %d exists.\r\n", vnum);
        descriptor_send(d, msg);
        return true;
    }

    if (!player_set_load_room(ch->base.name, d->account.account_id, vnum)) {
        descriptor_send(d, "The DB rejected the change.\r\n");
        return true;
    }
    snprintf(msg, sizeof(msg), "You will now enter the game in room %d (%s).\r\n",
             vnum, r->base.name);
    descriptor_send(d, msg);
    return true;
}
