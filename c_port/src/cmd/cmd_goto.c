#include "cmd_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include "cmd.h"
#include "room.h"
#include "room_repo.h"
#include "thing.h"
#include "world.h"

/* `goto <vnum>`: immortal-only teleport straight to a room by vnum --
 * trimmed from the original's doGoto (cmd/cmd_goto.cc), which also accepts
 * player/mob names and honors various no-teleport room flags; vnum-only is
 * enough for world-building, the driving use case here (Phase 2A). Room
 * lookup mirrors enter_world()'s lazy load-and-register pattern. */
bool cmd_goto(descriptor_t *d, const char *args) {
    if (!d->character) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }
    if (!*args || !isdigit((unsigned char)args[0])) {
        descriptor_send(d, "Usage: goto <room vnum>\r\n");
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
        char msg[80];
        snprintf(msg, sizeof(msg), "No room with vnum %d exists.\r\n", vnum);
        descriptor_send(d, msg);
        return true;
    }

    thing_set_room(&d->character->base, r);
    descriptor_send(d, "You vanish in a puff of smoke.\r\n");
    return cmd_dispatch(d, "look");
}
