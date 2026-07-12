/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>

#include "log.h"
#include "obj.h"

/* `pee` (user, 2026-07-11: "add pools and the pee command for 51"). A
 * flavor command, immortal-only (IMMORTAL_LEVEL_MIN) like the other one-off
 * fun/utility commands added this session -- leaves a non-takeable puddle
 * (obj_grow_pool(), obj.c) on the floor of the caller's room. A second `pee`
 * in the same room grows the existing puddle into a bigger pool instead of
 * adding a separate object (user, 2026-07-11: "pools should grow in size if
 * multiple puddles of the same type are created in a room"). */
bool cmd_pee(descriptor_t *d, const char *args) {
    (void)args;
    if (!d->character || !d->character->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    obj_grow_pool(d->character->base.roomp, "pee", "puddle pool pee urine", "pee");

    descriptor_send(d, "You relieve yourself. Ahh, much better.\r\n");
    char msg[128];
    snprintf(msg, sizeof(msg), "%s relieves %s on the floor.\r\n",
             d->character->base.name, "themselves");
    descriptor_room_echo(d->character->base.roomp, d->character, msg);

    game_log(LOG_EDIT, "%s left a puddle in room %d. [%s]",
             d->character->base.name, d->character->base.roomp->vnum,
             descriptor_display_host(d));
    return true;
}
