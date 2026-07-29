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

#include "being.h"
#include "descriptor.h"
#include "gametime.h"
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
 * the comment inside for why pets never wander on their own). */
static void mob_try_wander(being_t *m) {
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

/* world_for_each_mob() callback for mob_ai_tick() below -- runs every
 * one of this mob's independent AI behaviors in turn (wander, scavenge,
 * surplus collect/deliver, aggress, alignment flavor, lamplighter).
 * Combat follow-through for a charmed pet is deliberately NOT here --
 * see the trailing comment for why that lives in combat.c instead. */
static void mob_ai_visit(being_t *m) {
    mob_try_wander(m);
    mob_try_scavenge(m);
    mob_try_surplus_collect(m);
    mob_try_aggress(m);
    mob_try_align_flavor(m);
    mob_try_lamplighter(m);
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
