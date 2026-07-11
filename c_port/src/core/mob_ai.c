/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "mob_ai.h"

#include <stdio.h>
#include <stdlib.h>

#include "being.h"
#include "descriptor.h"
#include "obj.h"
#include "pulse.h"
#include "room.h"
#include "room_repo.h"
#include "thing.h"
#include "world.h"

/* Original ACT_* bits actually used here (misc/defs.h in the bundled
 * sneezymud-master reference tree), kept verbatim so already-seeded
 * `mob.actions` values "just work" with no data migration. Only the bits
 * this file reads are named; the rest (ACT_WIMPY, ACT_HATEFUL, ...) are
 * for future AI work (the fuller Mobile_Attitude system, TODO.md). */
#define ACT_SENTINEL   (1 << 1)
#define ACT_SCAVENGER  (1 << 2)
#define ACT_AGGRESSIVE (1 << 5)

#define MOB_WANDER_CHANCE_PCT 20
#define MOB_SCAVENGE_CHANCE_PCT 25
#define MOB_AGGRESS_CHANCE_PCT 25

/* Mobile_Attitude, scoped down (Session 43 continued, user: "class
 * Mobile_Attitude in sneezy should be implemented into tobin. mobs should
 * react to good vs evil and react accordingly"). The original models four
 * emotional attributes per mob (suspicion/greed/malice/anger) that Tobin
 * has no per-mob storage for; this reads the one thing that's actually
 * modeled on the PC side -- progress_t.alignment (being.h) -- and applies
 * the single reaction the user described: an ACT_AGGRESSIVE mob backs off
 * a sufficiently GOOD-aligned target instead of attacking on sight,
 * mirroring the original's aggro()'s karma-vs-mob-disposition check
 * (14-monster-ai-behavior.md) at a much simpler scale. Threshold matches
 * alignment_word()'s "good"/"saintly" tiers. */
#define AGGRESS_GOOD_IMMUNITY_THRESHOLD 350

static void mob_try_wander(being_t *m) {
    if (m->mob_actions & ACT_SENTINEL)
        return;
    if (m->fighting || m->position != POSITION_STANDING || !m->base.roomp)
        return;
    if (rand() % 100 >= MOB_WANDER_CHANCE_PCT)
        return;

    room_t *from = m->base.roomp;
    int dirs[ROOM_NUM_EXITS];
    int n = 0;
    for (int i = 0; i < ROOM_NUM_EXITS; i++) {
        if (from->exits[i] < 0)
            continue;
        if (from->exit_door[i] != 0 && (from->exit_cond[i] & EXIT_COND_CLOSED))
            continue;
        dirs[n++] = i;
    }
    if (!n)
        return;

    int dir = dirs[rand() % n];
    int dest = from->exits[dir];
    room_t *to = world_get_room(dest);
    if (!to) {
        to = room_repo_load(dest);
        if (to)
            world_register_room(to);
    }
    if (!to || (to->room_flag & ROOM_FLAG_NO_MOB))
        return;

    char msg[128];
    snprintf(msg, sizeof(msg), "%s leaves.\r\n", m->base.name);
    descriptor_room_echo(from, NULL, msg);

    thing_set_room(&m->base, to);

    snprintf(msg, sizeof(msg), "%s arrives.\r\n", m->base.name);
    descriptor_room_echo(to, NULL, msg);
}

/* Scoped down from the original's ACT_SCAVENGER (which picks up ANY loose
 * object -- including real loot) to just OBJ_CAT_TRASH specifically,
 * matching the user's "clean up" framing ("i want cleaner mobs to clean
 * up randomly") rather than risking a cleaner mob eating valuable dropped
 * gear or a corpse's contents. */
static void mob_try_scavenge(being_t *m) {
    if (!(m->mob_actions & ACT_SCAVENGER) || !m->base.roomp)
        return;
    if (rand() % 100 >= MOB_SCAVENGE_CHANCE_PCT)
        return;

    obj_t *pick = NULL;
    int count = 0;
    for (thing_t *t = m->base.roomp->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_OBJ)
            continue;
        obj_t *o = (obj_t *)t;
        if (o->category != OBJ_CAT_TRASH)
            continue;
        count++;
        if (rand() % count == 0) /* reservoir sampling: uniform pick over the trash items seen so far */
            pick = o;
    }
    if (!pick)
        return;

    char msg[256]; /* name (64) + short_descr (128) + fixed text (thing.h caps) */
    snprintf(msg, sizeof(msg), "%s picks up %s and cleans it away.\r\n",
             m->base.name, pick->base.short_descr);
    descriptor_room_echo(m->base.roomp, NULL, msg);
    obj_destroy(pick);
}

/* Picks a fight for an ACT_AGGRESSIVE mob against a non-immortal PC in its
 * room, unless that PC's alignment grants immunity (see the threshold's
 * doc comment above). No descriptor to send "You attack ..." from (mobs
 * have none) -- just the fighting-pointer/wait bookkeeping cmd_attack.c
 * does, plus a notification to the target if they're actually connected. */
static void mob_try_aggress(being_t *m) {
    if (!(m->mob_actions & ACT_AGGRESSIVE))
        return;
    if (m->fighting || m->position != POSITION_STANDING || !m->base.roomp)
        return;
    if (rand() % 100 >= MOB_AGGRESS_CHANCE_PCT)
        return;

    being_t *target = NULL;
    for (thing_t *t = m->base.roomp->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_PC)
            continue;
        being_t *pc = (being_t *)t;
        if (!pc->desc || being_is_immortal(pc) || pc->fighting)
            continue;
        if (pc->progress.alignment >= AGGRESS_GOOD_IMMUNITY_THRESHOLD)
            continue; /* good-aligned -- this aggressive mob leaves them alone */
        target = pc;
        break;
    }
    if (!target)
        return;

    m->fighting = target;
    target->fighting = m;
    being_set_wait(m, COMBAT_ROUND_PULSES);

    char msg[128];
    snprintf(msg, sizeof(msg), "%s attacks you!\r\n", m->base.name);
    descriptor_notify(target->desc, msg);
}

static void mob_ai_visit(being_t *m) {
    mob_try_wander(m);
    mob_try_scavenge(m);
    mob_try_aggress(m);
}

void mob_ai_tick(long pulse_num) {
    (void)pulse_num;
    world_for_each_mob(mob_ai_visit);
}
