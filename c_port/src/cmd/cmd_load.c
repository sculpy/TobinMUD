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
#include "mob_repo.h"
#include "obj.h"
#include "obj_repo.h"
#include "room.h"
#include "thing.h"

/* `load <mob|obj> <vnum|name>` -- immortal builder tool (BUILD_MIN_LEVEL,
 * same tier as `edroom`/`goto`): instantiates a mob or object prototype into
 * the caller's current room. Replaces the separate `mload`/`oload` commands
 * (user 2026-07-09: one command, category as the first argument) -- same
 * vnum-or-name lookup either way (a purely-numeric second argument is a
 * vnum; anything else is a case-insensitive substring match against the
 * `mob`/`obj` table's `name` column, lowest-vnum match wins). Manual only --
 * there's no zone-reset system executing yet (see TODO.md's Zones item), so
 * a room-floor object or mob placed this way does NOT survive a server
 * restart (only player-carried/worn/held object instances persist). */

static bool is_all_digits(const char *s) {
    if (!*s)
        return false;
    for (; *s; s++)
        if (!isdigit((unsigned char)*s))
            return false;
    return true;
}

static void load_mob(descriptor_t *d, being_t *ch, const char *trimmed) {
    int vnum = is_all_digits(trimmed) ? atoi(trimmed) : mob_find_vnum_by_name(trimmed);
    if (vnum < 0) {
        descriptor_send(d, "No mobile matches that.\r\n");
        return;
    }

    being_t *m = being_create_mob(vnum);
    if (!m) {
        descriptor_send(d, "No such mobile exists.\r\n");
        return;
    }

    thing_set_room(&m->base, ch->base.roomp);

    char msg[256];
    const char *label = m->base.short_descr[0] ? m->base.short_descr : m->base.name;
    snprintf(msg, sizeof(msg), "You conjure %s into being.\r\n", label);
    descriptor_send(d, msg);
    snprintf(msg, sizeof(msg), "%s conjures %s into being.\r\n", ch->base.name, label);
    descriptor_room_echo(ch->base.roomp, ch, msg);
}

static void load_obj(descriptor_t *d, being_t *ch, const char *trimmed) {
    int vnum = is_all_digits(trimmed) ? atoi(trimmed) : obj_find_vnum_by_name(trimmed);
    if (vnum < 0) {
        descriptor_send(d, "No object matches that.\r\n");
        return;
    }

    obj_t *o = obj_create_from_proto(vnum);
    if (!o) {
        descriptor_send(d, "No such object exists.\r\n");
        return;
    }

    thing_move_to(&o->base, &ch->base.roomp->base);

    char msg[256];
    const char *label = o->base.short_descr[0] ? o->base.short_descr : o->base.name;
    snprintf(msg, sizeof(msg), "You conjure %s into being.\r\n", label);
    descriptor_send(d, msg);
    snprintf(msg, sizeof(msg), "%s conjures %s into being.\r\n", ch->base.name, label);
    descriptor_room_echo(ch->base.roomp, ch, msg);
}

bool cmd_load(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    char cat[16] = "";
    int consumed = 0;
    if (sscanf(args, "%15s %n", cat, &consumed) < 1 || !cat[0]) {
        descriptor_send(d, "Usage: load <mob|obj> <vnum|name>\r\n");
        return true;
    }

    char trimmed[128];
    snprintf(trimmed, sizeof(trimmed), "%s", args + consumed);
    size_t tlen = strlen(trimmed);
    while (tlen > 0 && trimmed[tlen - 1] == ' ')
        trimmed[--tlen] = '\0';
    if (!*trimmed) {
        descriptor_send(d, "Usage: load <mob|obj> <vnum|name>\r\n");
        return true;
    }

    /* Category: any prefix of "mobile"/"object" -- covers the bare single
     * letters M/O (matching the zonefile reset opcodes) up through the full
     * words, e.g. "m", "mob", "mobile" all select the mob branch. */
    size_t clen = strlen(cat);
    if (clen <= 6 && strncasecmp("mobile", cat, clen) == 0) {
        load_mob(d, ch, trimmed);
    } else if (clen <= 6 && strncasecmp("object", cat, clen) == 0) {
        load_obj(d, ch, trimmed);
    } else {
        descriptor_send(d, "Usage: load <mob|obj> <vnum|name>\r\n");
    }
    return true;
}
