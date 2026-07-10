/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "zone.h"

#include <stdio.h>
#include <string.h>

#include "being.h"
#include "log.h"
#include "obj.h"
#include "room.h"
#include "room_repo.h"
#include "thing.h"
#include "world.h"
#include "zone_repo.h"

#define MAX_ZONES 512
#define MAX_RESETS_PER_ZONE 2048

/* In-memory zone age tracking (minutes since last reset) -- not persisted,
 * same precedent as other session-only state (fighting/desc/off_hand_next
 * in being.h): a fresh process always re-runs zone_boot_all() anyway (see
 * zone.h), so there's nothing meaningful to carry across a restart. */
typedef struct {
    int zone_nr;
    int lifespan;
    int age;
} zone_age_t;

static zone_age_t g_zone_ages[MAX_ZONES];
static int g_zone_age_count = 0;

/* Fetches room `vnum`, loading it from the DB and registering it into the
 * world cache if it isn't already resident -- same 3-step pattern already
 * repeated at every other room-lookup call site (cmd_goto.c, cmd_move.c,
 * descriptor.c, ...). */
static room_t *zone_get_room(int vnum) {
    room_t *r = world_get_room(vnum);
    if (!r) {
        r = room_repo_load(vnum);
        if (r)
            world_register_room(r);
    }
    return r;
}

/* Counts how many THING_MOB/THING_OBJ children of `room` already have
 * vnum `vnum` -- backs M/O's per-room load cap (arg2). Deliberately does
 * NOT track a world-wide max_exist cap like the original (that needs a
 * live count of every instance anywhere in the world) -- a documented
 * simplification, see zone.h. */
static int zone_count_in_room(const room_t *room, thing_kind_t kind, int vnum) {
    int n = 0;
    for (thing_t *t = room->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind == kind && t->id == vnum)
            n++;
    }
    return n;
}

/* 'M': load mob `arg1` into room `arg3`, unless the room already has
 * `arg2` or more of that vnum. Sets *last_mob to the new mob (or NULL on
 * any failure -- cascades to a following E/G/if_flag row failing too,
 * same as the original's mob==NULL-on-failure behavior). */
static bool zone_cmd_load_mob(const zone_reset_cmd_t *cmd, being_t **last_mob) {
    *last_mob = NULL;

    room_t *room = zone_get_room(cmd->arg3);
    if (!room)
        return false;

    if (zone_count_in_room(room, THING_MOB, cmd->arg1) >= cmd->arg2)
        return false;

    being_t *mob = being_create_mob(cmd->arg1);
    if (!mob)
        return false;

    thing_set_room(&mob->base, room);
    *last_mob = mob;
    return true;
}

/* 'O': load object `arg1` onto the floor of room `arg3`, unless the room
 * already has `arg2` or more. Boot-time only (matches the original: O is
 * "ground boot", periodic non-boot resets skip it entirely -- otherwise
 * every reset tick would keep adding fresh ground clutter forever, since
 * nothing currently removes a dropped-and-forgotten object). */
static bool zone_cmd_load_obj_ground(const zone_reset_cmd_t *cmd, bool boot_time,
                                      obj_t **last_obj) {
    *last_obj = NULL;
    if (!boot_time)
        return false;

    room_t *room = zone_get_room(cmd->arg3);
    if (!room)
        return false;

    if (zone_count_in_room(room, THING_OBJ, cmd->arg1) >= cmd->arg2)
        return false;

    obj_t *o = obj_create_from_proto(cmd->arg1);
    if (!o)
        return false;

    thing_set_room(&o->base, room);
    *last_obj = o;
    return true;
}

/* 'E': create object `arg1` and equip it onto `mob` -- placement (worn
 * limb vs. held) is derived from the object's OWN wear_flag column via
 * the same wear_slot_for_flag() a player's `wear`/`hold`/`wield` already
 * uses, not `arg3` (the original's raw wearSlotT slot index has no
 * equivalent in Tobin's 13-limb model, so it's deliberately ignored).
 * Falls back to loose-carried (not equipped) if the slot's already taken
 * or the item doesn't fit any Tobin limb, rather than silently discarding
 * it. */
static bool zone_cmd_equip(const zone_reset_cmd_t *cmd, being_t *mob) {
    if (!mob)
        return false;

    obj_t *o = obj_create_from_proto(cmd->arg1);
    if (!o)
        return false;

    int slot = wear_slot_for_flag(o->wear_flag, mob);
    if (slot == WEAR_SLOT_HELD) {
        int held_idx = mob->handed_right ? 0 : 1;
        if (mob->held[held_idx]) held_idx = 1 - held_idx;
        if (!mob->held[held_idx]) {
            thing_move_to(&o->base, &mob->base);
            mob->held[held_idx] = o;
            return true;
        }
    } else if (slot >= 0 && slot < LIMB_COUNT && !mob->equipment[slot]) {
        thing_move_to(&o->base, &mob->base);
        mob->equipment[slot] = o;
        return true;
    }

    /* No room to equip/hold it -- still give it to the mob rather than
     * losing it outright. */
    thing_move_to(&o->base, &mob->base);
    return true;
}

/* 'G': create object `arg1` and put it loose in `mob`'s carried inventory
 * (not equipped). Sets *last_obj so a following 'P' can place something
 * inside it, if it turns out to be a container. */
static bool zone_cmd_give(const zone_reset_cmd_t *cmd, being_t *mob, obj_t **last_obj) {
    if (!mob)
        return false;

    obj_t *o = obj_create_from_proto(cmd->arg1);
    if (!o)
        return false;

    thing_move_to(&o->base, &mob->base);
    *last_obj = o;
    return true;
}

/* 'P': create object `arg1` and place it inside `container` (the last
 * object loaded by a 'G'/'O' command) -- refuses if that object isn't
 * actually a container. */
static bool zone_cmd_place(const zone_reset_cmd_t *cmd, obj_t *container) {
    if (!container || !obj_is_container(container))
        return false;

    obj_t *o = obj_create_from_proto(cmd->arg1);
    if (!o)
        return false;

    thing_move_to(&o->base, &container->base);
    return true;
}

/* 'D': set the door on room `arg1`'s exit `arg2` to open (arg3=0),
 * closed (arg3=1), or closed+locked (arg3=2). Refuses an exit with no
 * door at all, matching the original. */
static bool zone_cmd_door(const zone_reset_cmd_t *cmd) {
    if (cmd->arg2 < 0 || cmd->arg2 >= ROOM_NUM_EXITS)
        return false;

    room_t *room = zone_get_room(cmd->arg1);
    if (!room)
        return false;

    if (room->exits[cmd->arg2] < 0 || room->exit_door[cmd->arg2] == 0)
        return false;

    switch (cmd->arg3) {
        case 0:
            room->exit_cond[cmd->arg2] &= ~(EXIT_COND_CLOSED | EXIT_COND_LOCKED);
            break;
        case 1:
            room->exit_cond[cmd->arg2] |= EXIT_COND_CLOSED;
            room->exit_cond[cmd->arg2] &= ~EXIT_COND_LOCKED;
            break;
        case 2:
            room->exit_cond[cmd->arg2] |= (EXIT_COND_CLOSED | EXIT_COND_LOCKED);
            break;
        default:
            return false;
    }
    return true;
}

/* Runs one zone's reset -- every row in cmd_no order, `if_flag` gating a
 * row on whether the PREVIOUS row fired (a simple linear dependency
 * chain, matching the original's "skip commands dependent on a failed
 * prev command"). Unhandled opcodes (Y/X/Z/A/V/H/F/T/L/K/C/R/I/J) always
 * count as a no-op failure -- silently, not logged per-row (would be
 * thousands of lines at boot); a one-line per-zone summary is logged by
 * the caller instead. */
static void zone_execute(int zone_nr, bool boot_time, int *out_mobs, int *out_objs) {
    static zone_reset_cmd_t cmds[MAX_RESETS_PER_ZONE];
    int n = zone_repo_load_resets(zone_nr, cmds, MAX_RESETS_PER_ZONE);

    being_t *last_mob = NULL;
    obj_t *last_obj = NULL;
    bool last_ok = true;
    int mobs = 0, objs = 0;

    for (int i = 0; i < n; i++) {
        const zone_reset_cmd_t *cmd = &cmds[i];

        if (cmd->if_flag && !last_ok)
            continue;

        bool ok = false;
        switch (cmd->command) {
            case 'M':
                ok = zone_cmd_load_mob(cmd, &last_mob);
                last_obj = NULL;
                if (ok) mobs++;
                break;
            case 'O': {
                obj_t *o = NULL;
                ok = zone_cmd_load_obj_ground(cmd, boot_time, &o);
                if (ok) { last_obj = o; objs++; }
                break;
            }
            case 'E':
                ok = zone_cmd_equip(cmd, last_mob);
                if (ok) objs++;
                break;
            case 'G': {
                obj_t *o = NULL;
                ok = zone_cmd_give(cmd, last_mob, &o);
                if (ok) { last_obj = o; objs++; }
                break;
            }
            case 'P':
                ok = zone_cmd_place(cmd, last_obj);
                if (ok) objs++;
                break;
            case 'D':
                ok = zone_cmd_door(cmd);
                break;
            default:
                ok = false; /* unhandled opcode -- see zone.h */
                break;
        }
        last_ok = ok;
    }

    if (out_mobs) *out_mobs = mobs;
    if (out_objs) *out_objs = objs;
}

void zone_reset_now(int zone_nr, int *out_mobs, int *out_objs) {
    zone_execute(zone_nr, false, out_mobs, out_objs);
}

/* Matches ZONE_ASSIGN_MIN_LEVEL (cmd_internal.h) -- kept as a separate
 * literal here rather than shared via a header both files would need to
 * pull in just for one constant. */
#define ZONE_UNRESTRICTED_LEVEL 55

bool zone_can_edit(const being_t *ch, int zone_nr) {
    if (!ch)
        return false;
    if (zone_nr < 0)
        return true; /* unzoned content -- no ownership boundary applies */
    if (ch->progress.level >= ZONE_UNRESTRICTED_LEVEL)
        return true;
    return zone_repo_is_assigned(zone_nr, ch->player_id);
}

void zone_boot_all(void) {
    static zone_t zones[MAX_ZONES];
    int n = zone_repo_load_all(zones, MAX_ZONES);

    g_zone_age_count = 0;
    int total_mobs = 0, total_objs = 0;

    for (int i = 0; i < n; i++) {
        if (g_zone_age_count < MAX_ZONES) {
            g_zone_ages[g_zone_age_count].zone_nr = zones[i].zone_nr;
            g_zone_ages[g_zone_age_count].lifespan = zones[i].lifespan > 0 ? zones[i].lifespan : 30;
            g_zone_ages[g_zone_age_count].age = 0;
            g_zone_age_count++;
        }

        if (!zones[i].enabled)
            continue;

        int mobs = 0, objs = 0;
        zone_execute(zones[i].zone_nr, true, &mobs, &objs);
        total_mobs += mobs;
        total_objs += objs;
    }

    log_info("Zones: booted %d zones (%d mobs, %d objects loaded).", n, total_mobs, total_objs);
}

/* ~60 seconds/tick (see main.c's pulse_register call) -- ages every zone
 * by one minute per tick, matching the DB's `lifespan` column's own
 * minutes unit. */
void zone_process_run(long pulse_num) {
    (void)pulse_num;

    for (int i = 0; i < g_zone_age_count; i++) {
        g_zone_ages[i].age++;
        if (g_zone_ages[i].age < g_zone_ages[i].lifespan)
            continue;

        g_zone_ages[i].age = 0;
        int mobs = 0, objs = 0;
        zone_execute(g_zone_ages[i].zone_nr, false, &mobs, &objs);
        if (mobs || objs)
            log_info("Zone %d reset: %d mobs, %d objects topped up.",
                      g_zone_ages[i].zone_nr, mobs, objs);
    }
}
