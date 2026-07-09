/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "being.h"
#include "obj.h"
#include "obj_repo.h"
#include "room.h"
#include "thing.h"

static bool is_all_digits(const char *s) {
    if (!*s)
        return false;
    for (; *s; s++)
        if (!isdigit((unsigned char)*s))
            return false;
    return true;
}

/* `oload <vnum>` or `oload <name>` -- immortal builder tool (BUILD_MIN_LEVEL,
 * same tier as `edroom`/`goto`): instantiates an object prototype into the
 * caller's current room. A purely-numeric argument is a vnum; anything else
 * is looked up as a case-insensitive substring against the `obj` table's
 * `name` column, taking the lowest-vnum match ("oload sword" loads the
 * first sword). Manual only -- there's no zone-reset system yet (a still-
 * future item, see TODO.md) to respawn objects automatically, so a
 * room-floor object placed this way does NOT survive a server restart
 * (only player-carried/worn/held instances persist, see obj_repo.h). */
bool cmd_oload(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    while (*args == ' ')
        args++;
    char trimmed[128];
    snprintf(trimmed, sizeof(trimmed), "%s", args);
    size_t tlen = strlen(trimmed);
    while (tlen > 0 && trimmed[tlen - 1] == ' ')
        trimmed[--tlen] = '\0';
    if (!*trimmed) {
        descriptor_send(d, "Usage: oload <vnum|name>\r\n");
        return true;
    }

    int vnum = is_all_digits(trimmed) ? atoi(trimmed) : obj_find_vnum_by_name(trimmed);
    if (vnum < 0) {
        descriptor_send(d, "No object matches that.\r\n");
        return true;
    }

    obj_t *o = obj_create_from_proto(vnum);
    if (!o) {
        descriptor_send(d, "No such object exists.\r\n");
        return true;
    }

    thing_move_to(&o->base, &ch->base.roomp->base);

    char msg[256];
    const char *label = o->base.short_descr[0] ? o->base.short_descr : o->base.name;
    snprintf(msg, sizeof(msg), "You conjure %s into being.\r\n", label);
    descriptor_send(d, msg);
    snprintf(msg, sizeof(msg), "%s conjures %s into being.\r\n", ch->base.name, label);
    descriptor_room_echo(ch->base.roomp, ch, msg);
    return true;
}
