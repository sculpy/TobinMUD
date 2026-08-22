/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <string.h>

#include "being.h"
#include "obj.h"
#include "room.h"
#include "skill.h"
#include "thing.h"

/* `lumberjack` (missing-skill audit, generic/cross-class, 2026-08-10):
 * real upstream SKILL_LOGGING ("lumberjack", task_logging.cc) is a
 * multi-pulse task that fells a tree in a wooded room for raw wood logs.
 * Tobin has no multi-tick task-continuation system and no per-room tree
 * object, so -- same "one command, one roll" scope-cut every other
 * gathering/extraction skill in this port already uses -- this resolves
 * instantly: in a wooded sector, a proficiency-scaled roll harvests one
 * real seeded wood log (obj vnum 75, "a cedar log", a MAT_WOOD commodity),
 * giving Tobin's own `whittle` profession its first in-world material
 * source (whittle previously only consumed logs that had to be loaded or
 * bought). Only works outdoors in genuinely wooded terrain. */

static const int LUMBER_LOG_VNUM = 75; /* cedar log commodity wood */

static bool is_wooded_sector(int sector) {
    const char *name = sector_name(sector);
    return strstr(name, "FOREST") != NULL || strstr(name, "JUNGLE") != NULL
        || strstr(name, "RAINFOREST") != NULL || strstr(name, "DEAD WOODS") != NULL;
}

bool cmd_lumberjack(descriptor_t *d, const char *args) {
    (void)args;
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }
    bool imm = being_is_immortal(ch);
    if (!imm && !being_knows_skill(ch, "lumberjack")) {
        descriptor_send(d, "You don't know the first thing about felling timber.\r\n");
        return true;
    }
    if (ch->fighting) {
        descriptor_send(d, "Not while you're fighting!\r\n");
        return true;
    }
    if (!is_wooded_sector(ch->base.roomp->sector)) {
        descriptor_send(d, "There's nothing here worth cutting for timber.\r\n");
        return true;
    }

    const skill_def_t *sk = skill_find(ch->char_class, "lumberjack", imm);
    int pct = imm ? 100 : (sk ? skill_learn_from_doing(ch, sk) : 0);

    if (!skill_roll_success(pct)) {
        descriptor_send(d, "You hack away at the timber but come up empty-handed.\r\n");
        return true;
    }

    obj_t *log = obj_create_from_proto(LUMBER_LOG_VNUM);
    if (!log) {
        descriptor_send(d, "You can't seem to gather any usable wood here.\r\n");
        return true;
    }
    thing_move_to(&log->base, &ch->base.roomp->base);

    descriptor_send(d, "You work an axe through the timber and fell a usable log.\r\n");
    if (ch->base.roomp) {
        char cap[128], msg[192];
        being_display_name_cap(ch, cap, sizeof(cap));
        snprintf(msg, sizeof(msg), "%s fells timber for a log.\r\n", cap);
        descriptor_room_echo(ch->base.roomp, ch, msg);
    }
    return true;
}
