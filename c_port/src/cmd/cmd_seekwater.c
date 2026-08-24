/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <string.h>

#include "being.h"
#include "room.h"
#include "room_repo.h"
#include "skill.h"
#include "world.h"

/* `seekwater` (missing-skill audit, generic/cross-class, Tier-3 port
 * 2026-08-16): real upstream (disc_advanced_adventuring.cc's
 * doSeekwater()) launches the tracking pathfinder (TPathFinder /
 * TASK_TRACKING) to find the nearest water ANYWHERE in range and points
 * the seeker step-by-step toward it, distance scaling with skill/level
 * (Elves see twice as far). Tobin has no pathfinder and no tracking-task
 * system, so this scopes down to a local water-sense (disclosed
 * divergence): a proficiency roll reveals whether there's water in the
 * current room or in any directly adjacent room, and which way. It won't
 * trace a multi-room trail the way upstream does -- it senses water you
 * could reach in a step, not water halfway across the zone. */

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

bool cmd_seekwater(descriptor_t *d, const char *args) {
    (void)args;
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }
    bool imm = being_is_immortal(ch);
    if (!imm && !being_knows_skill(ch, "seekwater")) {
        descriptor_send(d, "You do not have the ability to search for water.\r\n");
        return true;
    }
    if (ch->fighting) {
        descriptor_send(d, "The ensuing battle makes it too hard to search for water.\r\n");
        return true;
    }
    if (ch->position < POSITION_STANDING) {
        descriptor_send(d, "You need to be standing to search for water.\r\n");
        return true;
    }

    const skill_def_t *sk = skill_find(ch->char_class, "seekwater", imm);
    int pct = imm ? 100 : (sk ? skill_learn_from_doing(ch, sk) : 0);
    if (!skill_roll_success(pct)) {
        descriptor_send(d, "You search for signs of water, but can't get your bearings.\r\n");
        return true;
    }

    /* Water underfoot trumps any direction. */
    if (sector_is_water(ch->base.roomp->sector)) {
        descriptor_send(d, "Look!  Water!  It was here all along.\r\n");
        return true;
    }

    char found[256];
    found[0] = '\0';
    int count = 0;
    for (int dir = 0; dir < ROOM_NUM_EXITS; dir++) {
        room_t *nb = neighbor_room(ch->base.roomp, dir);
        if (nb && sector_is_water(nb->sector)) {
            if (count)
                strncat(found, ", ", sizeof(found) - strlen(found) - 1);
            strncat(found, DIR_NAMES[dir], sizeof(found) - strlen(found) - 1);
            count++;
        }
    }

    if (count == 0) {
        descriptor_send(d, "You search all around but sense no water nearby.\r\n");
        return true;
    }

    char msg[320];
    snprintf(msg, sizeof(msg), "You sense water close by, to the %s.\r\n", found);
    descriptor_send(d, msg);
    return true;
}
