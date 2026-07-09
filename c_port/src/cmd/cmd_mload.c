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
#include "mob_repo.h"
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

/* `mload <vnum>` or `mload <name>` -- immortal builder tool (BUILD_MIN_LEVEL,
 * same tier as `edroom`/`goto`/`oload`): instantiates a mob prototype into
 * the caller's current room. A purely-numeric argument is a vnum; anything
 * else is looked up as a case-insensitive substring against the `mob`
 * table's `name` column, taking the lowest-vnum match ("mload demon" loads
 * the first demon). Manual only -- there's no zone-reset system yet (2E,
 * still future) to respawn mobs automatically, so an `mload`ed mob does NOT
 * survive a server restart (same documented gap as room-floor objects). */
bool cmd_mload(descriptor_t *d, const char *args) {
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
        descriptor_send(d, "Usage: mload <vnum|name>\r\n");
        return true;
    }

    int vnum = is_all_digits(trimmed) ? atoi(trimmed) : mob_find_vnum_by_name(trimmed);
    if (vnum < 0) {
        descriptor_send(d, "No mobile matches that.\r\n");
        return true;
    }

    being_t *m = being_create_mob(vnum);
    if (!m) {
        descriptor_send(d, "No such mobile exists.\r\n");
        return true;
    }

    thing_set_room(&m->base, ch->base.roomp);

    char msg[256];
    const char *label = m->base.short_descr[0] ? m->base.short_descr : m->base.name;
    snprintf(msg, sizeof(msg), "You conjure %s into being.\r\n", label);
    descriptor_send(d, msg);
    snprintf(msg, sizeof(msg), "%s conjures %s into being.\r\n", ch->base.name, label);
    descriptor_room_echo(ch->base.roomp, ch, msg);
    return true;
}
