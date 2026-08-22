/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "affect.h"
#include "being.h"
#include "obj.h"
#include "room.h"
#include "room_repo.h"
#include "skill.h"
#include "thing.h"
#include "world.h"

/* `fish` (Fishing + Fishlore, missing-skill audit, generic/cross-class,
 * Tier-3 port 2026-08-16): real upstream (task/task_fishing.cc) is a
 * multi-pulse TASK_FISHING that needs a wielded pole and baited hook,
 * fishes a water-adjacent room, tracks a per-room "fished" depletion
 * count, rolls weighted fish species/weights, and even keeps a
 * `fishlargest` leaderboard; SKILL_FISHLORE passively sweetens the catch
 * (bigger/rarer fish, a chance to identify them). Tobin has no multi-tick
 * task-continuation system, no per-room fished counter, and no pole/bait
 * objects, so -- same "one command, one roll" scope-cut every other
 * gathering skill in this port uses (see forage/lumberjack) -- this
 * resolves instantly: standing beside (or in) water, a proficiency-scaled
 * `fishing` roll lands a fish (an ephemeral OBJ_CAT_FOOD of random
 * weight). `fishlore`, if known, gives a chance at a heftier "prize"
 * catch and names the fish for you. A per-being cooldown
 * (AFFECT_FISH_COOLDOWN) stands in for upstream's per-room depletion so
 * `fish` isn't a free-food loop. */

/* Common food-fish names, picked at random for flavor. */
static const char *const FISH_KINDS[] = {
    "trout", "bass", "perch", "carp", "pike", "catfish", "salmon", "eel",
};
#define FISH_KIND_COUNT ((int)(sizeof(FISH_KINDS) / sizeof(FISH_KINDS[0])))

/* Resolves the room a given exit leads to, loading it from the repo if it
 * isn't in the live world yet -- same shape as cmd_move.c's own neighbor
 * resolution. Returns NULL for no-exit / unresolvable. */
static room_t *neighbor_room(const room_t *from, int dir) {
    int dest = from->exits[dir];
    if (dest < 0)
        return NULL;
    room_t *to = world_get_room(dest);
    if (!to) {
        to = room_repo_load(dest);
        if (to)
            world_register_room(to);
    }
    return to;
}

/* True if `ch` is standing in, or directly beside, fishable water. */
static bool near_water(const room_t *here) {
    if (sector_is_water(here->sector))
        return true;
    for (int dir = 0; dir < ROOM_NUM_EXITS; dir++) {
        room_t *nb = neighbor_room(here, dir);
        if (nb && sector_is_water(nb->sector))
            return true;
    }
    return false;
}

bool cmd_fish(descriptor_t *d, const char *args) {
    (void)args;
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }
    bool imm = being_is_immortal(ch);
    if (!imm && !being_knows_skill(ch, "fishing")) {
        descriptor_send(d, "You don't know the first thing about fishing.\r\n");
        return true;
    }
    if (ch->fighting) {
        descriptor_send(d, "Not while you're fighting!\r\n");
        return true;
    }
    if (ch->position < POSITION_STANDING) {
        descriptor_send(d, "You need to be on your feet to fish.\r\n");
        return true;
    }
    if (!near_water(ch->base.roomp)) {
        descriptor_send(d, "There's no water here to fish.\r\n");
        return true;
    }
    if (being_has_affect(ch, AFFECT_FISH_COOLDOWN)) {
        descriptor_send(d, "You've fished this spot out for now -- give it a while.\r\n");
        return true;
    }

    being_apply_affect(ch, AFFECT_FISH_COOLDOWN, FISH_COOLDOWN_ROUNDS);

    const skill_def_t *sk = skill_find(ch->char_class, "fishing", imm);
    int pct = imm ? 100 : (sk ? skill_learn_from_doing(ch, sk) : 0);

    if (!skill_roll_success(pct)) {
        descriptor_send(d, "You cast your line and wait, but nothing's biting.\r\n");
        return true;
    }

    /* `fishlore` sweetens the catch: a chance at a hefty "prize" fish,
     * and it names the species for you (otherwise it's "a fish"). */
    bool lore = imm || being_knows_skill(ch, "fishlore");
    bool prize = false;
    if (lore) {
        const skill_def_t *lsk = skill_find(ch->char_class, "fishlore", imm);
        int lpct = imm ? 100 : (lsk ? skill_learn_from_doing(ch, lsk) : 0);
        prize = skill_roll_success(lpct) && (rand() % 100) < 30;
    }

    const char *kind = FISH_KINDS[rand() % FISH_KIND_COUNT];
    char name[64], sdesc[96], ldesc[128];
    if (lore) {
        snprintf(name, sizeof(name), "fish %s", kind);
        if (prize) {
            snprintf(sdesc, sizeof(sdesc), "a prize %s", kind);
            snprintf(ldesc, sizeof(ldesc), "A prize %s lies here, still flopping.", kind);
        } else {
            snprintf(sdesc, sizeof(sdesc), "a %s", kind);
            snprintf(ldesc, sizeof(ldesc), "A %s lies here.", kind);
        }
    } else {
        snprintf(name, sizeof(name), "fish");
        snprintf(sdesc, sizeof(sdesc), "a fish");
        snprintf(ldesc, sizeof(ldesc), "A fish lies here.");
    }

    obj_t *fish = obj_create_ephemeral(name, sdesc, ldesc, OBJ_CAT_FOOD);
    if (!fish)
        return true;
    /* Bigger prize catches feed a bit more (max hunger units). */
    int units = prize ? 8 : 4;
    fish->val[0] = units;
    fish->val[1] = units;
    thing_move_to(&fish->base, &ch->base.roomp->base);

    if (prize)
        descriptor_send(d, "You fight the line and land a real prize -- a fat, flopping catch!\r\n");
    else
        descriptor_send(d, "You cast your line and pull in a fish.\r\n");

    char cap[128], msg[192];
    being_display_name_cap(ch, cap, sizeof(cap));
    snprintf(msg, sizeof(msg), "%s casts a line and lands a fish.\r\n", cap);
    descriptor_room_echo(ch->base.roomp, ch, msg);
    return true;
}
