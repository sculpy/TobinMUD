/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "mob_ai.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "affect.h"
#include "being.h"
#include "descriptor.h"
#include "gametime.h"
#include "obj.h"
#include "obj_repo.h"
#include "player_repo.h"
#include "pulse.h"
#include "room.h"
#include "room_repo.h"
#include "suit_repo.h"
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

/* Full ACT_* bit names (misc/defs.h, bits 0-23), for `mob_action_names()`
 * below -- NOT just the 3 this file actually reads above. */
static const char *const ACT_NAMES[24] = {
    "STRINGS_CHANGED", "SENTINEL", "SCAVENGER", "DISGUISED", "NICE_THIEF",
    "AGGRESSIVE", "STAY_ZONE", "WIMPY", "ANNOYING", "HATEFUL", "AFRAID",
    "IMMORTAL", "HUNTING", "DEADLY", "POLYSELF", "GUARDIAN", "SKELETON",
    "ZOMBIE", "GHOST", "DIURNAL", "NOCTURNAL", "PROTECTOR", "PROTECTEE",
    "HIT_BY_PK",
};

/* Formats every set ACT_* bit in `flags` as a "[ NAME ]"-joined string
 * into `buf` (ACT_NAMES[] above), or "none" if nothing's set -- used by
 * `stat`/immortal debug output to show a mob's raw actions bitmask in
 * readable form; covers the full upstream bit range even though this
 * file itself only reads three of them (see ACT_NAMES's own comment). */
const char *mob_action_names(int flags, char *buf, size_t size) {
    size_t n = 0;
    buf[0] = '\0';
    for (int bit = 0; bit < 24; bit++) {
        if (!(flags & (1 << bit)))
            continue;
        n += (size_t)snprintf(buf + n, size > n ? size - n : 0, "%s[ %s ]",
                              n > 0 ? " " : "", ACT_NAMES[bit]);
        if (n >= size)
            break;
    }
    if (buf[0] == '\0')
        snprintf(buf, size, "none");
    return buf;
}

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

/* Random chance per AI tick for an idle, non-charmed, non-ACT_SENTINEL
 * mob to wander one room over -- picks a uniformly random open exit
 * (skipping closed doors and ROOM_FLAG_NO_MOB destinations), announces
 * the departure/arrival to both rooms, and moves it. No-ops for a
 * fighting/non-standing mob, one with no room, or a charmed pet (see
 * the comment inside for why pets never wander on their own).
 *
 * `force` (user, 2026-08-08, egotrip's `wander` subcommand -- "forces a
 * wander pulse for all mobs in the room") skips the per-tick RNG gate
 * below but keeps every other legitimate gate (charm/sentinel/fighting/
 * position/no valid exit) -- an immortal forcing a wander still can't
 * make a charmed pet abandon its master or a sentinel budge. */
static void mob_try_wander(being_t *m, bool force) {
    /* Pet/charm (Sneezy → Tobin feature audit): a charmed pet only ever
     * moves by following its master room-to-room (cmd_move.c's
     * charmed-pet drag-along), never on its own -- a random ACT_SENTINEL-
     * less wander would otherwise let a freshly-summoned pet (mob_actions
     * defaults to 0, so no ACT_SENTINEL bit) drift away from its master
     * within a tick or two. */
    if (being_has_affect(m, AFFECT_CHARMED))
        return;
    if (m->mob_actions & ACT_SENTINEL)
        return;
    if (m->fighting || m->position != POSITION_STANDING || !m->base.roomp)
        return;
    if (!force && rand() % 100 >= MOB_WANDER_CHANCE_PCT)
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

#define MOB_SURPLUS_COLLECT_CHANCE_PCT 25
#define MOB_SURPLUS_DELIVER_CHANCE_PCT 15
#define SURPLUS_ROOM_VNUM 563 /* real seeded room, "Surplus": "items you would
                                * like to give to charity... take it and use it" */

/* Case-insensitive substring check against a mob's raw space-separated
 * keyword list (base.name) -- same spirit as cmd_object.c's
 * obj_name_matches(), just a plain substring rather than a per-word
 * prefix match (a mob's own name is short and fixed, unlike a player's
 * free-typed search token, so there's no abbreviation to support here). */
static bool name_has_keyword(const char *name, const char *keyword) {
    size_t klen = strlen(keyword);
    for (const char *p = name; *p; p++)
        if (strncasecmp(p, keyword, klen) == 0)
            return true;
    return false;
}

/* Identifies a "surplus collector" mob by its raw keywords ("sweeper",
 * "hauler", or "collector"+"trash" together) rather than a dedicated
 * flag -- gates the surplus-collect/deliver AI below to just these
 * specially-named mobs. */
static bool is_surplus_collector(const being_t *m) {
    return name_has_keyword(m->base.name, "sweeper")
        || name_has_keyword(m->base.name, "hauler")
        || (name_has_keyword(m->base.name, "collector") && name_has_keyword(m->base.name, "trash"));
}

/* True if `o` is loose in `m`'s inventory -- not currently worn/held --
 * same filter cmd_object.c's is_loose() applies for a player, duplicated
 * here rather than shared (that one's static to cmd_object.c). Matters
 * so a collector mob with actual worn gear of its own never picks THAT
 * up as "collected" and carts it off to Surplus. */
static bool mob_carrying_loose(const being_t *m, const obj_t *o) {
    if (m->held[0] == o || m->held[1] == o)
        return false;
    for (int i = 0; i < LIMB_COUNT; i++)
        if (m->equipment[i] == o)
            return false;
    return true;
}

/* User 2026-07-26: "let the street sweepers and dirty refuse haulers
 * [and trash collectors] get items casually from the room and take them
 * and drop them off in surplus (room 563)." A deliberately DIFFERENT
 * mechanic from mob_try_scavenge()'s ACT_SCAVENGER/OBJ_CAT_TRASH-destroy
 * above -- this one relocates any loose TAKEABLE item (not just trash)
 * to the real seeded Surplus donation room instead of destroying
 * anything, and is keyed on the mob's own real-world archetype by name
 * keyword rather than the ACT_SCAVENGER flag, so a mob could have both
 * behaviors at once and they simply coexist (actual junk gets swept
 * away by one, anything usable gets donated by the other). No real
 * pathfinding exists (same scope-down SPEC_PROC_LAMPLIGHTER's own doc
 * comment already explains for a different mob) -- picks up whatever's
 * in its CURRENT room, then teleports to 563 to drop off and back,
 * rather than actually walking the distance. */
static void mob_try_surplus_collect(being_t *m) {
    if (!m->base.roomp || m->base.roomp->vnum == SURPLUS_ROOM_VNUM)
        return;
    if (!is_surplus_collector(m))
        return;

    bool carrying_any = false;
    for (thing_t *t = m->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind == THING_OBJ && mob_carrying_loose(m, (obj_t *)t)) {
            carrying_any = true;
            break;
        }
    }

    char capbuf[128], msg[256];

    /* Already carrying something collected -- occasionally make the
     * delivery run instead of picking up more. */
    if (carrying_any && rand() % 100 < MOB_SURPLUS_DELIVER_CHANCE_PCT) {
        room_t *from = m->base.roomp;
        room_t *surplus = world_get_room(SURPLUS_ROOM_VNUM);
        if (!surplus) {
            surplus = room_repo_load(SURPLUS_ROOM_VNUM);
            if (surplus)
                world_register_room(surplus);
        }
        if (!surplus)
            return;

        snprintf(msg, sizeof(msg), "%s hurries off with an armful of odds and ends.\r\n",
                 cap_first(m->base.short_descr, capbuf, sizeof(capbuf)));
        descriptor_room_echo(from, NULL, msg);

        thing_set_room(&m->base, surplus);

        int dropped = 0;
        thing_t *t = m->base.stuff_head;
        while (t) {
            thing_t *next = t->stuff_next; /* thing_move_to() relinks t out of m's chain */
            if (t->kind == THING_OBJ && mob_carrying_loose(m, (obj_t *)t)) {
                thing_move_to(t, &surplus->base);
                dropped++;
            }
            t = next;
        }
        if (dropped > 0) {
            snprintf(msg, sizeof(msg), "%s arrives and leaves %s here for those in need.\r\n",
                     cap_first(m->base.short_descr, capbuf, sizeof(capbuf)),
                     dropped == 1 ? "an item" : "a few items");
            descriptor_room_echo(surplus, NULL, msg);
        }

        thing_set_room(&m->base, from);
        snprintf(msg, sizeof(msg), "%s returns, ready to tidy up again.\r\n",
                 cap_first(m->base.short_descr, capbuf, sizeof(capbuf)));
        descriptor_room_echo(from, NULL, msg);
        return;
    }

    if (rand() % 100 >= MOB_SURPLUS_COLLECT_CHANCE_PCT)
        return;

    obj_t *pick = NULL;
    int count = 0;
    for (thing_t *t = m->base.roomp->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_OBJ)
            continue;
        obj_t *o = (obj_t *)t;
        if (!obj_takeable(o->wear_flag))
            continue;
        count++;
        if (rand() % count == 0) /* reservoir sampling, same as mob_try_scavenge() above */
            pick = o;
    }
    if (!pick)
        return;

    const char *label = pick->base.short_descr[0] ? pick->base.short_descr : pick->base.name;
    snprintf(msg, sizeof(msg), "%s picks up %s.\r\n",
             cap_first(m->base.short_descr, capbuf, sizeof(capbuf)), label);
    descriptor_room_echo(m->base.roomp, NULL, msg);
    thing_move_to(&pick->base, &m->base);
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
    /* `beast soother` (Druid, level 5, stub-audit fix): a calmed animal
     * mob doesn't pick fresh fights for the duration. */
    if (being_has_affect(m, AFFECT_CALMED))
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
        /* `feign death` (Monk, level 25, level-25 audit batch: "Play
         * dead to avoid detection or attack."). A feigning PC (see
         * cmd_feigndeath.c's own `feigning` flag) is skipped by
         * aggressive-mob targeting -- the real "avoid attack" half of
         * the roster text. */
        if (pc->feigning)
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

/* Pursuit (Sneezy → Tobin feature audit, "Monster AI & behavior
 * (pursuit)"). Checked Sneezy's own 14-monster-ai-behavior.md doc first:
 * the real system is a whole emotional/opinion model (hate/fear lists,
 * per-class combat AI dispatch, faction territorial combat, scripted
 * mobresponses) with a persistence/distance-based multi-room hunt() that
 * runs its own state machine across pulses -- Tobin has none of that
 * infrastructure (no per-mob memory, no cross-tick hunting pointer), and
 * building it is its own separate undertaking. Scoped to the one
 * concrete, named gap: an ACT_AGGRESSIVE mob a player successfully flees
 * from (cmd_flee.c) currently just lets them go with zero chance of
 * giving chase. This is a single-room, immediate reaction, not the
 * original's real hunt -- a mob either catches up right now or gives up
 * for good, no lingering hunting state, no following through a second
 * doorway. Same "placeholder odds" precedent cmd_flee.c's own escape
 * chance already uses. Returns true iff `m` followed and re-engaged. */
#define MOB_PURSUE_CHANCE_PCT 50

bool mob_ai_try_pursue(being_t *m, being_t *fled_ch, room_t *to) {
    if (!m || m->base.kind != THING_MOB || !fled_ch || !to)
        return false;
    if (!(m->mob_actions & ACT_AGGRESSIVE))
        return false;
    if (rand() % 100 >= MOB_PURSUE_CHANCE_PCT)
        return false;

    thing_set_room(&m->base, to);
    m->fighting = fled_ch;
    fled_ch->fighting = m;
    being_set_wait(m, COMBAT_ROUND_PULSES);

    char capbuf[128];
    char msg[192];
    snprintf(msg, sizeof(msg), "%s chases you down!\r\n",
             cap_first(m->base.short_descr, capbuf, sizeof(capbuf)));
    if (fled_ch->desc)
        descriptor_notify(fled_ch->desc, msg);

    snprintf(msg, sizeof(msg), "%s bursts in, hot on %s's trail!\r\n",
             cap_first(m->base.short_descr, capbuf, sizeof(capbuf)), fled_ch->base.name);
    descriptor_room_echo(to, m, msg);

    return true;
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

/* Same keyword-abbreviation matching spirit as cmd_drink.c's own local
 * copy (a "puddle"-style tag search, not a prefix match) -- duplicated
 * rather than shared, same established precedent across this codebase. */
static bool keyword_matches(const char *keywords, const char *tok) {
    size_t tok_len = strlen(tok);
    if (tok_len == 0)
        return false;
    const char *p = keywords;
    while (*p) {
        while (*p == ' ')
            p++;
        const char *start = p;
        while (*p && *p != ' ')
            p++;
        size_t wlen = (size_t)(p - start);
        if (wlen >= tok_len && strncasecmp(start, tok, tok_len) == 0)
            return true;
    }
    return false;
}

/* SPEC_PROC_LAMPLIGHTER (mob_ai.h has the full scope-down rationale):
 * lights every "lamppost"-keyworded OBJ_CAT_LIGHT object in this mob's
 * OWN current room at night, extinguishes it by day, auto-refueling to
 * full each time it lights one (the original's own "infinite fuel
 * supply for the NPC" -- TLight::lampLightStuff(), obj_light.cc). */
static void mob_try_lamplighter(being_t *m) {
    if (m->mob_spec_proc != SPEC_PROC_LAMPLIGHTER || !m->base.roomp)
        return;

    bool daytime = gametime_is_daytime();
    char capbuf[128], msg[256];
    for (thing_t *t = m->base.roomp->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_OBJ)
            continue;
        obj_t *o = (obj_t *)t;
        if (o->category != OBJ_CAT_LIGHT || !keyword_matches(o->base.name, "lamppost"))
            continue;

        if (daytime && o->val[3]) {
            o->val[3] = 0;
            snprintf(msg, sizeof(msg), "%s reaches up and extinguishes %s for the day.\r\n",
                     cap_first(m->base.short_descr, capbuf, sizeof(capbuf)), o->base.short_descr);
            descriptor_room_echo(m->base.roomp, NULL, msg);
        } else if (!daytime && !o->val[3]) {
            if (o->val[1] >= 0)
                o->val[2] = o->val[1];
            if (o->val[2] <= 0)
                continue; /* unrefuelable (val[1]<0) and already spent -- nothing this mob can do */
            o->val[3] = 1;
            snprintf(msg, sizeof(msg), "%s reaches up high and lights %s for the night.\r\n",
                     cap_first(m->base.short_descr, capbuf, sizeof(capbuf)), o->base.short_descr);
            descriptor_room_echo(m->base.roomp, NULL, msg);
        }
    }
}

/* Generic spec-proc dispatch (SPEC_PROCS.md: "lets port over all special
 * procedures from sneezy"). SPEC_PROC_DOCTOR/LAMPLIGHTER/NEWBIE_EQUIPPER
 * above pre-date this and stay as their own hand-special-cased checks
 * (already shipped/tested, no reason to churn them) -- everything ported
 * under this project going forward is registered here instead, in a real
 * id -> function table mirroring the original's `mob_specials[]` +
 * `TMonster::checkSpec`, since 322 more procs won't scale under the old
 * one-off pattern. Ids match the original's spec_mobs.h SPEC_* constants
 * verbatim, same "seeded numeric id from the real upstream data" contract
 * SPEC_PROC_DOCTOR/etc already established. Only a pulse hook exists so
 * far (mob_ai_tick's ~60s cadence) -- more hook points (speech, movement,
 * buy/list, ...) get added here as procs that need them are ported; see
 * SPEC_PROCS.md for the full checklist and known blockers. */
#define SPEC_CHICKEN 8

/* SPEC_CHICKEN (spec_mobs.cc's `chicken`): a rare per-tick chance to lay
 * an egg (obj vnum 2376, seeded verbatim) into the mob's own room. First
 * proc ported under this project -- chosen as the proof case because
 * it's fully self-contained (no faction/disease/pet/pathfinding
 * subsystem dependency, unlike most of the rest of spec_mobs.cc, see
 * SPEC_PROCS.md's blocker notes). Real upstream odds are 1-in-5000 per
 * pulse (`::number(0, 4999)` returns nonzero unless it rolls exactly 0);
 * kept verbatim rather than re-tuned for Tobin's own ~60s AI tick
 * cadence, which is slower than the original's per-pulse rate this was
 * tuned against -- a deliberate, disclosed scope note, not a bug. */
static void mob_spec_chicken_pulse(being_t *m) {
    if (!m->base.roomp || rand() % 5000 != 0)
        return;

    obj_t *egg = obj_create_from_proto(2376);
    if (!egg)
        return;
    thing_move_to(&egg->base, &m->base.roomp->base);

    char capbuf[128], msg[192];
    snprintf(msg, sizeof(msg), "%s lays an egg.\r\n",
             cap_first(m->base.short_descr, capbuf, sizeof(capbuf)));
    descriptor_room_echo(m->base.roomp, NULL, msg);
}

/* SPEC_REPLICANT (spec_mobs.cc's `replicant`) -- id 71, another
 * "wrongly marked [-] before the array-position correction" find
 * (SPEC_PROCS.md correction #2): no named constant in spec_mobs.h, but
 * a real slot in `mob_specials[]`. Third proc ported under this
 * project. On pulse, a damaged replicant heals to full and spawns a
 * fresh, undamaged copy of itself into the room -- `thing_t.id` IS the
 * live mob's own vnum for `THING_MOB` (thing.h's own doc comment),
 * so no extra lookup is needed to know what to respawn.
 * Deliberately unconditional, matching the original verbatim: it fires
 * whenever the mob is below full HP for ANY reason (a real fight, an
 * immortal's `hurt`, ...), not just mid-combat -- the original has no
 * narrower gate either. */
#define SPEC_REPLICANT 71

static void mob_spec_replicant_pulse(being_t *m) {
    if (!m->base.roomp || m->progress.hp >= m->progress.max_hp)
        return;

    char msg[192];
    snprintf(msg, sizeof(msg), "Drops of %s's blood hit the ground, and spring up into another one!\r\n",
             being_display_name(m));
    descriptor_room_echo(m->base.roomp, NULL, msg);

    being_t *copy = being_create_mob(m->base.id);
    if (copy) {
        thing_set_room(&copy->base, m->base.roomp);
        descriptor_room_echo(m->base.roomp, NULL, "Two undamaged opponents face you now.\r\n");
    }

    m->progress.hp = m->progress.max_hp;
}

/* SPEC_THIEF (spec_mobs.cc's `thief`) -- id 4, one of the "wrongly
 * marked [-] before the array-position correction" finds (SPEC_PROCS.md
 * correction #2): no named constant in spec_mobs.h, but a real slot in
 * `mob_specials[]`. On pulse, an awake standing thief mob that isn't
 * fighting has a 1-in-26 chance (upstream `::number(0, 25)`, kept
 * verbatim) to try pickpocketing a random non-immortal PC in its room
 * that also isn't fighting -- ported from upstream's own `rob_blind()`
 * helper (spec_mobs.cc): scans the victim's LOOSE (not worn/held, same
 * is_loose() rule cmd_steal.c's own player-driven steal already uses)
 * inventory, a 1-in-5 chance per item to actually consider it (upstream
 * `::number(0, 4)`), first considered item gets silently moved into the
 * thief's own inventory via thing_move_to() -- genuinely BLIND, unlike
 * `cmd_steal.c`'s player-driven version: no message to anyone, on
 * success OR if no item is found, matching upstream's own rob_blind()
 * (which only messages the room for the hobbit-race flavor line this
 * port skips, a disclosed simplification -- no hobbit-specific steal-
 * skill floor either, Tobin's skill system doesn't gate this mob-driven
 * path on a skill value the way a PC's `steal` does). No item-monogram
 * check (upstream skips monogrammed items) -- Tobin has no monogram
 * concept on objects. */
static void mob_spec_thief_pulse(being_t *m) {
    if (!m->base.roomp || m->position != POSITION_STANDING || m->fighting)
        return;
    if (rand() % 26 != 0)
        return;

    being_t *vict = NULL;
    for (thing_t *t = m->base.roomp->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_PC)
            continue;
        being_t *pc = (being_t *)t;
        if (pc->fighting || being_is_immortal(pc))
            continue;
        vict = pc;
        break;
    }
    if (!vict)
        return;

    for (thing_t *t = vict->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_OBJ)
            continue;
        obj_t *o = (obj_t *)t;
        bool loose = true;
        for (int i = 0; i < 2 && loose; i++)
            if (vict->held[i] == o)
                loose = false;
        for (int i = 0; i < LIMB_COUNT && loose; i++)
            if (vict->equipment[i] == o)
                loose = false;
        if (!loose || rand() % 5 != 0)
            continue;

        thing_move_to(&o->base, &m->base);
        if (vict->base.kind == THING_PC)
            player_inventory_save(vict->player_id, vict);
        return;
    }
}

#define SPEC_THIEF 4

/* The actual dispatch table -- add one `case` per newly-ported proc that
 * only needs the pulse hook. Procs needing other hooks (speech, etc)
 * get their own small dispatch function alongside this one, called from
 * wherever that hook already lives (cmd_say.c, cmd_move.c, ...), same
 * "id -> function" shape either way. */
static void mob_spec_dispatch_pulse(being_t *m) {
    switch (m->mob_spec_proc) {
    case SPEC_CHICKEN:
        mob_spec_chicken_pulse(m);
        break;
    case SPEC_REPLICANT:
        mob_spec_replicant_pulse(m);
        break;
    case SPEC_THIEF:
        mob_spec_thief_pulse(m);
        break;
    default:
        break;
    }
}

/* SPEC_BEGGAR (spec_mobs.cc's `beggar`) -- id 17, found in the REAL
 * `mob_specials[]` registration array (spec_mobs.cc), not the sparse
 * named-constant list in spec_mobs.h (a scoping correction found this
 * session -- that header only names the entries referenced BY NAME
 * elsewhere in the C++ source; most of the array's 222 slots, including
 * this one, have no named constant at all and are only reachable by
 * array position/id). Second proc ported under this project, and the
 * first needing a hook beyond the pulse -- a new "given coins"/"given
 * item" pair, called from cmd_object.c's `give` right after a mob
 * successfully receives something. A mob has no descriptor to notify
 * directly, so reactions are room echoes (same convention chicken's
 * egg-laying announcement already uses). Two of the original's six
 * coin-amount reactions had crude language toned down (kept the beat/
 * amount-tiers verbatim, not the wording) -- a disclosed, deliberate
 * scope note, not a bug. */
#define SPEC_BEGGAR 17

static void mob_spec_beggar_given_item(being_t *m) {
    char capbuf[128], msg[192];
    snprintf(msg, sizeof(msg), "%s says, \"Thanks! I'll pawn that off for some coin.\"\r\n",
             cap_first(m->base.short_descr, capbuf, sizeof(capbuf)));
    descriptor_room_echo(m->base.roomp, NULL, msg);
}

static void mob_spec_beggar_given_coins(being_t *m, int amount) {
    char capbuf[128], msg[256];
    const char *name = cap_first(m->base.short_descr, capbuf, sizeof(capbuf));
    if (amount < 50) {
        snprintf(msg, sizeof(msg), "%s says, \"Hmph. Don't strain yourself.\"\r\n", name);
    } else if (amount < 250) {
        snprintf(msg, sizeof(msg), "%s says, \"Good... that'll buy a meal.\"\r\n", name);
    } else if (amount < 1000) {
        snprintf(msg, sizeof(msg), "%s says, \"Wow. Thanks!\"\r\n", name);
    } else if (amount < 10000) {
        snprintf(msg, sizeof(msg),
                 "%s staggers a bit, utterly amazed, and says, \"I can eat well for a year now! Thank you!\"\r\n",
                 name);
    } else if (amount < 100000) {
        snprintf(msg, sizeof(msg), "%s shakes uncontrollably, too stunned to speak.\r\n", name);
    } else {
        snprintf(msg, sizeof(msg), "%s passes out in amazement, hitting the ground with a loud *thud*!\r\n", name);
        m->position = POSITION_SLEEPING;
    }
    descriptor_room_echo(m->base.roomp, NULL, msg);
}

static void mob_spec_dispatch_given_item(being_t *m) {
    switch (m->mob_spec_proc) {
    case SPEC_BEGGAR:
        mob_spec_beggar_given_item(m);
        break;
    default:
        break;
    }
}

static void mob_spec_dispatch_given_coins(being_t *m, int amount) {
    switch (m->mob_spec_proc) {
    case SPEC_BEGGAR:
        mob_spec_beggar_given_coins(m, amount);
        break;
    default:
        break;
    }
}

/* Public hooks (see mob_ai.h) -- called from cmd_object.c's `give` right
 * after a mob target successfully receives an item/coins. No-op for a
 * mob with no matching spec_proc (the common case). */
void mob_ai_notify_given_item(being_t *m) {
    if (!m || m->base.kind != THING_MOB || !m->base.roomp)
        return;
    mob_spec_dispatch_given_item(m);
}

void mob_ai_notify_given_coins(being_t *m, int amount) {
    if (!m || m->base.kind != THING_MOB || !m->base.roomp)
        return;
    mob_spec_dispatch_given_coins(m, amount);
}

/* Public wrapper (see mob_ai.h) for egotrip's `wander` subcommand --
 * forces the one wander attempt mob_try_wander() would otherwise only
 * make on a per-tick dice roll. */
void mob_ai_force_wander(being_t *m) {
    if (!m || m->base.kind != THING_MOB)
        return;
    mob_try_wander(m, true);
}

/* world_for_each_mob() callback for mob_ai_tick() below -- runs every
 * one of this mob's independent AI behaviors in turn (wander, scavenge,
 * surplus collect/deliver, aggress, alignment flavor, lamplighter).
 * Combat follow-through for a charmed pet is deliberately NOT here --
 * see the trailing comment for why that lives in combat.c instead. */
static void mob_ai_visit(being_t *m) {
    mob_try_wander(m, false);
    mob_try_scavenge(m);
    mob_try_surplus_collect(m);
    mob_try_aggress(m);
    mob_try_align_flavor(m);
    mob_try_lamplighter(m);
    mob_spec_dispatch_pulse(m);
    /* Pet/charm's own "joins its master's fight" logic lives in
     * combat.c's pet-assist pass (combat_process_run()) instead of here
     * -- this AI tick runs on a ~60s wander/scavenge cadence, far too
     * slow for combat (COMBAT_ROUND_PULSES is ~1.2s); a first version
     * set it here and a charmed pet could sit out nearly a full minute
     * of its master's fight before ever engaging. mob_try_wander()'s own
     * AFFECT_CHARMED guard above still applies -- a pet just never
     * randomly wanders, regardless of where the join logic lives. */
}

/* Runs on a timer (see main.c), roughly every ~60s: visits every mob in
 * the world and lets it take its own independent AI action for this
 * tick (mob_ai_visit() above). Deliberately slow-cadence -- combat
 * itself resolves on a much faster pulse in combat.c, not here. */
void mob_ai_tick(long pulse_num) {
    (void)pulse_num;
    world_for_each_mob(mob_ai_visit);
}

/* See mob_ai.h's doc comment. Only greets for a matching mob's OWN class
 * suit -- a mob with no suit defined for the arriver's class (suit_id < 0)
 * says nothing, same "nothing for you right now" spirit as the say-
 * triggered reissue in cmd_say.c. Stops at the first matching mob in the
 * room, same convention try_newbie_equipper() uses. */
void mob_ai_greet_newbie_equipper(being_t *arriver, room_t *r) {
    if (!arriver || !r || !arriver->desc || arriver->player_id <= 0)
        return;

    for (thing_t *t = r->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_MOB)
            continue;
        being_t *mob = (being_t *)t;
        if (mob->mob_spec_proc != SPEC_PROC_NEWBIE_EQUIPPER)
            continue;

        int suit_id = suit_repo_find_for_class((int)arriver->char_class);
        if (suit_id < 0)
            return;

        int vnums[SUIT_MAX_ITEMS], qtys[SUIT_MAX_ITEMS];
        int n = suit_repo_load_items_qty(suit_id, vnums, qtys, SUIT_MAX_ITEMS);
        if (n == 0)
            return;

        char capbuf[128];
        being_display_name_cap(mob, capbuf, sizeof(capbuf));
        char out[1536];
        size_t len = (size_t)snprintf(out, sizeof(out),
            "%s looks you over and says, \"If you're ever short on gear, just ask "
            "-- here's what I can set you up with:\"\r\n", capbuf);
        for (int i = 0; i < n && len < sizeof(out); i++) {
            obj_proto_t proto;
            const char *label = obj_proto_load(vnums[i], &proto) ? proto.short_descr : "(unknown item)";
            if (qtys[i] > 1)
                len += (size_t)snprintf(out + len, sizeof(out) - len, "  %s (x%d)\r\n", label, qtys[i]);
            else
                len += (size_t)snprintf(out + len, sizeof(out) - len, "  %s\r\n", label);
        }
        descriptor_send(arriver->desc, out);
        return;
    }
}
