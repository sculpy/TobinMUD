/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
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
#include "world.h"

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

/* Live world-wide instance count for a mob/obj vnum -- world_for_each_mob()/
 * world_for_each_obj() plus a static-global target+counter, same "counting
 * visitor" idiom cmd_goto.c's goto_is_guildmaster_room() already uses for a
 * different search. Only `load` needs a world-wide count (everywhere else
 * that cares, like zone.c's per-room reset cap, only ever counts ONE room),
 * so this stays local rather than becoming new world.h API surface. */
static int g_count_target_vnum;
static int g_count_result;

/* world_for_each_mob() visitor: tallies mobs matching g_count_target_vnum
 * into g_count_result. */
static void count_mob_visit(being_t *m) {
    if (m->base.id == g_count_target_vnum)
        g_count_result++;
}

/* world_for_each_obj() visitor: tallies objects matching g_count_target_vnum
 * into g_count_result. */
static void count_obj_visit(obj_t *o) {
    if (o->vnum == g_count_target_vnum)
        g_count_result++;
}

/* Warns (never blocks) when loading `vnum` pushes its world-wide instance
 * count over the prototype's own `max_exist` (0 = uncapped, no warning) --
 * user 2026-07-18: "when a immortal loads an obj or mob... max exist
 * should be bypassed with a warning to clean up after the immort is done
 * with the mob or obj if he goes over max exist". An immortal manually
 * spawning something is deliberately exempt from the cap that a zone
 * reset's own per-room check (zone.c's zone_count_in_room()) enforces --
 * this is a "please remember to clean up" nudge, not a refusal. */
static void warn_if_over_max_exist(descriptor_t *d, int vnum, int max_exist, bool is_mob) {
    if (max_exist <= 0)
        return;
    g_count_target_vnum = vnum;
    g_count_result = 0;
    if (is_mob)
        world_for_each_mob(count_mob_visit);
    else
        world_for_each_obj(count_obj_visit);
    if (g_count_result <= max_exist)
        return;
    char msg[160];
    snprintf(msg, sizeof(msg),
             "Warning: %d of vnum %d now exist in the world (its own limit is %d) -- "
             "please clean up when you're done with it.\r\n",
             g_count_result, vnum, max_exist);
    descriptor_send(d, msg);
}

/* `load mob <vnum|name>`: resolves the mob prototype, spawns an instance
 * into `ch`'s current room, announces it, and nudges the immortal if this
 * pushes the world-wide count over the prototype's max_exist. */
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

    mob_proto_t proto;
    if (mob_proto_load(vnum, &proto))
        warn_if_over_max_exist(d, vnum, proto.max_exist, true);
}

/* `load obj <vnum|name>`: resolves the object prototype and spawns an
 * instance straight into `ch`'s own inventory (see the 2026-07-22 note
 * below on why not the room floor), then the same max_exist warning as
 * load_mob() above. */
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

    /* Straight into the loading immortal's own inventory (user
     * 2026-07-22: "when an immort loads an obj let it go into inventory
     * rather than the room") -- previously landed on the room floor,
     * same as a zone-reset load, which meant an immortal testing/
     * building had to `get` it themselves as a second step every time. */
    thing_move_to(&o->base, &ch->base);

    char msg[256];
    const char *label = o->base.short_descr[0] ? o->base.short_descr : o->base.name;
    snprintf(msg, sizeof(msg), "You conjure %s into being.\r\n", label);
    descriptor_send(d, msg);
    snprintf(msg, sizeof(msg), "%s conjures %s into being.\r\n", ch->base.name, label);
    descriptor_room_echo(ch->base.roomp, ch, msg);

    if (ch->base.kind == THING_PC)
        player_inventory_save(ch->player_id, ch);

    obj_proto_t proto;
    if (obj_proto_load(vnum, &proto))
        warn_if_over_max_exist(d, vnum, proto.max_exist, false);
}

/* The `load` command: parses "<mob|obj> <vnum|name>" and dispatches to
 * load_mob()/load_obj() -- see the file's top comment for the merged-
 * command history and vnum-or-name lookup rule. */
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
