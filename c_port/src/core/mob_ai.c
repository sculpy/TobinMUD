/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "mob_ai.h"

#include <ctype.h>
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
/* Symmetric "evil" tier, same magnitude as the good threshold above --
 * used by the mob-alignment extension below (mob_align != 0). */
#define AGGRESS_EVIL_THRESHOLD (-350)

/* Extension (user 2026-07-11: "ask player to choose initial alignment so
 * good will attack evil and evil will attack good randomly ... people who
 * are neutral should be taunted by evil and supported by good"): an
 * ALIGNED aggressive mob (mob.align, new column -- see mob_repo.h) fights
 * only the OPPOSITE alignment (never its own), same random chance as any
 * other aggro roll (MOB_AGGRESS_CHANCE_PCT, mob_try_aggress() below). A
 * neutral PC never gets attacked by an aligned mob -- instead, at a
 * smaller ambient chance, they get a one-line in-character reaction (a
 * taunt from evil, a word of support from good) with no combat
 * consequence (mob_try_align_flavor() below). Unaligned mobs
 * (mob_align == 0, the vast majority -- every mob before this feature)
 * are completely unaffected: still the original "attack anyone except the
 * sufficiently good" behavior. */
#define MOB_ALIGN_FLAVOR_CHANCE_PCT 15

/* short_descr is stored lowercase-first ("a lady"); capitalize only when it
 * starts a whole message, skipping any leading inline color tag first --
 * same duplicated-helper precedent as cmd_object.c's/cmd_look.c's own
 * cap_first() copies (each file keeps its own rather than sharing one). */
static const char *cap_first(const char *label, char *buf, size_t bufsz) {
    snprintf(buf, bufsz, "%s", label);
    size_t i = 0;
    while (buf[i] == '<' && buf[i + 1] != '\0' && buf[i + 2] == '>')
        i += 3;
    if (buf[i])
        buf[i] = (char)toupper((unsigned char)buf[i]);
    return buf;
}

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

    /* Bug (user, 2026-07-11: "lady stroll walk leaves. is not correct"):
     * this used to print m->base.name directly -- for a mob, that's the
     * space-separated KEYWORD list (e.g. "lady stroll walk" for a mob you
     * can `look lady`/`look stroll`/`look walk` at), not a display name,
     * producing exactly the garbled "lady stroll walk leaves." the user
     * saw. Fixed to use short_descr (capitalized) plus the actual
     * direction, matching do_move()'s player-facing "exits to the <dir>"
     * phrasing (cmd_move.c). */
    char capbuf[128];
    char msg[256];
    snprintf(msg, sizeof(msg), "%s walks to the %s.\r\n",
             cap_first(m->base.short_descr, capbuf, sizeof(capbuf)), DIR_NAMES[dir]);
    descriptor_room_echo(from, NULL, msg);

    thing_set_room(&m->base, to);

    snprintf(msg, sizeof(msg), "%s walks in from the %s.\r\n",
             cap_first(m->base.short_descr, capbuf, sizeof(capbuf)), DIR_NAMES[REV_DIR[dir]]);
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

    /* Same "raw keyword list, not a display name" bug class as
     * mob_try_wander() above -- use the capitalized short_descr instead
     * of m->base.name. */
    char capbuf[128];
    char msg[256]; /* short_descr (128) x2 + fixed text (thing.h caps) */
    snprintf(msg, sizeof(msg), "%s picks up %s and cleans it away.\r\n",
             cap_first(m->base.short_descr, capbuf, sizeof(capbuf)), pick->base.short_descr);
    descriptor_room_echo(m->base.roomp, NULL, msg);
    obj_destroy(pick);
}

/* Picks a fight for an ACT_AGGRESSIVE mob against a non-immortal PC in its
 * room. An UNALIGNED mob (mob_align == 0, the original/default behavior)
 * attacks anyone except a sufficiently good-aligned PC. An ALIGNED mob
 * (mob_align != 0) only ever fights the OPPOSITE alignment -- a good mob
 * never attacks another good PC, an evil mob never attacks another evil
 * PC -- and leaves neutral PCs to mob_try_align_flavor() below instead of
 * fighting them. No descriptor to send "You attack ..." from (mobs have
 * none) -- just the fighting-pointer/wait bookkeeping cmd_attack.c does,
 * plus a notification to the target if they're actually connected. */
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

        if (m->mob_align == 0) {
            if (pc->progress.alignment >= AGGRESS_GOOD_IMMUNITY_THRESHOLD)
                continue; /* good-aligned -- this aggressive mob leaves them alone */
        } else if (m->mob_align > 0) {
            if (pc->progress.alignment > AGGRESS_EVIL_THRESHOLD)
                continue; /* not evil enough -- a good-aligned mob only fights evil */
        } else {
            if (pc->progress.alignment < AGGRESS_GOOD_IMMUNITY_THRESHOLD)
                continue; /* not good enough -- an evil-aligned mob only fights good */
        }
        target = pc;
        break;
    }
    if (!target)
        return;

    m->fighting = target;
    target->fighting = m;
    being_set_wait(m, COMBAT_ROUND_PULSES);

    /* Same "raw keyword list, not a display name" bug class as
     * mob_try_wander() above -- use the capitalized short_descr instead
     * of m->base.name. */
    char capbuf[128];
    char msg[192];
    snprintf(msg, sizeof(msg), "%s attacks you!\r\n",
             cap_first(m->base.short_descr, capbuf, sizeof(capbuf)));
    descriptor_notify(target->desc, msg);
}

/* Ambient, non-combat reaction to a NEUTRAL PC sharing the room with an
 * ALIGNED aggressive mob (user: "people who are neutral should be taunted
 * by evil and supported by good") -- never fires for unaligned mobs
 * (mob_align == 0, untouched by this feature) or for good/evil PCs (those
 * get real combat via mob_try_aggress() instead, never flavor text). */
static void mob_try_align_flavor(being_t *m) {
    if (m->mob_align == 0 || !(m->mob_actions & ACT_AGGRESSIVE))
        return;
    if (m->fighting || !m->base.roomp)
        return;
    if (rand() % 100 >= MOB_ALIGN_FLAVOR_CHANCE_PCT)
        return;

    being_t *target = NULL;
    for (thing_t *t = m->base.roomp->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_PC)
            continue;
        being_t *pc = (being_t *)t;
        if (!pc->desc || being_is_immortal(pc))
            continue;
        if (pc->progress.alignment >= AGGRESS_GOOD_IMMUNITY_THRESHOLD
            || pc->progress.alignment <= AGGRESS_EVIL_THRESHOLD)
            continue; /* only a NEUTRAL PC gets taunted/supported */
        target = pc;
        break;
    }
    if (!target)
        return;

    char capbuf[128];
    char msg[256];
    if (m->mob_align > 0)
        snprintf(msg, sizeof(msg), "%s nods approvingly in your direction.\r\n",
                 cap_first(m->base.short_descr, capbuf, sizeof(capbuf)));
    else
        snprintf(msg, sizeof(msg), "%s sneers and mutters something insulting about you.\r\n",
                 cap_first(m->base.short_descr, capbuf, sizeof(capbuf)));
    descriptor_notify(target->desc, msg);
}

static void mob_ai_visit(being_t *m) {
    mob_try_wander(m);
    mob_try_scavenge(m);
    mob_try_aggress(m);
    mob_try_align_flavor(m);
}

void mob_ai_tick(long pulse_num) {
    (void)pulse_num;
    world_for_each_mob(mob_ai_visit);
}
