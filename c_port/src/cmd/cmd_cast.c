/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "affect.h"
#include "cmd.h"
#include "combat.h"
#include "help_repo.h"
#include "obj.h"
#include "obj_magic_repo.h"
#include "liquids.h"
#include "player_repo.h"
#include "pulse.h"
#include "room.h"
#include "spellcast.h"
#include "spell_mana.h"
#include "mob_ai.h"
#include "room_repo.h"
#include "skill.h"
#include "spell_component.h"
#include "thing.h"
#include "weather.h"
#include "world.h"

/* `cast <spell>` -- Mage/Druid spellcasting (user 2026-07-11: "druids and
 * mages should require components to cast with, so implement task_pray
 * task_cast etc"). Cleric's `pray` (cmd_pray.c) is the sibling command,
 * gated on a holy symbol instead of a component.
 *
 * A "spell component" is any object anywhere in the caster's own
 * containment chain (carried, worn, or held -- thing_name_matches()
 * covers all three uniformly, same as `get`/`wear`) whose keyword list
 * contains the word "component" -- a deliberately generic convention so
 * a builder can create any reagent/component-pouch item ("a pouch of
 * spell components") without a new object category. A component now has
 * real CHARGES (user 2026-07-18: "should be getting 10 casts out of each
 * component"), not single-use -- val[0]/val[1] hold current/max charges
 * (obj.h's val[] doc, same MAGIC_DEVICE precedent), decremented on every
 * cast ATTEMPT (success or fail, same timing the original always used --
 * see the original's TComponent::charges/useComponent(),
 * misc/obj_component.cc, "use up one charge... else discard it as
 * worthless"), only actually destroyed once the last charge is spent.
 *
 * v1 scope: no mana cost (Tobin has no mana/resource pool yet -- see
 * TODO.md's spell-framework backlog) and no bespoke per-spell mechanic
 * for the full ~150-entry roster -- this implements the casting GATE the
 * user asked for (class + level + component), with a real, generic
 * effect for every category the roster actually contains (heal/cure/
 * ward/single-target damage/area-effect damage, all keyed off the
 * spell's own name/description, see task_cast()'s own comment for the
 * full breakdown) and a plain "nothing happens yet" fallback only for
 * mechanics Tobin has no subsystem for at all yet (teleport/summon/
 * polymorph/invisibility/...), so every listed spell is at least
 * reachable and consumes its component correctly. */

/* "eyes of Fertuman" (Mage 22) world-wide search state -- world_for_each_mob()/
 * world_for_each_obj() take a plain visitor callback with no user-data
 * parameter, so the search target/caster/running count are shared via these
 * statics, same "counting visitor" idiom cmd_load.c's count_mob_visit()/
 * count_obj_visit() already use. Capped at 5 total matches across both
 * mobs and objects (g_fertuman_found), a flat simplification of the real
 * upstream's skill/level-scaled cap. */
static being_t *g_fertuman_ch;
static const char *g_fertuman_target;
static int g_fertuman_found;

static void fertuman_mob_visit(being_t *m) {
    if (g_fertuman_found >= 5 || m == g_fertuman_ch || !m->base.roomp)
        return;
    if (being_is_immortal(m))
        return;
    if (!thing_name_matches(m->base.name, g_fertuman_target, strlen(g_fertuman_target)))
        return;
    char buf[512];
    snprintf(buf, sizeof(buf), "  %s is in %s.\r\n",
             being_display_name(m), m->base.roomp->base.name);
    if (g_fertuman_ch->desc)
        descriptor_send(g_fertuman_ch->desc, buf);
    g_fertuman_found++;
}

static void fertuman_obj_visit(obj_t *o) {
    if (g_fertuman_found >= 5 || !o->base.roomp)
        return; /* only a room-floor item has a reliable room (thing.h) */
    if (!thing_name_matches(o->base.name, g_fertuman_target, strlen(g_fertuman_target)))
        return;
    char buf[512];
    const char *label = o->base.short_descr[0] ? o->base.short_descr : o->base.name;
    snprintf(buf, sizeof(buf), "  %s is in %s.\r\n", label, o->base.roomp->base.name);
    if (g_fertuman_ch->desc)
        descriptor_send(g_fertuman_ch->desc, buf);
    g_fertuman_found++;
}

/* Case-insensitive "does haystack contain needle" (strcasestr is GNU-only,
 * same style already duplicated in cmd_exec.c/cmd_scan.c/combat.c). */
static bool ci_contains(const char *haystack, const char *needle) {
    size_t nlen = strlen(needle);
    for (const char *p = haystack; *p; p++)
        if (strncasecmp(p, needle, nlen) == 0)
            return true;
    return false;
}

/* `dufali` (Monk, missing-skill audit, 2026-08-09): real upstream help
 * text -- "a quasi-magic that permits a trained monk to ignore certain
 * antagonistic magics cast upon the yofuist. Various charms,
 * paralyzations and holding spells are known to be counterable while
 * some poisons can also be negated... not a perfect science." Wired in
 * as a resist check at each hostile bind/sleep/poison application site
 * that can land on a PC target (cmd_cast.c's own `ensorcer` charm spell
 * is mob-only, so there's no PC-charm site to hook here). Proficiency-
 * scaled at half strength ("not a perfect science" -- never a guaranteed
 * counter even at max skill). */
static bool dufali_resists(being_t *target) {
    if (!target || being_is_immortal(target))
        return false;
    if (!being_knows_skill(target, "dufali"))
        return false;
    const skill_def_t *sk = skill_find(target->char_class, "dufali", false);
    if (!sk)
        return false;
    return skill_roll_success(skill_learn_from_doing(target, sk) / 2);
}

/* Looks through everything `ch` is carrying, wearing, or holding for
 * an item whose name/keywords contain `keyword` -- used here to find
 * a spell component. Returns NULL if there isn't one. */
static obj_t *find_keyword_item(const being_t *ch, const char *keyword) {
    size_t len = strlen(keyword);
    for (thing_t *t = ch->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind == THING_OBJ && thing_name_matches(t->name, keyword, len))
            return (obj_t *)t;
    }
    return NULL;
}

/* Component gate for a cast (per-spell binding, ported from real
 * SneezyMUD's findComponent()). If the spell has a seeded reagent bound to
 * it (obj.val2, see spell_component.c), the caster must be holding THAT
 * reagent -- searched carried/worn/held plus one level into a spellbag.
 * A spell with no bound reagent anywhere (Tobin-original / unseeded) keeps
 * the old generic "any component-keyword item" behavior so nothing that
 * cast before still refuses. Returns the component to consume, or NULL. */
/* A non-consumable stand-in returned for immortals, so no cast's
 * component gate ever refuses an immortal and consume_component()
 * never touches it (user: immortals don't require spell components). */
static obj_t g_immortal_component_sentinel;

static obj_t *component_for_cast(being_t *ch, const char *spell, bool imm) {
    if (imm)
        return &g_immortal_component_sentinel;
    if (spell_bound_component_vnum(spell) > 0) {
        obj_t *c = spell_component_find_for(ch, spell);
        if (c)
            return c;
        /* Real Sneezy exempts NOHASSLE immortals from the exact-reagent
         * rule (useComponent() returns TRUE for them) -- let an immortal
         * fall back to any component-keyword item they happen to carry. */
        if (imm)
            return find_keyword_item(ch, "component");
        return NULL;
    }
    return find_keyword_item(ch, "component");
}

/* Skips a leading inline color tag ("<o>a torch<1>") before capitalizing
 * -- same duplication precedent as cmd_light.c/cmd_object.c's own
 * cap_first(), needed here for a component-consumed message that opens
 * a sentence with the item's own short_descr. */
static const char *cap_first(const char *label, char *buf, size_t bufsz) {
    snprintf(buf, bufsz, "%s", label);
    size_t i = 0;
    while (buf[i] == '<' && buf[i + 1] != '\0' && buf[i + 2] == '>')
        i += 3;
    if (buf[i])
        buf[i] = (char)toupper((unsigned char)buf[i]);
    return buf;
}

/* Spends one charge from `component` -- destroys it only once that was
 * the last one. A pre-existing/never-charged item (val[0]==0, e.g.
 * something loaded before this system existed) is treated as a single
 * fallback charge so it still works once instead of refusing outright. */
static void consume_component(descriptor_t *d, obj_t *component) {
    if (!component || component == &g_immortal_component_sentinel)
        return;
    int charges = component->val[0] > 0 ? component->val[0] : 1;
    if (charges > 1) {
        component->val[0] = charges - 1;
        return;
    }
    char capbuf[128], msg[192];
    const char *label = component->base.short_descr[0] ? component->base.short_descr : component->base.name;
    snprintf(msg, sizeof(msg), "%s is used up.\r\n", cap_first(label, capbuf, sizeof(capbuf)));
    descriptor_send(d, msg);
    obj_destroy(component);
}

/* Looks up `name` (a prefix is fine, e.g. "cast heal" reaches "heal
 * light") in the roster for `cls`, restricted to spells (not the
 * SKILL_TIER_COMBAT weapon-proficiency placeholders, which aren't
 * something you "cast"). First match wins, same abbreviation convention
 * as command/skill-name matching elsewhere. `any_class` (immortals --
 * user 2026-07-12: "immortals can use any skill or spell in game, no
 * class restrictions") searches the whole roster instead of just `cls`. */
/* Per-word prefix match, so a multi-word spell/prayer name can be
 * abbreviated word-by-word ("sorc globe" -> "sorcerer's globe"), not only
 * by a single leading prefix of the whole string (which "sorc globe"
 * fails, diverging at "sorc " vs "sorce", so the old whole-string
 * strncasecmp wrongly peeled "globe" off as a target). Each
 * whitespace-delimited token of `query` must prefix the matching token of
 * `full`, in order; a query with MORE tokens than `full` fails, so a real
 * trailing target word is still left for find_spell_and_target() to peel. */
static bool spell_name_matches(const char *full, const char *query) {
    while (*query == ' ')
        query++;
    while (*query) {
        while (*full == ' ')
            full++;
        if (*full == '\0')
            return false; /* query still has a token but `full` ran out */
        while (*query && *query != ' ') {
            if (*full == ' ' || *full == '\0'
                || tolower((unsigned char)*full) != tolower((unsigned char)*query))
                return false;
            query++;
            full++;
        }
        while (*full && *full != ' ')
            full++; /* skip the rest of this `full` token */
        while (*query == ' ')
            query++;
    }
    return true;
}

static const skill_def_t *find_spell(player_class_t cls, const char *name, bool any_class) {
    int count = skill_count();
    for (int i = 0; i < count; i++) {
        const skill_def_t *sk = skill_at(i);
        if ((!any_class && sk->cls != cls) || sk->tier == SKILL_TIER_COMBAT)
            continue;
        if (spell_name_matches(sk->name, name))
            return sk;
    }
    return NULL;
}

/* Splits `args` into a spell (possibly abbreviated, possibly multi-word)
 * and an optional trailing target name -- same convention cmd_pray.c's
 * identical helper already established (duplicated here rather than
 * shared, matching this codebase's "small static helpers get duplicated
 * per command file" convention). Tries the WHOLE string against
 * find_spell() first; only if that fails does it peel off the last word
 * and retry the remainder, treating the peeled word as a target.
 * `*out_target` is NULL for a self-cast. Added as part of "offensive
 * spell breadth" (Sneezy -> Tobin feature audit) -- `cast` previously had
 * NO target syntax at all, unlike `pray`, which silently meant an
 * offensive spell could only ever hit whoever `ch->fighting` already was. */
static const skill_def_t *find_spell_and_target(player_class_t cls, const char *args,
                                                bool any_class, char *target_buf, size_t target_bufsz,
                                                const char **out_target) {
    *out_target = NULL;
    const skill_def_t *sk = find_spell(cls, args, any_class);
    if (sk)
        return sk;

    const char *last_space = strrchr(args, ' ');
    if (!last_space || !last_space[1])
        return NULL;

    size_t spell_len = (size_t)(last_space - args);
    char spell_buf[128];
    if (spell_len >= sizeof(spell_buf))
        return NULL;
    memcpy(spell_buf, args, spell_len);
    spell_buf[spell_len] = '\0';

    sk = find_spell(cls, spell_buf, any_class);
    if (!sk)
        return NULL;

    snprintf(target_buf, target_bufsz, "%s", last_space + 1);
    *out_target = target_buf;
    return sk;
}

/* Damage scales with the SPELL's own min_level, not the caster's current
 * level (the pre-breadth formula's actual bug: a level-1 "gust" and a
 * level-50 "atomize" dealt identical damage to a max-level caster).
 * Rough calibration against combat.c's melee formula (~1 + str-derived +
 * rand(0-5), typically single digits to ~teens): a level-1 spell lands
 * around a weak melee hit, a level-50 one hits several times harder --
 * "casters nuke, but scale with how hard-won the spell was to reach". */
static int spell_damage_for_level(int min_level) {
    return 4 + min_level + (rand() % (min_level / 3 + 4));
}

/* Real area-effect for spells whose own description says so verbatim
 * ("area-effect burst of X damage") -- previously every one of these
 * (pebble spray, fireball, tsunami, hellfire, ...) silently behaved
 * exactly like a single-target spell, contradicting their own text. Hits
 * every OTHER being in the room except the caster and their own group
 * (being_in_group(), same friendly-fire exclusion the original's area
 * spells use) -- PCs and mobs alike. Uses combat_apply_skill_damage()
 * (not a raw being_hurt_limb()) so a kill is handled correctly (XP,
 * corpse, defeat cleanup) instead of leaving a 0-HP being in a broken
 * half-defeated state, the same real gap this whole pass also fixes for
 * the single-target path below. Does NOT establish new `fighting`
 * relationships for bystanders it catches -- Tobin's combat model is
 * strictly 1v1, so an area spell is a supplemental burst, not a way to
 * open simultaneous fights with everyone in the room. */
static void cast_area_damage(descriptor_t *d, being_t *ch, const skill_def_t *sk) {
    if (!ch->base.roomp) {
        descriptor_send(d, "You aren't anywhere.\r\n");
        return;
    }
    int dmg = spell_damage_for_level(sk->min_level);
    int hit_count = 0;
    room_t *r = ch->base.roomp;
    for (thing_t *t = r->base.stuff_head; t; ) {
        thing_t *next = t->stuff_next; /* save before a hit might destroy t */
        if (t == &ch->base || (t->kind != THING_PC && t->kind != THING_MOB)) {
            t = next;
            continue;
        }
        being_t *victim = (being_t *)t;
        if (t->kind == THING_PC && !victim->desc) {
            t = next; /* linkdead -- can't be manipulated, same rule combat_find_room_target() uses */
            continue;
        }
        if (being_in_group(ch, victim)) {
            t = next;
            continue;
        }
        hit_count++;
        /* LIMB_REAL_COUNT, not LIMB_COUNT (Limbs -> wearSlotT, 2026-07-26)
         * -- excludes the mob-only, always-inactive EX_* placeholder
         * slots from ever being randomly hit. */
        limb_t limb = (limb_t)(rand() % LIMB_REAL_COUNT);
        int limb_hp_before = victim->limbs[limb].hp;
        bool defeated = combat_apply_skill_damage(ch, victim, dmg, limb);
        if (!defeated && victim->desc) {
            char msg[128];
            snprintf(msg, sizeof(msg), "The %s catches you %s!\r\n",
                     sk->name, describe_dam(dmg, limb_hp_before, NULL));
            descriptor_notify(victim->desc, msg);
        }
        t = next;
    }
    char msg[160];
    if (hit_count > 0)
        snprintf(msg, sizeof(msg), "You unleash %s, catching everyone nearby!\r\n", sk->name);
    else
        snprintf(msg, sizeof(msg), "You unleash %s, but there's no one else here to catch in it.\r\n", sk->name);
    descriptor_send(d, msg);
    if (hit_count > 0) {
        snprintf(msg, sizeof(msg), "%s unleashes %s, catching everyone nearby!\r\n",
                 being_display_name(ch), sk->name);
        descriptor_room_echo(r, ch, msg);
    }
}

/* Applies a real effect for the categories of spell this roster
 * actually contains, based on the spell's own name/one-line description
 * (both real Sneezy flavor text, see skill.c) -- expanded 2026-07-18
 * (user: "implement spell/skill affects... make each work from sneezy
 * code") beyond the original heal/damage-only v1, and again as part of
 * "offensive spell breadth" (Sneezy -> Tobin feature audit): `target`
 * is now a real resolved being (self, by default -- see
 * find_spell_and_target()), not implicitly always `ch`, and the
 * damage/area branches below replace the old flat-regardless-of-tier
 * formula with real breadth (see spell_damage_for_level()/
 * cast_area_damage()'s own comments). Cure poison/disease reuse THIS
 * session's own disease/poison affect work (affect.h) -- casting "cure
 * poison" now genuinely removes AFFECT_POISON, closing the loop with
 * `drink`'s puddle-poison roll and the hospital's cure. Protective
 * spells (armor/shield/resistance/stone skin/wards, a large chunk of
 * the Mage/Druid roster) all reuse the SAME AFFECT_SANCTUARY damage-
 * reduction mechanic "sanctuary" itself already uses -- an honest
 * scope-down (one real shared buff, not ~30 bespoke elemental-
 * resistance systems Tobin has no damage-type model to back anyway)
 * rather than a silent no-op. Still not attempted: mana costs (no mana
 * pool exists), elemental damage TYPES as a real mechanic (no immunity
 * system exists to back it -- messaging stays generic), and anything
 * needing a subsystem Tobin doesn't have at all yet (teleport/summon/
 * polymorph/invisibility/...) -- those fall through to the same honest
 * "nothing happens yet" placeholder as before. */
/* `iron will` (Monk, level 48 -- Session 158 backlog). The passive
 * mental-resistance counter to the mind-affecting spells (fear,
 * slumber, transfix): a being who knows it simply throws the effect
 * off. Returns true (and messages both sides + trains the resister's
 * own skill) when the effect is resisted, so each caller can early-
 * out. Immortals aren't handled here (their own per-spell immortal
 * checks already run first). */
static bool iron_will_resists(descriptor_t *d, being_t *ch, being_t *victim, const char *effect) {
    if (!victim || being_is_immortal(victim) || !being_knows_skill(victim, "iron will"))
        return false;
    if (victim->base.kind == THING_PC) {
        const skill_def_t *iw = skill_find(victim->char_class, "iron will", false);
        if (iw)
            skill_learn_from_doing(victim, iw);
    }
    char m[192];
    snprintf(m, sizeof(m), "You reach for %s's mind, but their iron will throws off the %s!\r\n",
             being_display_name(victim), effect);
    descriptor_send(d, m);
    if (victim->desc) {
        char cb[128];
        snprintf(m, sizeof(m), "%s reaches for your mind, but your iron will throws off the %s!\r\n",
                 being_display_name_cap(ch, cb, sizeof(cb)), effect);
        descriptor_notify(victim->desc, m);
    }
    return true;
}

void cmd_cast_resolve_effect(descriptor_t *d, being_t *ch, being_t *target, const skill_def_t *sk) {
    char msg[192];
    /* skill_def_t's own `desc` field is a generic "See help `X` for
     * help." placeholder for every roster entry (the real descriptive
     * text -- "damaging attackers", "breathe underwater", etc. --
     * migrated to the help_topic table at some point in this project's
     * history, per `hedit`). The keyword branches below were written
     * against that real prose and never updated for the move, so they
     * look up the live help_topic body here (falling back to sk->desc,
     * harmlessly inert, if a spell has no help topic yet) instead of
     * trusting the roster's own dead placeholder text. */
    char help_body[HELP_BODY_MAX];
    const char *desc = help_topic_load_exact(sk->name, help_body, sizeof(help_body)) ? help_body : sk->desc;
    /* Resolved target for the OFFENSIVE (damage) branch only: an
     * explicit target (target != ch) is used as-is; with none given,
     * falls back to whoever ch is already fighting, same as before this
     * breadth pass. NULL (no explicit target AND not fighting anyone)
     * is handled with a "who?" message rather than silently doing
     * nothing. Heal/buff branches keep using `target` directly (self by
     * default), unaffected -- see cmd_pray.c's identical atk_target for
     * the full rationale.
     *
     * Formerly `static void task_cast(...)`, and formerly opened with a
     * one-shot spell_flavor_show() + MSP casting-sound flourish right
     * here -- both now live in spellcast.c's spellcast_start()/
     * spellcast_tick_run() instead (user 2026-08-09: "spell casting
     * should take 2-3 rounds before hitting with purple colored
     * messaging... 2-3 lines per casting tick"), shown once per round of
     * the new multi-round delay rather than once at the moment the
     * effect below actually lands. This function's own body is
     * otherwise 100% unchanged -- a pure timing move, not a rewrite of
     * what any spell DOES. Exposed (non-static) so spellcast.c's
     * spellcast_tick_run() can call it once a delayed cast's countdown
     * completes -- see spellcast.h's doc comment. */
    being_t *atk_target = (target != ch) ? target : ch->fighting;
    /* Shaman/Druid audit batch C, 2026-08-09 -- these 5 exact-name checks
     * (flatulence/raze/shield of mists/living vines/thornflesh) MUST run
     * before any generic ci_contains(desc/sk->name, ...) catch-all
     * further down this chain, or they're unreachable dead code: found
     * live (smoke_test_missing_skills_batchc.py) that `shield of mists`
     * was silently resolving as a generic "protective ward" self-buff
     * instead of its own real effect, because the much-earlier generic
     * armor/ward branch's `ci_contains(sk->name, "shield")` check
     * matches the substring "shield" inside "shield of mists" and wins
     * first when this block was placed in file order alongside its
     * sibling spells (right after `bramble drain`, further down) --
     * same root-cause SHAPE as the sk->desc/help_topic dead-branch bug
     * Session 145 fixed, just a fresh substring collision instead of a
     * stale-data-source one. Moved here, first in the whole dispatch
     * chain, rather than only relocating `shield of mists` alone --
     * every exact-name check in an if-else chain like this one is
     * safest kept ahead of every generic substring check, on principle,
     * not just the one collision this happened to catch live. */
    if (strcasecmp(sk->name, "knot") == 0) {
        /* `knot` (Mage, missing-skill audit, 2026-08-09): real upstream
         * (disc_mage_spirit.cc's knot()) "tears a gap in reality" and
         * steps the CASTER (self only -- no target argument in the real
         * source) through to a hardcoded safe room, refusing to work in
         * a no-escape room or on a murderer. Same fallback-room shape
         * `word of recall` (Cleric, cmd_pray.c) already established for
         * "no per-player hometown/recall-point concept in Tobin" --
         * reuses that identical DEFAULT_LOAD_ROOM_MORTAL destination
         * rather than a second hardcoded room, since Tobin has no
         * equivalent of the real room 2387 to port literally. Not
         * ported: the AFFECT_PLAYERKILL murderer refusal (no PK-murder
         * flag exists in Tobin, same disclosed gap `word of recall`
         * already carries). */
        if (ch->base.roomp && (ch->base.roomp->room_flag & (ROOM_FLAG_ARENA | ROOM_FLAG_NO_ESCAPE))) {
            descriptor_send(d, "The defenses of this area are too strong -- you can't tear a gap in reality here.\r\n");
            return;
        }
        room_t *dest = world_get_room(DEFAULT_LOAD_ROOM_MORTAL);
        if (!dest) {
            dest = room_repo_load(DEFAULT_LOAD_ROOM_MORTAL);
            if (dest)
                world_register_room(dest);
        }
        if (!dest) {
            descriptor_send(d, "You tear a gap in reality, but it leads nowhere -- you stay put.\r\n");
            return;
        }
        ch->fighting = NULL;
        room_t *old_room = ch->base.roomp;
        char capbuf[128];
        being_display_name_cap(ch, capbuf, sizeof(capbuf));
        if (old_room) {
            snprintf(msg, sizeof(msg), "%s tears a gap in reality and steps through.\r\n", capbuf);
            descriptor_room_echo(old_room, ch, msg);
        }
        descriptor_send(d, "You tear a gap in reality and step through.\r\n");
        thing_set_room(&ch->base, dest);
        snprintf(msg, sizeof(msg), "%s steps out of a gap in reality.\r\n", capbuf);
        descriptor_room_echo(dest, ch, msg);
        cmd_dispatch(d, "look");
        return;
    } else if (strcasecmp(sk->name, "sunscald") == 0) {
        /* sunscald (Druid, level 16 -- Session 158 backlog, lowest
         * unimplemented spell). A searing lance of concentrated sunlight:
         * a straightforward single-target radiant damage spell, same
         * open-combat + damage shape as the generic damage branch below
         * but given its own exact-name branch so it never depends on
         * help-body keyword text (the Session 145 dead-branch lesson). */
        if (!atk_target) {
            descriptor_send(d, "Cast that at whom?\r\n");
            return;
        }
        if (being_is_immortal(atk_target)) {
            descriptor_send(d, "You can't do that to an immortal being.\r\n");
            return;
        }
        if (!ch->fighting) {
            ch->fighting = atk_target;
            atk_target->fighting = ch;
            being_set_wait(ch, COMBAT_ROUND_PULSES);
        }
        int dmg = spell_damage_for_level(sk->min_level);
        limb_t limb = (limb_t)(rand() % LIMB_REAL_COUNT);
        int hpb = atk_target->limbs[limb].hp;
        bool defeated = combat_apply_skill_damage(ch, atk_target, dmg, limb);
        const char *intensity = describe_dam(dmg, hpb, NULL);
        snprintf(msg, sizeof(msg), "You scald %s with a searing lance of sunlight, burning them %s!\r\n",
                 being_display_name(atk_target), intensity);
        descriptor_send(d, msg);
        if (!defeated && atk_target->desc) {
            char tcb[128];
            snprintf(msg, sizeof(msg), "%s scalds you with a searing lance of sunlight, burning you %s!\r\n",
                     being_display_name_cap(ch, tcb, sizeof(tcb)), intensity);
            descriptor_notify(atk_target->desc, msg);
        }
    } else if (strcasecmp(sk->name, "withering touch") == 0) {
        /* withering touch (Druid, level 32 -- Session 158 backlog). A
         * necrotic touch: single-target damage PLUS a lingering decay on
         * a surviving victim, reusing AFFECT_POISON as the HP-draining
         * "wither" over the next several rounds (Tobin has no dedicated
         * necrosis affect; AFFECT_POISON is its closest working DoT, same
         * reuse precedent as several spells in this file). */
        if (!atk_target) {
            descriptor_send(d, "Cast that at whom?\r\n");
            return;
        }
        if (being_is_immortal(atk_target)) {
            descriptor_send(d, "You can't do that to an immortal being.\r\n");
            return;
        }
        if (!ch->fighting) {
            ch->fighting = atk_target;
            atk_target->fighting = ch;
            being_set_wait(ch, COMBAT_ROUND_PULSES);
        }
        int dmg = spell_damage_for_level(sk->min_level);
        limb_t limb = (limb_t)(rand() % LIMB_REAL_COUNT);
        int hpb = atk_target->limbs[limb].hp;
        bool defeated = combat_apply_skill_damage(ch, atk_target, dmg, limb);
        const char *intensity = describe_dam(dmg, hpb, NULL);
        if (!defeated)
            being_apply_affect(atk_target, AFFECT_POISON, 10);
        snprintf(msg, sizeof(msg), "You lay a withering touch on %s, rotting the flesh %s!\r\n",
                 being_display_name(atk_target), intensity);
        descriptor_send(d, msg);
        if (!defeated && atk_target->desc) {
            char tcb[128];
            snprintf(msg, sizeof(msg), "%s lays a withering touch on you, rotting your flesh %s!\r\n",
                     being_display_name_cap(ch, tcb, sizeof(tcb)), intensity);
            descriptor_notify(atk_target->desc, msg);
        }
    } else if (strcasecmp(sk->name, "wave crash") == 0) {
        /* wave crash (Druid, level 32 -- Session 158 backlog). A crashing
         * wall of water: a real room-wide area-effect burst, reusing the
         * shared cast_area_damage() helper (hits every non-grouped being
         * in the room, friendly-fire excluded), same as fireball/tsunami/
         * flatulence. */
        cast_area_damage(d, ch, sk);
    } else if (strcasecmp(sk->name, "feral wrath") == 0) {
        /* feral wrath (Druid, level 28 -- Session 158 backlog). A self
         * only battle-fury: the caster gives in to animal rage, sharpening
         * their own blows. Reuses AFFECT_BLESS -- the attacker-side to-hit
         * + flat-damage buff (combat.c) -- as the "savage blows" effect,
         * same "map onto the closest existing working affect" precedent as
         * the rest of this roster. Always lands on the caster, never a
         * named target (a rage is not something you cast onto someone
         * else). */
        being_apply_affect(ch, AFFECT_BLESS, WARD_DURATION_ROUNDS);
        descriptor_send(d, "You surrender to a feral wrath -- your blows turn savage and sure!\r\n");
        if (ch->base.roomp) {
            char rcb[128], recho[160];
            snprintf(recho, sizeof(recho), "%s's eyes go wild with a feral wrath!\r\n",
                     being_display_name_cap(ch, rcb, sizeof(rcb)));
            descriptor_room_echo(ch->base.roomp, ch, recho);
        }
    } else if (strcasecmp(sk->name, "leeching vine") == 0) {
        /* leeching vine (Druid, level 48 -- Session 158 backlog). A
         * parasitic vine that drains the victim and feeds the caster:
         * damage plus a heal-back, same combat_apply_skill_damage() +
         * being_heal() drain shape as life leech/bramble drain (a heavy
         * 3/4 transfer, befitting a level-48 spell). */
        if (!atk_target) {
            descriptor_send(d, "Cast that at whom?\r\n");
            return;
        }
        if (being_is_immortal(atk_target)) {
            descriptor_send(d, "You can't do that to an immortal being.\r\n");
            return;
        }
        if (!ch->fighting) {
            ch->fighting = atk_target;
            atk_target->fighting = ch;
            being_set_wait(ch, COMBAT_ROUND_PULSES);
        }
        int dmg = spell_damage_for_level(sk->min_level);
        limb_t limb = (limb_t)(rand() % LIMB_REAL_COUNT);
        int hpb = atk_target->limbs[limb].hp;
        bool defeated = combat_apply_skill_damage(ch, atk_target, dmg, limb);
        const char *intensity = describe_dam(dmg, hpb, NULL);
        int drained = dmg * 3 / 4;
        if (drained < 1)
            drained = 1;
        being_heal(ch, drained);
        snprintf(msg, sizeof(msg),
                 "Leeching vines coil around %s, draining them %s and feeding you! (+%d HP)\r\n",
                 being_display_name(atk_target), intensity, drained);
        descriptor_send(d, msg);
        if (!defeated && atk_target->desc) {
            char tcb[128];
            snprintf(msg, sizeof(msg), "%s's leeching vines coil around you, draining you %s!\r\n",
                     being_display_name_cap(ch, tcb, sizeof(tcb)), intensity);
            descriptor_notify(atk_target->desc, msg);
        }
    } else if (strcasecmp(sk->name, "tree walk") == 0) {
        /* tree walk (Druid, level 41 -- Session 158 backlog). Step into
         * one tree and out of another far away: a SELF-only random
         * teleport, reusing the same room_repo_random_teleport_vnum() +
         * NO_ESCAPE gate the Mage `teleport` spell uses, minus the
         * offensive/target path (a druid tree-walks themselves, not
         * others). */
        if (!ch->base.roomp) {
            descriptor_send(d, "You aren't anywhere.\r\n");
            return;
        }
        if (ch->base.roomp->room_flag & ROOM_FLAG_NO_ESCAPE) {
            descriptor_send(d, "The wild refuses you here -- there is no tree to step through.\r\n");
            return;
        }
        int dest_vnum = room_repo_random_teleport_vnum();
        room_t *dest = dest_vnum > 0 ? world_get_room(dest_vnum) : NULL;
        if (!dest && dest_vnum > 0) {
            dest = room_repo_load(dest_vnum);
            if (dest)
                world_register_room(dest);
        }
        if (!dest) {
            descriptor_send(d, "You reach for the deep roots, but nothing answers.\r\n");
            return;
        }
        room_t *old_room = ch->base.roomp;
        if (old_room) {
            char departmsg[128];
            snprintf(departmsg, sizeof(departmsg), "%s melts into the bark of a nearby tree!\r\n",
                     ch->base.name);
            descriptor_room_echo(old_room, ch, departmsg);
        }
        thing_set_room(&ch->base, dest);
        char arrmsg[128];
        snprintf(arrmsg, sizeof(arrmsg), "%s steps out from the trunk of a tree!\r\n", ch->base.name);
        descriptor_room_echo(dest, ch, arrmsg);
        descriptor_send(d, "You step into the bark and out through a tree far away!\r\n");
        cmd_dispatch(d, "look");
        return;
    } else if (strcasecmp(sk->name, "death mist") == 0) {
        /* Death mist (Druid, level 45, Advanced discipline -- Tier-5
         * scope-out reopened by user 2026-08-16: "high level druid...
         * about level 45"). Real upstream SPELL_DEATH_MIST
         * (disc_shaman_skunk.cc's deathMist) is a DISC_SHAMAN spell,
         * folded onto Druid on the same "no Shaman class in Tobin"
         * precedent as raze/root control/the whole shaman-nature line.
         * The caster breathes out a chilling green mist that infects
         * every other (non-immortal, not-already-infected) being in the
         * room with syphilis -- AFFECT_DISEASE_SYPHILIS, Tobin's real
         * HP-drain-over-time disease affect (affect.c) -- skipping the
         * caster's own groupmates, exactly as upstream does. Faithful to
         * upstream's self-infect risk, a failed secondary skill roll
         * leaves the caster infected too. Placed here as an early
         * exact-name branch (ahead of every generic ci_contains() check)
         * on the same principle the flatulence/raze group documents.
         * Disclosed divergences: upstream also rides a -10 STR modifier
         * (APPLY_STR) on the affect -- Tobin diseases are fixed-effect by
         * type with no per-cast stat rider, so that extra is dropped; and
         * the isImmune(IMMUNE_DISEASE) waist-gear check has no Tobin
         * equivalent (no disease-immunity item system), so it's skipped. */
        if (!ch->base.roomp) {
            descriptor_send(d, "You aren't anywhere.\r\n");
            return;
        }
        /* A nasty, lingering affliction -- longer than the combat
         * diseases (garrotte 90 / bleeding 40 rounds) to reflect
         * upstream's ~25-mudhour syphilis, but still bounded. */
        const int DEATH_MIST_DURATION_ROUNDS = 150;
        descriptor_send(d, "<g>A chilling green mist escapes your open mouth.<z>\r\n");
        char dmcap[128];
        snprintf(msg, sizeof(msg), "<g>%s opens %s mouth and a chilling green mist pours out.<z>\r\n",
                 being_display_name_cap(ch, dmcap, sizeof(dmcap)), gender_possess(ch->gender));
        descriptor_room_echo(ch->base.roomp, ch, msg);

        int infected = 0;
        room_t *dmr = ch->base.roomp;
        for (thing_t *t = dmr->base.stuff_head; t;) {
            thing_t *next = t->stuff_next;
            if (t == &ch->base || (t->kind != THING_PC && t->kind != THING_MOB)) {
                t = next;
                continue;
            }
            being_t *victim = (being_t *)t;
            if (t->kind == THING_PC && !victim->desc) {
                t = next;
                continue;
            }
            if (being_is_immortal(victim) || being_in_group(ch, victim)
                || being_has_affect(victim, AFFECT_DISEASE_SYPHILIS)) {
                t = next;
                continue;
            }
            being_apply_affect(victim, AFFECT_DISEASE_SYPHILIS, DEATH_MIST_DURATION_ROUNDS);
            infected++;
            if (victim->desc)
                descriptor_notify(victim->desc,
                                  "The green mist seeps into you -- you feel a sudden stinging in your waist.\r\n");
            char vcap[128];
            snprintf(msg, sizeof(msg), "%s starts to look a little uncomfortable.\r\n",
                     being_display_name_cap(victim, vcap, sizeof(vcap)));
            descriptor_room_echo(dmr, victim, msg);
            t = next;
        }

        /* Self-infect risk -- upstream re-rolls bSuccess and, on a fail,
         * the caster catches their own mist. */
        if (!being_is_immortal(ch) && !being_has_affect(ch, AFFECT_DISEASE_SYPHILIS)) {
            const skill_def_t *dmsk = skill_find(ch->char_class, "death mist", false);
            int pct = dmsk ? skill_proficiency(ch, dmsk) : 0;
            if (!skill_roll_success(pct)) {
                being_apply_affect(ch, AFFECT_DISEASE_SYPHILIS, DEATH_MIST_DURATION_ROUNDS);
                descriptor_send(d, "Some of your own mist curls back over you -- you feel a stinging in your waist!\r\n");
            }
        }

        if (!infected)
            descriptor_send(d, "Nothing nearby seems any worse for your mist.\r\n");
    } else if (strcasecmp(sk->name, "flatulence") == 0) {
        /* Shaman/Druid audit batch C, 2026-08-09: real upstream
         * (disc_shaman_skunk.cc) is a room-wide AoE that damages every
         * non-grouped, non-immortal occupant of the caster's room, with
         * a crit-fail case that damages the CASTER instead. Ported
         * faithfully -- same "everyone but your own group" exemption,
         * same real defensive pre-capture of `stuff_next` cmd_egotrip.c
         * already established for a room-walk loop that might delete
         * an occupant mid-pass (a defeated mob). Real upstream's own
         * suffocation-immunity exemption isn't ported (no per-being
         * immunity-type system in Tobin) -- a disclosed scope-cut. A
         * 1-in-10 mishap douses the caster instead, echoing the real
         * crit-fail case without needing the full crit-success/crit-
         * fail subsystem. */
        int dmg = spell_damage_for_level(sk->min_level);
        if (!(rand() % 10)) {
            being_hurt_limb_only(ch, (limb_t)(rand() % LIMB_REAL_COUNT), dmg);
            descriptor_send(d, "Oh no -- that one stuck with you! You choke on your own fumes!\r\n");
            char rmsg[128], capbuf[128];
            snprintf(rmsg, sizeof(rmsg), "%s chokes on their own fumes!\r\n",
                     being_display_name_cap(ch, capbuf, sizeof(capbuf)));
            descriptor_room_echo(ch->base.roomp, ch, rmsg);
        } else {
            descriptor_send(d, "You turn around quickly and pass gas -- noxious fumes fill the room!\r\n");
            char rmsg[128], capbuf[128];
            snprintf(rmsg, sizeof(rmsg), "%s turns around quickly and passes gas -- noxious fumes fill the room!\r\n",
                     being_display_name_cap(ch, capbuf, sizeof(capbuf)));
            descriptor_room_echo(ch->base.roomp, ch, rmsg);
            thing_t *t = ch->base.roomp->base.stuff_head;
            while (t) {
                thing_t *next = t->stuff_next;
                if (t->kind == THING_PC || t->kind == THING_MOB) {
                    being_t *occ = (being_t *)t;
                    if (occ != ch && !being_is_immortal(occ) && !being_in_group(ch, occ)) {
                        combat_apply_skill_damage(ch, occ, dmg, (limb_t)(rand() % LIMB_REAL_COUNT));
                        if (occ->desc)
                            descriptor_notify(occ->desc, "You are choked by the noxious fumes!\r\n");
                    }
                }
                t = next;
            }
        }
    } else if (strcasecmp(sk->name, "raze") == 0) {
        /* Shaman/Druid audit batch C, 2026-08-09: real upstream
         * (disc_shaman_spider.cc) is Shaman's single most powerful
         * attack spell (LIFEFORCE_400, the heaviest cost in the whole
         * roster) -- "calls upon the loa to erase any memory of" the
         * victim, refuses outright against an immortal, and doubles
         * damage on a critical success. Ported as roughly double the
         * usual spell_damage_for_level() output (reflecting its real
         * outsized power relative to every other damage spell this
         * audit ported) plus a flat 10% chance of doubling again --
         * a disclosed approximation of the real crit-success mechanic,
         * not the full crit-success/crit-fail/luck-resist subsystem. */
        if (!atk_target) {
            descriptor_send(d, "Cast that at whom?\r\n");
            return;
        }
        if (being_is_immortal(atk_target)) {
            descriptor_send(d, "You can't do that to an immortal being.\r\n");
            return;
        }
        if (!ch->fighting) {
            ch->fighting = atk_target;
            atk_target->fighting = ch;
            being_set_wait(ch, COMBAT_ROUND_PULSES);
        }
        int dmg = spell_damage_for_level(sk->min_level) * 2;
        if (!(rand() % 10))
            dmg *= 2;
        limb_t limb = (limb_t)(rand() % LIMB_REAL_COUNT);
        int limb_hp_before = atk_target->limbs[limb].hp;
        bool defeated = combat_apply_skill_damage(ch, atk_target, dmg, limb);
        const char *intensity = describe_dam(dmg, limb_hp_before, NULL);
        snprintf(msg, sizeof(msg), "You call upon the wrath of the wild to erase %s's existence -- they are razed %s!\r\n",
                 being_display_name(atk_target), intensity);
        descriptor_send(d, msg);
        if (!defeated && atk_target->desc) {
            char tcapbuf[128];
            snprintf(msg, sizeof(msg), "%s calls upon the wrath of the wild to erase your existence -- you are razed %s!\r\n",
                     being_display_name_cap(ch, tcapbuf, sizeof(tcapbuf)), intensity);
            descriptor_notify(atk_target->desc, msg);
        }
    } else if (strcasecmp(sk->name, "healing grasp") == 0) {
        /* Druid batch, 2026-08-11: real upstream disc_shaman_healing.cc
         * healingGrasp -- a straight restorative laid on a target (self
         * by default; TAR_CHAR_ROOM | TAR_FIGHT_SELF upstream). Same
         * being_heal() shape as the cure-light/salve branch below, sized
         * a notch above salve to match its higher roster slot. */
        int amount = 20 + ch->progress.level;
        being_heal(target, amount);
        if (target == ch) {
            snprintf(msg, sizeof(msg), "You lay a healing grasp upon yourself and feel restored! (+%d HP)\r\n", amount);
            descriptor_send(d, msg);
        } else {
            snprintf(msg, sizeof(msg), "You lay a healing grasp upon %s -- their wounds close. (+%d HP)\r\n",
                     being_display_name(target), amount);
            descriptor_send(d, msg);
            if (target->desc) {
                char tcapbuf[128];
                snprintf(msg, sizeof(msg), "%s lays a healing grasp upon you -- your wounds close! (+%d HP)\r\n",
                         being_display_name_cap(ch, tcapbuf, sizeof(tcapbuf)), amount);
                descriptor_notify(target->desc, msg);
            }
        }
    } else if (strcasecmp(sk->name, "life leech") == 0) {
        /* Druid batch, 2026-08-11: real upstream disc_shaman_skunk.cc
         * lifeLeech -- damage plus a life-drain heal-back to the caster,
         * same being-target/combat_apply_skill_damage()+being_heal()
         * shape as bramble drain, with a larger 3/4 transfer (a heavier
         * drain than bramble's "small amount"). */
        if (!atk_target) {
            descriptor_send(d, "Cast that at whom?\r\n");
            return;
        }
        if (being_is_immortal(atk_target)) {
            descriptor_send(d, "You can't do that to an immortal being.\r\n");
            return;
        }
        if (!ch->fighting) {
            ch->fighting = atk_target;
            atk_target->fighting = ch;
            being_set_wait(ch, COMBAT_ROUND_PULSES);
        }
        int dmg = spell_damage_for_level(sk->min_level);
        limb_t limb = (limb_t)(rand() % LIMB_REAL_COUNT);
        int limb_hp_before = atk_target->limbs[limb].hp;
        bool defeated = combat_apply_skill_damage(ch, atk_target, dmg, limb);
        const char *intensity = describe_dam(dmg, limb_hp_before, NULL);
        int drained = dmg * 3 / 4;
        if (drained < 1)
            drained = 1;
        being_heal(ch, drained);
        snprintf(msg, sizeof(msg),
                 "You leech the life from %s %s, drawing it into yourself! (+%d HP)\r\n",
                 being_display_name(atk_target), intensity, drained);
        descriptor_send(d, msg);
        if (!defeated && atk_target->desc) {
            char tcapbuf[128];
            snprintf(msg, sizeof(msg), "%s leeches the life from you %s!\r\n",
                     being_display_name_cap(ch, tcapbuf, sizeof(tcapbuf)), intensity);
            descriptor_notify(atk_target->desc, msg);
        }
    } else if (strcasecmp(sk->name, "vampiric touch") == 0) {
        /* Druid batch, 2026-08-11: real upstream disc_shaman_skunk.cc
         * vampiricTouch -- a full life-transfer drain (heal-back equal
         * to the damage dealt, vs life leech's 3/4), same shape as life
         * leech above. */
        if (!atk_target) {
            descriptor_send(d, "Cast that at whom?\r\n");
            return;
        }
        if (being_is_immortal(atk_target)) {
            descriptor_send(d, "You can't do that to an immortal being.\r\n");
            return;
        }
        if (!ch->fighting) {
            ch->fighting = atk_target;
            atk_target->fighting = ch;
            being_set_wait(ch, COMBAT_ROUND_PULSES);
        }
        int dmg = spell_damage_for_level(sk->min_level);
        limb_t limb = (limb_t)(rand() % LIMB_REAL_COUNT);
        int limb_hp_before = atk_target->limbs[limb].hp;
        bool defeated = combat_apply_skill_damage(ch, atk_target, dmg, limb);
        const char *intensity = describe_dam(dmg, limb_hp_before, NULL);
        int drained = dmg;
        if (drained < 1)
            drained = 1;
        being_heal(ch, drained);
        snprintf(msg, sizeof(msg),
                 "Your vampiric touch drains %s %s, and their life floods into you! (+%d HP)\r\n",
                 being_display_name(atk_target), intensity, drained);
        descriptor_send(d, msg);
        if (!defeated && atk_target->desc) {
            char tcapbuf[128];
            snprintf(msg, sizeof(msg), "%s's vampiric touch drains you %s!\r\n",
                     being_display_name_cap(ch, tcapbuf, sizeof(tcapbuf)), intensity);
            descriptor_notify(atk_target->desc, msg);
        }
    } else if (strcasecmp(sk->name, "squish") == 0) {
        /* Druid batch, 2026-08-11: real upstream disc_shaman_spider.cc
         * squish -- a crushing single-target strike. Ported as a
         * standard spell_damage_for_level() hit (Tobin has no crit-
         * success subsystem to port the real doubling into -- the same
         * disclosed scope-cut as raze). */
        if (!atk_target) {
            descriptor_send(d, "Cast that at whom?\r\n");
            return;
        }
        if (being_is_immortal(atk_target)) {
            descriptor_send(d, "You can't do that to an immortal being.\r\n");
            return;
        }
        if (!ch->fighting) {
            ch->fighting = atk_target;
            atk_target->fighting = ch;
            being_set_wait(ch, COMBAT_ROUND_PULSES);
        }
        int dmg = spell_damage_for_level(sk->min_level);
        limb_t limb = (limb_t)(rand() % LIMB_REAL_COUNT);
        int limb_hp_before = atk_target->limbs[limb].hp;
        bool defeated = combat_apply_skill_damage(ch, atk_target, dmg, limb);
        const char *intensity = describe_dam(dmg, limb_hp_before, NULL);
        snprintf(msg, sizeof(msg), "You squish %s beneath a sudden crushing weight %s!\r\n",
                 being_display_name(atk_target), intensity);
        descriptor_send(d, msg);
        if (!defeated && atk_target->desc) {
            char tcapbuf[128];
            snprintf(msg, sizeof(msg), "%s squishes you beneath a sudden crushing weight %s!\r\n",
                     being_display_name_cap(ch, tcapbuf, sizeof(tcapbuf)), intensity);
            descriptor_notify(atk_target->desc, msg);
        }
    } else if (strcasecmp(sk->name, "boiling blood") == 0) {
        /* Druid batch, 2026-08-11: real upstream disc_shaman_skunk.cc
         * bloodBoil -- superheats the target's blood for heavy damage
         * (LIFEFORCE_150 upstream). Ported as a slightly-above-baseline
         * strike; Tobin has no heat damage-type or over-time burn to
         * port the real periodic effect into (disclosed scope-cut, same
         * as inferno's own note). */
        if (!atk_target) {
            descriptor_send(d, "Cast that at whom?\r\n");
            return;
        }
        if (being_is_immortal(atk_target)) {
            descriptor_send(d, "You can't do that to an immortal being.\r\n");
            return;
        }
        if (!ch->fighting) {
            ch->fighting = atk_target;
            atk_target->fighting = ch;
            being_set_wait(ch, COMBAT_ROUND_PULSES);
        }
        int dmg = spell_damage_for_level(sk->min_level) * 5 / 4;
        limb_t limb = (limb_t)(rand() % LIMB_REAL_COUNT);
        int limb_hp_before = atk_target->limbs[limb].hp;
        bool defeated = combat_apply_skill_damage(ch, atk_target, dmg, limb);
        const char *intensity = describe_dam(dmg, limb_hp_before, NULL);
        snprintf(msg, sizeof(msg), "You set %s's blood boiling %s!\r\n",
                 being_display_name(atk_target), intensity);
        descriptor_send(d, msg);
        if (!defeated && atk_target->desc) {
            char tcapbuf[128];
            snprintf(msg, sizeof(msg), "%s sets your blood boiling %s!\r\n",
                     being_display_name_cap(ch, tcapbuf, sizeof(tcapbuf)), intensity);
            descriptor_notify(atk_target->desc, msg);
        }
    } else if (strcasecmp(sk->name, "coronary") == 0) {
        /* Druid batch, 2026-08-11: real upstream disc_shaman_skunk.cc
         * cardiacStress ("coronary") -- a heart attack, the batch's
         * heaviest single strike (TASK_DANGEROUS, LAG_4, LIFEFORCE_240
         * upstream, near the top of the Shaman roster). Ported at 7/4
         * baseline, between an ordinary strike and raze's outsized 2x;
         * refuses against an immortal with upstream's own quip. */
        if (!atk_target) {
            descriptor_send(d, "Cast that at whom?\r\n");
            return;
        }
        if (being_is_immortal(atk_target)) {
            descriptor_send(d, "Gods don't have heart attacks -- they don't have hearts.\r\n");
            return;
        }
        if (!ch->fighting) {
            ch->fighting = atk_target;
            atk_target->fighting = ch;
            being_set_wait(ch, COMBAT_ROUND_PULSES);
        }
        int dmg = spell_damage_for_level(sk->min_level) * 7 / 4;
        limb_t limb = LIMB_BODY;
        int limb_hp_before = atk_target->limbs[limb].hp;
        bool defeated = combat_apply_skill_damage(ch, atk_target, dmg, limb);
        const char *intensity = describe_dam(dmg, limb_hp_before, NULL);
        snprintf(msg, sizeof(msg), "%s clutches their chest and keels over as you stop their heart %s!\r\n",
                 being_display_name(atk_target), intensity);
        msg[0] = (char)toupper((unsigned char)msg[0]);
        descriptor_send(d, msg);
        if (!defeated && atk_target->desc) {
            char tcapbuf[128];
            snprintf(msg, sizeof(msg), "%s stops your heart -- the pain is INTENSE %s!\r\n",
                     being_display_name_cap(ch, tcapbuf, sizeof(tcapbuf)), intensity);
            descriptor_notify(atk_target->desc, msg);
        }
    } else if (strcasecmp(sk->name, "shield of mists") == 0) {
        /* Shaman/Druid audit batch C, 2026-08-09 -- see
         * AFFECT_SHIELD_OF_MISTS's own doc comment (affect.h) for the
         * real-to-Tobin mechanic mapping. Self-castable or castable on
         * a room-mate, matching real upstream's TAR_CHAR_ROOM |
         * TAR_FIGHT_SELF target spec. */
        being_t *recipient = (target != ch) ? target : ch;
        being_apply_affect(recipient, AFFECT_SHIELD_OF_MISTS, 40 * COMBAT_ROUND_PULSES);
        if (recipient == ch) {
            descriptor_send(d, "You are enveloped by a thick green mist!\r\n");
        } else {
            snprintf(msg, sizeof(msg), "%s is enveloped by a thick green mist!\r\n",
                     being_display_name(recipient));
            descriptor_send(d, msg);
            if (recipient->desc)
                descriptor_notify(recipient->desc, "You are enveloped by a thick green mist!\r\n");
        }
    } else if (strcasecmp(sk->name, "living vines") == 0) {
        /* Shaman/Druid audit batch C, 2026-08-09 -- see
         * AFFECT_LIVING_VINES's own doc comment (affect.h). Outdoor
         * gate already enforced earlier in cmd_cast() (see that
         * check's own comment). */
        if (!atk_target) {
            descriptor_send(d, "Cast that at whom?\r\n");
            return;
        }
        if (!ch->fighting) {
            ch->fighting = atk_target;
            atk_target->fighting = ch;
            being_set_wait(ch, COMBAT_ROUND_PULSES);
        }
        being_apply_affect(atk_target, AFFECT_LIVING_VINES, 30 * COMBAT_ROUND_PULSES);
        snprintf(msg, sizeof(msg), "Living vines burst from the earth and wrap tight around %s!\r\n",
                 being_display_name(atk_target));
        descriptor_send(d, msg);
        if (atk_target->desc) {
            char tcapbuf[128];
            snprintf(msg, sizeof(msg), "%s calls forth living vines, which burst from the earth and wrap tight around you!\r\n",
                     being_display_name_cap(ch, tcapbuf, sizeof(tcapbuf)));
            descriptor_notify(atk_target->desc, msg);
        }
    } else if (strcasecmp(sk->name, "thornflesh") == 0) {
        /* Shaman/Druid audit batch C, 2026-08-09 -- see
         * AFFECT_THORNFLESH's own doc comment (affect.h) and combat.c's
         * own reflect-damage hook. Self only, matching real upstream's
         * TAR_SELF_ONLY | TAR_FIGHT_SELF. */
        if (being_has_affect(ch, AFFECT_THORNFLESH)) {
            descriptor_send(d, "Your flesh is already armored well enough with thorns.\r\n");
            return;
        }
        being_apply_affect(ch, AFFECT_THORNFLESH, 40 * COMBAT_ROUND_PULSES);
        descriptor_send(d, "Thorns emerge from your body!\r\n");
        char rmsg[128], capbuf[128];
        snprintf(rmsg, sizeof(rmsg), "Thorns emerge from %s's body!\r\n",
                 being_display_name_cap(ch, capbuf, sizeof(capbuf)));
        descriptor_room_echo(ch->base.roomp, ch, rmsg);
    } else if (strcasecmp(sk->name, "sorcerer's globe") == 0) {
        /* Level-1 stub-audit fix (2026-08-04, user: "lower level players
         * should get a full experience"): roster text "A magical shield
         * that buffs the group's defense" -- a genuine ROOM-WIDE
         * Sanctuary buff (same AFFECT_SANCTUARY the single-target
         * "shield"-family branch below already uses), reusing
         * cmd_rally.c's "every PC/mob in the room but not an immortal"
         * group-buff shape since Tobin has no separate group/party
         * roster to walk instead. */
        int hit = 0;
        for (thing_t *t = ch->base.roomp->base.stuff_head; t; t = t->stuff_next) {
            if (t->kind != THING_PC && t->kind != THING_MOB)
                continue;
            being_t *ally = (being_t *)t;
            if (being_is_immortal(ally))
                continue;
            being_apply_affect(ally, AFFECT_SANCTUARY, 12);
            hit++;
        }
        (void)hit;
        descriptor_send(d, "You cast sorcerer's globe -- a shimmering dome of protection settles over the room!\r\n");
        char rmsg[128], capbuf[128];
        snprintf(rmsg, sizeof(rmsg), "%s casts sorcerer's globe -- a shimmering dome of protection settles over the room!\r\n",
                 being_display_name_cap(ch, capbuf, sizeof(capbuf)));
        descriptor_room_echo(ch->base.roomp, ch, rmsg);
    } else if (strcasecmp(sk->name, "flare") == 0) {
        /* Level-3 stub-audit fix: "Lights up the room" -- Tobin's
         * darkness mechanic is a per-being affect check
         * (room_is_dark_for(), being.c), not a room-level light state
         * (room_t has no such field to flip), so this is scoped down to
         * a genuine room-wide version of mage sight below: every PC/mob
         * present (not immortals, same sorcerer's globe exclusion)
         * gets AFFECT_INFRAVISION for the duration, matching what
         * "lights up the room" actually needs to deliver -- everyone in
         * it can see. */
        int fhit = 0;
        for (thing_t *t = ch->base.roomp->base.stuff_head; t; t = t->stuff_next) {
            if (t->kind != THING_PC && t->kind != THING_MOB)
                continue;
            being_t *occ = (being_t *)t;
            if (being_is_immortal(occ))
                continue;
            being_apply_affect(occ, AFFECT_INFRAVISION, 10 * COMBAT_ROUND_PULSES);
            fhit++;
        }
        (void)fhit;
        descriptor_send(d, "You cast flare -- the room fills with a warm magical glow!\r\n");
        char rmsg[128], capbuf[128];
        snprintf(rmsg, sizeof(rmsg), "%s casts flare -- the room fills with a warm magical glow!\r\n",
                 being_display_name_cap(ch, capbuf, sizeof(capbuf)));
        descriptor_room_echo(ch->base.roomp, ch, rmsg);
    } else if (strcasecmp(sk->name, "infravision") == 0) {
        /* Level-28 stub-audit fix: its own standalone ADVANCED-tier
         * spell -- same real dark-vision mage sight's own infravision
         * piece already grants (AFFECT_INFRAVISION), just self-only and
         * longer-lasting to match the higher tier. */
        being_apply_affect(ch, AFFECT_INFRAVISION, 60 * COMBAT_ROUND_PULSES);
        descriptor_send(d, "You cast infravision -- your eyes adjust, piercing the darkness!\r\n");
    } else if (strcasecmp(sk->name, "true sight") == 0) {
        /* Level-28 stub-audit fix: "Sees through illusions and
         * disguises" -- scoped to the one real piece Tobin has a
         * mechanic for (AFFECT_DETECT_INVISIBLE, already a working spell
         * elsewhere), same "one real piece over faking the whole bundle"
         * precedent mage sight's own doc comment already used. */
        being_apply_affect(ch, AFFECT_DETECT_INVISIBLE, 100);
        descriptor_send(d, "You cast true sight -- your vision sharpens, piercing illusion and disguise alike!\r\n");
    } else if (strcasecmp(sk->name, "mage sight") == 0) {
        /* Level-1 stub-audit fix: roster text bundles infravision/true
         * sight/detection -- scoped down to just infravision (real dark-
         * vision, AFFECT_INFRAVISION/room_is_dark_for(), affect.h/
         * being.c), the one piece of the bundle Tobin has an actual
         * darkness mechanic to hook into; true sight (seeing through
         * illusions/disguises) and general detection have no matching
         * subsystem yet, a disclosed simplification -- same "one real
         * piece over faking the whole bundle" precedent this audit uses
         * elsewhere. Self only. */
        being_apply_affect(ch, AFFECT_INFRAVISION, 30 * COMBAT_ROUND_PULSES);
        descriptor_send(d, "You cast mage sight -- your eyes adjust, piercing the darkness!\r\n");
        char rmsg[128], capbuf[128];
        snprintf(rmsg, sizeof(rmsg), "%s's eyes glow faintly for a moment.\r\n",
                 being_display_name_cap(ch, capbuf, sizeof(capbuf)));
        descriptor_room_echo(ch->base.roomp, ch, rmsg);
    } else if (strcasecmp(sk->name, "teleport") == 0) {
        /* Full spell/skill/prayer roster import continued, level-5+ list
         * (audit continued): real upstream (disc_mage_sorcery.cc's
         * teleport()/genericTeleport()) sends the target -- self by
         * default, or another being if named -- to a genuinely RANDOM
         * room in the whole world, not a chosen destination (skill.c's
         * own roster text "random or chosen location" turned out to be
         * an inaccurate guess, same shape as chi/jirin/cintai's earlier
         * flavor-text corrections -- fixed to just "random location").
         * `target` here (not atk_target) is used directly: no target
         * means SELF-teleport, unlike every offensive spell in this file
         * that falls back to the caster's current opponent. Ported: the
         * NO-ESCAPE check on the CASTER's own room (real upstream checks
         * this regardless of self/offensive cast) and the DEATH/PRIVATE/
         * HAVE-TO-WALK exclusions on the destination (room_repo.h's new
         * room_repo_random_teleport_vnum(), one DB query instead of the
         * real client-side retry loop -- see that function's own doc
         * comment for why). Not ported: the `isLucky` resist-and-fizzle
         * roll on an offensive cast (the outer cast-proficiency roll
         * already stands in for it, same precedent as `fear`) and the
         * critical-failure branch that flings the CASTER instead. */
        if (!ch->base.roomp) {
            descriptor_send(d, "You aren't anywhere.\r\n");
            return;
        }
        if (ch->base.roomp->room_flag & ROOM_FLAG_NO_ESCAPE) {
            descriptor_send(d, "The defenses of this area are too strong.\r\n");
            return;
        }
        if (target != ch && being_is_immortal(target)) {
            snprintf(msg, sizeof(msg), "You can't do that to %s -- they're a god!\r\n",
                     being_display_name(target));
            descriptor_send(d, msg);
            return;
        }
        int dest_vnum = room_repo_random_teleport_vnum();
        room_t *dest = dest_vnum > 0 ? world_get_room(dest_vnum) : NULL;
        if (!dest && dest_vnum > 0) {
            dest = room_repo_load(dest_vnum);
            if (dest)
                world_register_room(dest);
        }
        if (!dest) {
            descriptor_send(d, "You reach for the ether, but nothing happens.\r\n");
            return;
        }
        room_t *old_room = target->base.roomp;
        if (target->desc) {
            descriptor_send(target->desc, "You shimmer out of existence!\r\n");
        }
        if (old_room) {
            char departmsg[128];
            snprintf(departmsg, sizeof(departmsg), "%s shimmers out of existence!\r\n",
                     target->base.name);
            descriptor_room_echo(old_room, target, departmsg);
        }
        thing_set_room(&target->base, dest);
        char arrivemsg[128];
        snprintf(arrivemsg, sizeof(arrivemsg), "%s shimmers into existence!\r\n", target->base.name);
        descriptor_room_echo(dest, target, arrivemsg);
        if (target->desc) {
            descriptor_send(target->desc, "You shimmer into existence somewhere else entirely!\r\n");
            cmd_dispatch(target->desc, "look");
        }
        if (target == ch) {
            snprintf(msg, sizeof(msg), "You cast %s -- reality wrenches and you're somewhere else!\r\n", sk->name);
            descriptor_send(d, msg);
        } else {
            snprintf(msg, sizeof(msg), "You cast %s at %s -- they vanish into thin air!\r\n",
                     sk->name, being_display_name(target));
            descriptor_send(d, msg);
        }
    } else if (strcasecmp(sk->name, "dispel magic") == 0) {
        /* Full spell/skill/prayer roster import continued, level-5+ list
         * (audit continued): checked the real upstream first
         * (disc_mage_alchemy.cc's `dispelMagic()`/`castDispelMagic()`)
         * -- it turns out to be entirely OBJECT-targeted in the real
         * game (strips an item's objaffect-style enchantment bonuses
         * and its ITEM_MAGIC flag), never a being at all. Skill.c's own
         * roster text ("Strips magical effects from a being or
         * object") is only half right, same kind of guess this audit
         * has corrected before -- but unlike those, this is a
         * deliberate DEVIATION from the real mechanic rather than a
         * flavor-text fix, disclosed here: Tobin has no runtime
         * per-object enchantment state to strip (objaffect rows are
         * permanent DB-defined weapon bonuses, not a temporary spell
         * effect), so the real object-only mechanic has nothing to
         * port onto. Beings, on the other hand, already have a real,
         * removable affects[] runtime (curse/poison/disease/fear/
         * stupidity/sanctuary/flying/... all live there) -- implemented
         * as a being-targeted "strip every active affect" instead,
         * self by default or a named target, functionally useful both
         * offensively (strip an enemy's buffs) and supportively (clear
         * an ally's curse/poison/disease/fear/stupidity all at once,
         * where the single-purpose cure spells only handle one kind
         * each). */
        int cleared = 0;
        for (int i = 0; i < MAX_ACTIVE_AFFECTS; i++) {
            if (target->affects[i].type != AFFECT_NONE) {
                being_remove_affect(target, target->affects[i].type);
                cleared++;
            }
        }
        if (target == ch) {
            snprintf(msg, sizeof(msg), cleared
                     ? "You cast %s -- a wave of anti-magic washes over you, stripping every active effect!\r\n"
                     : "You cast %s, but you have no active magical effects to strip.\r\n", sk->name);
            descriptor_send(d, msg);
        } else {
            snprintf(msg, sizeof(msg), cleared
                     ? "You cast %s over %s -- a wave of anti-magic strips every active effect from them!\r\n"
                     : "You cast %s over %s, but they have no active magical effects to strip.\r\n",
                     sk->name, being_display_name(target));
            descriptor_send(d, msg);
            if (target->desc && cleared)
                descriptor_notify(target->desc, "A wave of anti-magic washes over you, stripping every active effect!\r\n");
        }
    } else if (strcasecmp(sk->name, "cure poison") == 0) {
        bool had = being_has_affect(target, AFFECT_POISON);
        if (had)
            being_remove_affect(target, AFFECT_POISON);
        if (target == ch) {
            snprintf(msg, sizeof(msg), had
                     ? "You cast %s -- the poison in your veins fades away!\r\n"
                     : "You cast %s, but you weren't poisoned to begin with.\r\n", sk->name);
            descriptor_send(d, msg);
        } else {
            snprintf(msg, sizeof(msg), had
                     ? "You cast %s over %s -- their poison fades away!\r\n"
                     : "You cast %s over %s, but they weren't poisoned.\r\n",
                     sk->name, being_display_name(target));
            descriptor_send(d, msg);
            if (target->desc && had)
                descriptor_notify(target->desc, "The poison in your veins fades away!\r\n");
        }
    } else if (strcasecmp(sk->name, "cure disease") == 0) {
        bool cured = false;
        for (int i = 0; i < MAX_ACTIVE_AFFECTS; i++) {
            if (affect_is_disease(target->affects[i].type)) {
                being_remove_affect(target, target->affects[i].type);
                cured = true;
            }
        }
        if (target == ch) {
            snprintf(msg, sizeof(msg), cured
                     ? "You cast %s -- your sickness lifts!\r\n"
                     : "You cast %s, but you weren't sick to begin with.\r\n", sk->name);
            descriptor_send(d, msg);
        } else {
            snprintf(msg, sizeof(msg), cured
                     ? "You cast %s over %s -- their sickness lifts!\r\n"
                     : "You cast %s over %s, but they weren't sick.\r\n",
                     sk->name, being_display_name(target));
            descriptor_send(d, msg);
            if (target->desc && cured)
                descriptor_notify(target->desc, "Your sickness lifts!\r\n");
        }
    } else if (ci_contains(desc, "heal")
               /* `salve` (Druid, level-12 stub-audit fix): desc "Treats
                * a minor wound" carries no "heal" keyword despite being
                * a plain heal-tier spell -- same fix as Cleric's
                * identical spell (cmd_pray.c). */
               || strcasecmp(sk->name, "salve") == 0) {
        int amount = 8 + ch->progress.level / 2;
        being_heal(target, amount);
        if (target == ch) {
            snprintf(msg, sizeof(msg), "You cast %s and feel restored! (+%d HP)\r\n", sk->name, amount);
            descriptor_send(d, msg);
        } else {
            snprintf(msg, sizeof(msg), "You cast %s, and %s is restored! (+%d HP)\r\n",
                     sk->name, being_display_name(target), amount);
            descriptor_send(d, msg);
            if (target->desc) {
                char tcapbuf[128];
                snprintf(msg, sizeof(msg), "%s casts %s, restoring you! (+%d HP)\r\n",
                         being_display_name_cap(ch, tcapbuf, sizeof(tcapbuf)), sk->name, amount);
                descriptor_notify(target->desc, msg);
            }
        }
    } else if (ci_contains(desc, "armor bonus") || ci_contains(desc, "reduces incoming damage")
               || ci_contains(desc, "resistance to") || ci_contains(desc, "reflective shield")
               || ci_contains(desc, "self-ward") || ci_contains(sk->name, "shield")
               || ci_contains(sk->name, "stone skin") || ci_contains(sk->name, "barkskin")
               || ci_contains(sk->name, "flaming flesh")) {
        /* `flaming flesh` (Mage, level 25, level-25 audit batch) folded in
         * here: real upstream (disc_mage_fire.cc's flamingFlesh()) turned
         * out to be a pure APPLY_ARMOR buff despite the roster's own
         * "damaging attackers" flavor text -- no reflect-damage mechanic
         * in the real spell at all, same kind of flavor-text correction
         * this audit has made before (chi/jirin/cintai, teleport). */
        being_apply_affect(target, affect_ward_for(sk->name, desc), WARD_DURATION_ROUNDS);
        if (target == ch) {
            snprintf(msg, sizeof(msg), "You cast %s -- a protective ward settles over you!\r\n", sk->name);
            descriptor_send(d, msg);
        } else {
            snprintf(msg, sizeof(msg), "You cast %s over %s -- a protective ward settles over them!\r\n",
                     sk->name, being_display_name(target));
            descriptor_send(d, msg);
            if (target->desc)
                descriptor_notify(target->desc, "A protective ward settles over you!\r\n");
        }
    } else if (ci_contains(desc, "breathe underwater")) {
        /* "gills of flesh" (Sneezy → Tobin feature audit, "Water,
         * drowning, flight") -- a real, longer-than-combat duration
         * (100 rounds, ~2 real minutes) since this is a travel-utility
         * buff meant to outlast a swim across several rooms, not a
         * combat-round ward like Sanctuary's 12. */
        being_apply_affect(ch, AFFECT_WATERBREATH, 100);
        snprintf(msg, sizeof(msg), "You cast %s -- gills split open along your neck!\r\n", sk->name);
        descriptor_send(d, msg);
    } else if (ci_contains(desc, "float above the ground")) {
        being_apply_affect(ch, AFFECT_FLYING, 100);
        snprintf(msg, sizeof(msg), "You cast %s -- you rise gently off the ground!\r\n", sk->name);
        descriptor_send(d, msg);
    } else if (ci_contains(desc, "area-effect")) {
        /* Real room-wide effect (breadth work) -- previously fell into
         * the single-target branch below like everything else. */
        cast_area_damage(d, ch, sk);
    } else if (strcasecmp(sk->name, "storm call") == 0) {
        /* Level-23 audit batch (2026-07-28). Ported from Shaman's real
         * `stormy skies` (disc_shaman_frog.cc's stormySkies()), renamed/
         * reflavored for Druid same as the rest of Druid's Shaman-sourced
         * damage spells (see the roster-import writeup above). Real
         * upstream gates on RAINY/LIGHTNING/SNOWY weather AND the target
         * being outdoors -- Tobin has no SNOWY sky state and no per-room
         * weather (weather.h is one world-wide sky state), so this gates
         * on WEATHER_RAINY/WEATHER_STORMY plus the caster's own room not
         * being ROOM_FLAG_INDOORS (the same indoor check gametime.c's own
         * darkness gate already uses). No half-damage luck-save or crit-
         * success double-damage roll (same "no crit branch ported"
         * precedent as every other spell audited this batch). */
        if (!atk_target) {
            descriptor_send(d, "Cast that at whom?\r\n");
            return;
        }
        weather_t w = weather_current();
        bool bad_weather = w != WEATHER_RAINY && w != WEATHER_STORMY;
        bool indoors = ch->base.roomp && (ch->base.roomp->room_flag & ROOM_FLAG_INDOORS);
        if (bad_weather || indoors) {
            descriptor_send(d, "You fail to call upon the weather to aid you!\r\n");
            if (indoors)
                descriptor_send(d, "You have to be outside to cast this spell!\r\n");
            return;
        }
        if (!ch->fighting) {
            ch->fighting = atk_target;
            atk_target->fighting = ch;
            being_set_wait(ch, COMBAT_ROUND_PULSES);
        }
        int dmg = spell_damage_for_level(sk->min_level);
        limb_t limb = (limb_t)(rand() % LIMB_REAL_COUNT);
        int limb_hp_before = atk_target->limbs[limb].hp;
        bool defeated = combat_apply_skill_damage(ch, atk_target, dmg, limb);
        const char *intensity = describe_dam(dmg, limb_hp_before, NULL);
        const char *flavor = (w == WEATHER_STORMY) ? "a lightning bolt from the stormy skies"
                                                    : "a driving bolt of rain";
        snprintf(msg, sizeof(msg), "You call down %s, striking %s %s!\r\n",
                 flavor, being_display_name(atk_target), intensity);
        descriptor_send(d, msg);
        if (!defeated && atk_target->desc) {
            char tcapbuf[128];
            snprintf(msg, sizeof(msg), "%s calls down %s, striking you %s!\r\n",
                     being_display_name_cap(ch, tcapbuf, sizeof(tcapbuf)), flavor, intensity);
            descriptor_notify(atk_target->desc, msg);
        }
    } else if (strcasecmp(sk->name, "clot") == 0) {
        /* Level-5 stub-audit fix (Druid only -- Mage has no "clot" in
         * its roster, so this branch is unreachable there). Same
         * targeted AFFECT_DISEASE_BLEEDING cure as Cleric's identical
         * spell (cmd_pray.c). */
        bool had = being_has_affect(target, AFFECT_DISEASE_BLEEDING);
        if (had)
            being_remove_affect(target, AFFECT_DISEASE_BLEEDING);
        if (target == ch) {
            snprintf(msg, sizeof(msg), had
                     ? "You cast %s -- your bleeding stops!\r\n"
                     : "You cast %s, but you weren't bleeding to begin with.\r\n", sk->name);
            descriptor_send(d, msg);
        } else {
            snprintf(msg, sizeof(msg), had
                     ? "You cast %s over %s -- their bleeding stops!\r\n"
                     : "You cast %s over %s, but they weren't bleeding.\r\n",
                     sk->name, being_display_name(target));
            descriptor_send(d, msg);
            if (target->desc && had)
                descriptor_notify(target->desc, "Your bleeding stops!\r\n");
        }
    } else if (strcasecmp(sk->name, "sense life") == 0) {
        descriptor_send(d, "You cast sense life -- your mind reaches out, sensing the living...\r\n");
        cmd_scan(d, "");
    } else if (strcasecmp(sk->name, "accelerate") == 0) {
        /* Level-11 stub-audit fix: "A group buff that speeds everyone
         * up" -- the group-wide version of `haste`'s own single-target
         * AFFECT_HASTE (see that spell's doc comment for the mechanic
         * and disclosed scope-cut), same room-walk-loop shape as
         * sorcerer's globe/flare/feathery descent above. */
        int achit = 0;
        for (thing_t *t = ch->base.roomp->base.stuff_head; t; t = t->stuff_next) {
            if (t->kind != THING_PC && t->kind != THING_MOB)
                continue;
            being_t *occ = (being_t *)t;
            if (being_is_immortal(occ))
                continue;
            being_apply_affect(occ, AFFECT_HASTE, 60);
            achit++;
        }
        (void)achit;
        descriptor_send(d, "You cast accelerate -- everyone nearby feels a burst of speed!\r\n");
        char rmsg[128], capbuf[128];
        snprintf(rmsg, sizeof(rmsg), "%s casts accelerate -- everyone nearby feels a burst of speed!\r\n",
                 being_display_name_cap(ch, capbuf, sizeof(capbuf)));
        descriptor_room_echo(ch->base.roomp, ch, rmsg);
    } else if (strcasecmp(sk->name, "calm") == 0) {
        if (!atk_target) {
            descriptor_send(d, "Cast that at whom?\r\n");
            return;
        }
        being_t *calmed_other = atk_target->fighting;
        if (calmed_other) {
            atk_target->fighting = NULL;
            calmed_other->fighting = NULL;
        }
        if (atk_target->base.kind == THING_MOB)
            being_apply_affect(atk_target, AFFECT_CALMED, 30 * COMBAT_ROUND_PULSES);
        snprintf(msg, sizeof(msg), "You cast calm -- %s grows still, the violence draining away.\r\n",
                 being_display_name(atk_target));
        descriptor_send(d, msg);
        if (atk_target->desc) {
            char tcapbuf[128];
            snprintf(msg, sizeof(msg), "%s casts calm over you -- the violence drains away.\r\n",
                     being_display_name_cap(ch, tcapbuf, sizeof(tcapbuf)));
            descriptor_notify(atk_target->desc, msg);
        }
    } else if (strcasecmp(sk->name, "flight") == 0 || strcasecmp(sk->name, "falcon wings") == 0) {
        int fwhit = 0;
        for (thing_t *t = ch->base.roomp->base.stuff_head; t; t = t->stuff_next) {
            if (t->kind != THING_PC && t->kind != THING_MOB)
                continue;
            being_t *occ = (being_t *)t;
            if (being_is_immortal(occ))
                continue;
            being_apply_affect(occ, AFFECT_FLYING, 100);
            fwhit++;
        }
        (void)fwhit;
        descriptor_send(d, "You cast falcon wings -- everyone nearby rises gently off the ground!\r\n");
        char rmsg[128], capbuf[128];
        snprintf(rmsg, sizeof(rmsg), "%s casts falcon wings -- everyone nearby rises gently off the ground!\r\n",
                 being_display_name_cap(ch, capbuf, sizeof(capbuf)));
        descriptor_room_echo(ch->base.roomp, ch, rmsg);
    } else if (strcasecmp(sk->name, "faerie fog") == 0) {
        int fghit = 0;
        for (thing_t *t = ch->base.roomp->base.stuff_head; t; t = t->stuff_next) {
            if (t->kind != THING_MOB)
                continue;
            being_apply_affect((being_t *)t, AFFECT_BLIND, 20 * COMBAT_ROUND_PULSES);
            fghit++;
        }
        (void)fghit;
        descriptor_send(d, "You cast faerie fog -- an illusory mist rolls through the room, blinding hostile eyes!\r\n");
        char rmsg[160], capbuf[128];
        snprintf(rmsg, sizeof(rmsg), "%s casts faerie fog -- an illusory mist rolls through the room!\r\n",
                 being_display_name_cap(ch, capbuf, sizeof(capbuf)));
        descriptor_room_echo(ch->base.roomp, ch, rmsg);
    } else if (strcasecmp(sk->name, "cloud of concealment") == 0) {
        int cchit = 0;
        for (thing_t *t = ch->base.roomp->base.stuff_head; t; t = t->stuff_next) {
            if (t->kind != THING_PC)
                continue;
            being_t *occ = (being_t *)t;
            if (being_is_immortal(occ))
                continue;
            being_apply_affect(occ, AFFECT_INVISIBLE, 60);
            cchit++;
        }
        (void)cchit;
        descriptor_send(d, "You cast cloud of concealment -- a thick, obscuring mist rolls through the room!\r\n");
        char rmsg[160], capbuf[128];
        snprintf(rmsg, sizeof(rmsg), "%s casts cloud of concealment -- a thick, obscuring mist rolls through the room!\r\n",
                 being_display_name_cap(ch, capbuf, sizeof(capbuf)));
        descriptor_room_echo(ch->base.roomp, ch, rmsg);
    } else if (strcasecmp(sk->name, "stealth") == 0) {
        int sthit = 0;
        for (thing_t *t = ch->base.roomp->base.stuff_head; t; t = t->stuff_next) {
            if (t->kind != THING_PC && t->kind != THING_MOB)
                continue;
            being_t *occ = (being_t *)t;
            if (being_is_immortal(occ))
                continue;
            occ->sneaking = true;
            sthit++;
        }
        (void)sthit;
        descriptor_send(d, "You cast stealth -- everyone nearby's footsteps fall silent!\r\n");
        char rmsg[128], capbuf[128];
        snprintf(rmsg, sizeof(rmsg), "%s casts stealth -- everyone nearby's footsteps fall silent!\r\n",
                 being_display_name_cap(ch, capbuf, sizeof(capbuf)));
        descriptor_room_echo(ch->base.roomp, ch, rmsg);
    } else if (strcasecmp(sk->name, "inferno") == 0) {
        if (!atk_target) {
            descriptor_send(d, "Cast that at whom?\r\n");
            return;
        }
        if (!ch->fighting) {
            ch->fighting = atk_target;
            atk_target->fighting = ch;
            being_set_wait(ch, COMBAT_ROUND_PULSES);
        }
        int infdmg = spell_damage_for_level(sk->min_level);
        limb_t inflimb = (limb_t)(rand() % LIMB_REAL_COUNT);
        int inflimb_hp_before = atk_target->limbs[inflimb].hp;
        bool infdefeated = combat_apply_skill_damage(ch, atk_target, infdmg, inflimb);
        const char *infintensity = describe_dam(infdmg, inflimb_hp_before, NULL);
        snprintf(msg, sizeof(msg), "You cast inferno -- flames engulf %s, scorching them %s!\r\n",
                 being_display_name(atk_target), infintensity);
        descriptor_send(d, msg);
        if (!infdefeated && atk_target->desc) {
            char tcapbuf[128];
            snprintf(msg, sizeof(msg), "%s casts inferno -- flames engulf you, scorching you %s!\r\n",
                     being_display_name_cap(ch, tcapbuf, sizeof(tcapbuf)), infintensity);
            descriptor_notify(atk_target->desc, msg);
        }
    } else if (strcasecmp(sk->name, "protection from earth") == 0) {
        int pehit = 0;
        for (thing_t *t = ch->base.roomp->base.stuff_head; t; t = t->stuff_next) {
            if (t->kind != THING_PC && t->kind != THING_MOB)
                continue;
            being_t *occ = (being_t *)t;
            if (being_is_immortal(occ))
                continue;
            being_apply_affect(occ, AFFECT_PROTECTION, 60);
            pehit++;
        }
        (void)pehit;
        descriptor_send(d, "You cast protection from earth -- a warding shimmer settles over the room!\r\n");
        char rmsg[160], capbuf[128];
        snprintf(rmsg, sizeof(rmsg), "%s casts protection from earth -- a warding shimmer settles over the room!\r\n",
                 being_display_name_cap(ch, capbuf, sizeof(capbuf)));
        descriptor_room_echo(ch->base.roomp, ch, rmsg);
    } else if (strcasecmp(sk->name, "beast soother") == 0) {
        /* Level-5 stub-audit fix: "Calms a hostile or hunting animal" --
         * real ceasefire, not flavor text. Refuses a PC target (nothing
         * to calm there); ends the mob's CURRENT fight if it's fighting
         * anyone at all (not just ch -- "calms a hostile animal" isn't
         * scoped to only animals attacking the caster), and applies
         * AFFECT_CALMED so mob_try_aggress() (mob_ai.c) leaves it alone
         * for the duration instead of immediately re-aggroing next
         * tick. */
        if (!atk_target) {
            descriptor_send(d, "Cast that at whom?\r\n");
            return;
        }
        if (atk_target->base.kind != THING_MOB) {
            descriptor_send(d, "That's not a wild animal.\r\n");
            return;
        }
        being_t *other = atk_target->fighting;
        if (other) {
            atk_target->fighting = NULL;
            other->fighting = NULL;
        }
        being_apply_affect(atk_target, AFFECT_CALMED, 30 * COMBAT_ROUND_PULSES);
        snprintf(msg, sizeof(msg), "You cast beast soother -- %s grows calm and still.\r\n",
                 being_display_name(atk_target));
        descriptor_send(d, msg);
        char rmsg[160], capbuf[128];
        snprintf(rmsg, sizeof(rmsg), "%s casts beast soother over %s, and it grows calm and still.\r\n",
                 being_display_name_cap(ch, capbuf, sizeof(capbuf)), being_display_name(atk_target));
        descriptor_room_echo(ch->base.roomp, ch, rmsg);
    } else if (strcasecmp(sk->name, "silence") == 0) {
        if (!atk_target) {
            descriptor_send(d, "Cast that at whom?\r\n");
            return;
        }
        if (!ch->fighting) {
            ch->fighting = atk_target;
            atk_target->fighting = ch;
            being_set_wait(ch, COMBAT_ROUND_PULSES);
        }
        being_apply_affect(atk_target, AFFECT_SILENCE, 30 * COMBAT_ROUND_PULSES);
        snprintf(msg, sizeof(msg), "You cast silence -- %s's voice dies in %s throat!\r\n",
                 being_display_name(atk_target), gender_possess(atk_target->gender));
        descriptor_send(d, msg);
        if (atk_target->desc) {
            char tcapbuf[128];
            snprintf(msg, sizeof(msg), "%s casts silence -- your voice dies in your throat!\r\n",
                     being_display_name_cap(ch, tcapbuf, sizeof(tcapbuf)));
            descriptor_notify(atk_target->desc, msg);
        }
    } else if (strcasecmp(sk->name, "suffocate") == 0) {
        if (!atk_target) {
            descriptor_send(d, "Cast that at whom?\r\n");
            return;
        }
        if (!ch->fighting) {
            ch->fighting = atk_target;
            atk_target->fighting = ch;
            being_set_wait(ch, COMBAT_ROUND_PULSES);
        }
        int sfdmg = spell_damage_for_level(sk->min_level);
        limb_t sflimb = (limb_t)(rand() % LIMB_REAL_COUNT);
        int sflimb_hp_before = atk_target->limbs[sflimb].hp;
        bool sfdefeated = combat_apply_skill_damage(ch, atk_target, sfdmg, sflimb);
        const char *sfintensity = describe_dam(sfdmg, sflimb_hp_before, NULL);
        snprintf(msg, sizeof(msg), "You cast suffocate -- %s's lungs burn for air %s!\r\n",
                 being_display_name(atk_target), sfintensity);
        descriptor_send(d, msg);
        if (!sfdefeated && atk_target->desc) {
            char tcapbuf[128];
            snprintf(msg, sizeof(msg), "%s casts suffocate -- your lungs burn for air %s!\r\n",
                     being_display_name_cap(ch, tcapbuf, sizeof(tcapbuf)), sfintensity);
            descriptor_notify(atk_target->desc, msg);
        }
    } else if (strcasecmp(sk->name, "fumble") == 0) {
        if (!atk_target) {
            descriptor_send(d, "Cast that at whom?\r\n");
            return;
        }
        if (!ch->fighting) {
            ch->fighting = atk_target;
            atk_target->fighting = ch;
            being_set_wait(ch, COMBAT_ROUND_PULSES);
        }
        int fprimary = atk_target->handed_right ? 0 : 1;
        int fsecondary = atk_target->handed_right ? 1 : 0;
        obj_t *fweapon = NULL;
        if (atk_target->held[fprimary] && atk_target->held[fprimary]->category == OBJ_CAT_WEAPON)
            fweapon = atk_target->held[fprimary];
        else if (atk_target->held[fsecondary] && atk_target->held[fsecondary]->category == OBJ_CAT_WEAPON)
            fweapon = atk_target->held[fsecondary];
        if (!fweapon) {
            snprintf(msg, sizeof(msg), "%s isn't even holding a weapon!\r\n", being_display_name(atk_target));
            descriptor_send(d, msg);
            return;
        }
        for (int i = 0; i < 2; i++)
            if (atk_target->held[i] == fweapon)
                atk_target->held[i] = NULL;
        thing_move_to(&fweapon->base, &atk_target->base.roomp->base);
        if (atk_target->base.kind == THING_PC)
            player_inventory_save(atk_target->player_id, atk_target);
        const char *flabel = fweapon->base.short_descr[0] ? fweapon->base.short_descr : fweapon->base.name;
        char fmsg[256];
        snprintf(fmsg, sizeof(fmsg), "You cast fumble -- %s's fingers spasm, and %s clatters to the floor!\r\n",
                 being_display_name(atk_target), flabel);
        descriptor_send(d, fmsg);
        if (atk_target->desc) {
            char tcapbuf[128];
            snprintf(fmsg, sizeof(fmsg), "%s casts fumble -- your fingers spasm, and %s clatters to the floor!\r\n",
                     being_display_name_cap(ch, tcapbuf, sizeof(tcapbuf)), flabel);
            descriptor_notify(atk_target->desc, fmsg);
        }
    } else if (strcasecmp(sk->name, "immobilize") == 0) {
        if (!atk_target) {
            descriptor_send(d, "Cast that at whom?\r\n");
            return;
        }
        if (!ch->fighting) {
            ch->fighting = atk_target;
            atk_target->fighting = ch;
            being_set_wait(ch, COMBAT_ROUND_PULSES);
        }
        if (dufali_resists(atk_target)) {
            snprintf(msg, sizeof(msg), "You cast immobilize, but %s shrugs the magic off through sheer discipline!\r\n",
                     being_display_name(atk_target));
            descriptor_send(d, msg);
            if (atk_target->desc)
                descriptor_notify(atk_target->desc, "You feel a binding magic take hold, but shrug it off through sheer discipline!\r\n");
            return;
        }
        being_apply_affect(atk_target, AFFECT_BIND, 30 * COMBAT_ROUND_PULSES);
        snprintf(msg, sizeof(msg), "You cast immobilize -- %s is rooted to the spot!\r\n",
                 being_display_name(atk_target));
        descriptor_send(d, msg);
        if (atk_target->desc) {
            char tcapbuf[128];
            snprintf(msg, sizeof(msg), "%s casts immobilize -- you're rooted to the spot!\r\n",
                     being_display_name_cap(ch, tcapbuf, sizeof(tcapbuf)));
            descriptor_notify(atk_target->desc, msg);
        }
    } else if (ci_contains(sk->name, "garmul")) {
        if (!atk_target) {
            descriptor_send(d, "Cast that at whom?\r\n");
            return;
        }
        if (!ch->fighting) {
            ch->fighting = atk_target;
            atk_target->fighting = ch;
            being_set_wait(ch, COMBAT_ROUND_PULSES);
        }
        bool stripped = false;
        if (being_has_affect(atk_target, AFFECT_WATERBREATH)) {
            being_remove_affect(atk_target, AFFECT_WATERBREATH);
            stripped = true;
        }
        if (being_has_affect(atk_target, AFFECT_FLYING)) {
            being_remove_affect(atk_target, AFFECT_FLYING);
            stripped = true;
        }
        (void)stripped;
        snprintf(msg, sizeof(msg), "You cast %s -- %s's legs feel heavy and clumsy, like a fish out of water!\r\n",
                 sk->name, being_display_name(atk_target));
        descriptor_send(d, msg);
        if (atk_target->desc) {
            char tcapbuf[128];
            snprintf(msg, sizeof(msg), "%s casts %s -- your legs feel heavy and clumsy!\r\n",
                     being_display_name_cap(ch, tcapbuf, sizeof(tcapbuf)), sk->name);
            descriptor_notify(atk_target->desc, msg);
        }
    } else if (strcasecmp(sk->name, "faerie fire") == 0) {
        /* Level-6 stub-audit fix: "Marks a target with a pink aura,
         * easier to hit" -- opens combat the same way the plain damage
         * branch does (this is an offensive debuff, TAR_VIOLENT in
         * spirit), applies AFFECT_FAERIE_FIRE (combat.c's strike roll
         * widens the defender-side to-hit modifier while it's active). */
        if (!atk_target) {
            descriptor_send(d, "Cast that at whom?\r\n");
            return;
        }
        if (!ch->fighting) {
            ch->fighting = atk_target;
            atk_target->fighting = ch;
            being_set_wait(ch, COMBAT_ROUND_PULSES);
        }
        being_apply_affect(atk_target, AFFECT_FAERIE_FIRE, 30 * COMBAT_ROUND_PULSES);
        snprintf(msg, sizeof(msg), "You cast faerie fire -- a shimmering pink aura clings to %s!\r\n",
                 being_display_name(atk_target));
        descriptor_send(d, msg);
        if (atk_target->desc) {
            char tcapbuf[128];
            snprintf(msg, sizeof(msg), "%s casts faerie fire -- a shimmering pink aura clings to you!\r\n",
                     being_display_name_cap(ch, tcapbuf, sizeof(tcapbuf)));
            descriptor_notify(atk_target->desc, msg);
        }
    } else if (strcasecmp(sk->name, "feathery descent") == 0) {
        /* Level-7 stub-audit fix: "A group buff that softens falls" --
         * real room-wide AFFECT_FEATHERY_DESCENT (fall.c checks it
         * alongside the `catfall` skill: wider survivable-depth
         * threshold, halved crush damage), same room-walk-loop shape as
         * sorcerer's globe/flare above. */
        int fdhit = 0;
        for (thing_t *t = ch->base.roomp->base.stuff_head; t; t = t->stuff_next) {
            if (t->kind != THING_PC && t->kind != THING_MOB)
                continue;
            being_t *occ = (being_t *)t;
            if (being_is_immortal(occ))
                continue;
            being_apply_affect(occ, AFFECT_FEATHERY_DESCENT, 30 * COMBAT_ROUND_PULSES);
            fdhit++;
        }
        (void)fdhit;
        descriptor_send(d, "You cast feathery descent -- everyone nearby feels lighter than air!\r\n");
        char rmsg[128], capbuf[128];
        snprintf(rmsg, sizeof(rmsg), "%s casts feathery descent -- everyone nearby feels lighter than air!\r\n",
                 being_display_name_cap(ch, capbuf, sizeof(capbuf)));
        descriptor_room_echo(ch->base.roomp, ch, rmsg);
    } else if (strcasecmp(sk->name, "entangling roots") == 0) {
        /* Level-1 stub-audit fix: Druid's signature level-1 attack --
         * roster text "only works outdoors" needed its own explicit
         * branch (the generic damage branch below has no outdoor gate),
         * and its desc says "damaging" not "damage" so it would have
         * missed that branch anyway (see the broadened ci_contains()
         * stem match just below, which also independently fixes several
         * other "damaging"/"damages"-worded stubs). The outdoor check
         * itself lives in cmd_cast()'s outer dispatcher, BEFORE the
         * component is consumed -- see that check's own comment for why
         * (a refusal here would still cost the caster their component,
         * a pre-existing gap in how every task_cast() early-return
         * interacts with the caller's unconditional post-call
         * consume_component()). */
        if (!atk_target) {
            descriptor_send(d, "Cast that at whom?\r\n");
            return;
        }
        if (!ch->fighting) {
            ch->fighting = atk_target;
            atk_target->fighting = ch;
            being_set_wait(ch, COMBAT_ROUND_PULSES);
        }
        int dmg = spell_damage_for_level(sk->min_level);
        limb_t limb = (limb_t)(rand() % LIMB_REAL_COUNT);
        int limb_hp_before = atk_target->limbs[limb].hp;
        bool defeated = combat_apply_skill_damage(ch, atk_target, dmg, limb);
        const char *intensity = describe_dam(dmg, limb_hp_before, NULL);
        snprintf(msg, sizeof(msg), "You cast entangling roots -- roots erupt underfoot, tripping and mauling %s %s!\r\n",
                 being_display_name(atk_target), intensity);
        descriptor_send(d, msg);
        if (!defeated && atk_target->desc) {
            char tcapbuf[128];
            snprintf(msg, sizeof(msg), "%s casts entangling roots -- roots erupt underfoot, tripping and mauling you %s!\r\n",
                     being_display_name_cap(ch, tcapbuf, sizeof(tcapbuf)), intensity);
            descriptor_notify(atk_target->desc, msg);
        }
    } else if (strcasecmp(sk->name, "root control") == 0) {
        /* Root control (Druid batch, Tier 2 port 2026-08-16). Real
         * upstream is a Shaman-spider spell (disc_shaman_spider.cc's
         * rootControl()): "Large tree roots start to form in front of
         * $N", then on success the caster "commands the tree roots to
         * trip $N, causing $M to fall to the $g" -- setPosition(SITTING)
         * + addToWait(combatRound(1)) + reconcileDamage(). Ported as a
         * genuine knockdown-plus-damage attack: unlike sibling
         * `entangling roots` (which only damages), this ALSO drops the
         * victim to POSITION_SITTING and costs them a combat round --
         * the exact real crowd-control mechanic Tobin's own `trip`
         * skill uses (cmd_trip.c: spellcast_distract + position +
         * being_set_wait), the distinguishing effect that made root
         * control a real port gap rather than a duplicate of entangling
         * roots. Outdoor-gated ("in nature or on land") in the outer
         * dispatcher, same as entangling roots/living vines. */
        if (!atk_target) {
            descriptor_send(d, "Cast that at whom?\r\n");
            return;
        }
        if (!ch->fighting) {
            ch->fighting = atk_target;
            atk_target->fighting = ch;
            being_set_wait(ch, COMBAT_ROUND_PULSES);
        }
        int dmg = spell_damage_for_level(sk->min_level);
        limb_t limb = (limb_t)(rand() % LIMB_REAL_COUNT);
        int limb_hp_before = atk_target->limbs[limb].hp;
        bool defeated = combat_apply_skill_damage(ch, atk_target, dmg, limb);
        const char *intensity = describe_dam(dmg, limb_hp_before, NULL);
        snprintf(msg, sizeof(msg), "You command tree roots to erupt and trip %s -- they crash to the ground %s!\r\n",
                 being_display_name(atk_target), intensity);
        descriptor_send(d, msg);
        if (!defeated) {
            /* Knockdown half: same real mechanic as `trip` (cmd_trip.c) --
             * rattle a mid-cast target, put them on the ground, and cost
             * them a round. Skipped if the damage already defeated them. */
            spellcast_distract(atk_target, 1);
            atk_target->position = POSITION_SITTING;
            being_set_wait(atk_target, COMBAT_ROUND_PULSES);
            if (atk_target->desc) {
                char tcapbuf[128];
                snprintf(msg, sizeof(msg), "%s commands tree roots to erupt and trip you -- you crash to the ground %s!\r\n",
                         being_display_name_cap(ch, tcapbuf, sizeof(tcapbuf)), intensity);
                descriptor_notify(atk_target->desc, msg);
            }
        }
    } else if (strcasecmp(sk->name, "shapeshift") == 0) {
        /* Shapeshift (Druid batch, Tier-2 port 2026-08-16). Real upstream
         * (disc_shaman_frog.cc's shapeShift()) melts the caster's flesh
         * into an animal form for a level-scaled duration -- mechanically
         * the SAME descriptor-swap as polymorph (swap d->character to a
         * temporary mob body, revert on expiry), just Druid-flavored and
         * with a form chosen by the caster's power rather than a fixed
         * bear. Reuses being_start_polymorph() exactly like the polymorph
         * branch above (AFFECT_POLYMORPH auto-reverts). The form scales
         * with caster level across a short table of real seeded animal
         * mobs (rat/wolf/hawk/lion/dire wolf/brown bear), mirroring
         * upstream's own level-gated ShapeShiftList[] rather than letting
         * a low-level Druid wear a dire form. */
        room_t *room = ch->base.roomp;
        char capbuf[128];
        being_display_name_cap(ch, capbuf, sizeof(capbuf));
        int lvl = ch->progress.level;
        int form_vnum; const char *form_name;
        if (lvl < 10)      { form_vnum = 129; form_name = "a good-sized rat"; }
        else if (lvl < 20) { form_vnum = 570; form_name = "a fierce gray wolf"; }
        else if (lvl < 30) { form_vnum = 573; form_name = "a brownish-gold hawk"; }
        else if (lvl < 40) { form_vnum = 577; form_name = "a young lion"; }
        else if (lvl < 50) { form_vnum = 583; form_name = "a large dire wolf"; }
        else               { form_vnum = 585; form_name = "a brown bear"; }
        if (!being_start_polymorph(d, form_vnum, TRANSFORM_DURATION_ROUNDS)) {
            descriptor_send(d, "You reach for a wild shape, but the change won't take hold.\r\n");
            return;
        }
        snprintf(msg, sizeof(msg), "Your flesh turns liquid and flows into a new shape -- you are now %s!\r\n", form_name);
        descriptor_send(d, msg);
        if (room) {
            snprintf(msg, sizeof(msg), "%s's flesh melts and flows into the shape of %s!\r\n", capbuf, form_name);
            descriptor_room_echo(room, NULL, msg);
        }
    } else if (strcasecmp(sk->name, "transfix") == 0) {
        /* Transfix (Druid batch, Tier-2 port 2026-08-16). Real upstream
         * (disc_shaman_spider.cc's transfix()) mesmerizes a dumb animal
         * that isn't already fighting -- "$N stares transfixed into your
         * eyes" -- holding it frozen and staring. Ported as AFFECT_TRANSFIX
         * (affect.h), a real "can't act" hold gated in cmd_attack.c (the
         * target can't initiate an attack) and mob_ai.c (a transfixed mob
         * won't aggress). Upstream refuses a target that is already
         * fighting (!victim->fight()); kept faithfully. The dumb-animal-
         * only restriction is dropped (Tobin has no such flag) -- see
         * AFFECT_TRANSFIX's own doc comment. */
        if (!atk_target) {
            descriptor_send(d, "Transfix whom?\r\n");
            return;
        }
        if (being_is_immortal(atk_target)) {
            descriptor_send(d, "A god pays no attention to your gaze.\r\n");
            return;
        }
        if (iron_will_resists(d, ch, atk_target, "mesmerising gaze"))
            return;
        if (atk_target->fighting || ch->fighting) {
            descriptor_send(d, "It's far too agitated to be mesmerized right now.\r\n");
            return;
        }
        if (being_has_affect(atk_target, AFFECT_TRANSFIX)) {
            snprintf(msg, sizeof(msg), "%s is already transfixed.\r\n", being_display_name(atk_target));
            descriptor_send(d, msg);
            return;
        }
        being_apply_affect(atk_target, AFFECT_TRANSFIX, 20 + sk->min_level);
        snprintf(msg, sizeof(msg), "You catch %s's gaze -- it stares back, transfixed, and stands frozen in place!\r\n",
                 being_display_name(atk_target));
        descriptor_send(d, msg);
        if (atk_target->desc) {
            char tcapbuf[128];
            snprintf(msg, sizeof(msg), "%s catches your gaze -- you stare, transfixed, unable to look away!\r\n",
                     being_display_name_cap(ch, tcapbuf, sizeof(tcapbuf)));
            descriptor_notify(atk_target->desc, msg);
        }
    } else if (strcasecmp(sk->name, "creeping doom") == 0) {
        /* Creeping doom (Druid batch, Tier-2 port 2026-08-16). Real
         * upstream leaves SPELL_CREEPING_DOOM with no concrete cast
         * handler (skills.cc registers the discipline slot, but
         * disc_shaman_frog.cc has no creepingDoom() body -- it was never
         * finished in Sneezy itself), so there's no exact formula to
         * copy. Ported faithfully-in-spirit to the name: a summoned
         * swarm of biting, venomous insects that bites once for damage
         * and then keeps eating away -- an initial hit plus AFFECT_POISON
         * (affect.c's own HP-drain-over-time affect), the closest real
         * "creeping doom" mechanic Tobin already has. */
        if (!atk_target) {
            descriptor_send(d, "Loose the swarm upon whom?\r\n");
            return;
        }
        if (!ch->fighting) {
            ch->fighting = atk_target;
            atk_target->fighting = ch;
            being_set_wait(ch, COMBAT_ROUND_PULSES);
        }
        int dmg = spell_damage_for_level(sk->min_level);
        limb_t limb = (limb_t)(rand() % LIMB_REAL_COUNT);
        int limb_hp_before = atk_target->limbs[limb].hp;
        bool defeated = combat_apply_skill_damage(ch, atk_target, dmg, limb);
        const char *intensity = describe_dam(dmg, limb_hp_before, NULL);
        snprintf(msg, sizeof(msg), "You call forth a creeping doom -- a swarm of venomous insects engulfs %s %s!\r\n",
                 being_display_name(atk_target), intensity);
        descriptor_send(d, msg);
        if (!defeated) {
            being_apply_affect(atk_target, AFFECT_POISON, 12 * COMBAT_ROUND_PULSES);
            if (atk_target->desc) {
                char tcapbuf[128];
                snprintf(msg, sizeof(msg), "%s calls forth a creeping doom -- venomous insects swarm over you %s, and their bites fester!\r\n",
                         being_display_name_cap(ch, tcapbuf, sizeof(tcapbuf)), intensity);
                descriptor_notify(atk_target->desc, msg);
            }
        }
    } else if (strcasecmp(sk->name, "stormy skies") == 0) {
        /* Stormy skies (Druid batch, Tier-2 port 2026-08-16). Real
         * upstream (disc_shaman_frog.cc's stormySkies()) only works
         * outdoors under RAINY/LIGHTNING/SNOWY weather, calling a
         * lightning bolt (or hail) down on the victim for damage. Tobin's
         * weather (weather.h) is a single world-wide sky state; the
         * outdoor + RAINY/STORMY gate lives in the outer dispatcher
         * (before the component is consumed, same pattern as entangling
         * roots' outdoor gate), so by the time this branch runs the sky
         * is already known to be stormy. Pure level-scaled storm damage,
         * same combat_apply_skill_damage() shape as the other Druid
         * attack spells. */
        if (!atk_target) {
            descriptor_send(d, "Call the storm down on whom?\r\n");
            return;
        }
        if (!ch->fighting) {
            ch->fighting = atk_target;
            atk_target->fighting = ch;
            being_set_wait(ch, COMBAT_ROUND_PULSES);
        }
        int dmg = spell_damage_for_level(sk->min_level);
        limb_t limb = (limb_t)(rand() % LIMB_REAL_COUNT);
        int limb_hp_before = atk_target->limbs[limb].hp;
        bool defeated = combat_apply_skill_damage(ch, atk_target, dmg, limb);
        const char *intensity = describe_dam(dmg, limb_hp_before, NULL);
        snprintf(msg, sizeof(msg), "You summon a lightning bolt from the stormy skies and call it down on %s %s!\r\n",
                 being_display_name(atk_target), intensity);
        descriptor_send(d, msg);
        if (!defeated && atk_target->desc) {
            char tcapbuf[128];
            snprintf(msg, sizeof(msg), "%s summons a lightning bolt from the stormy skies -- it strikes you %s!\r\n",
                     being_display_name_cap(ch, tcapbuf, sizeof(tcapbuf)), intensity);
            descriptor_notify(atk_target->desc, msg);
        }
    } else if (strcasecmp(sk->name, "bramble drain") == 0) {
        /* Level-3 stub-audit fix: "A thorned vine that drains a small
         * amount of life to you" -- real damage + a life-drain heal-
         * back, same being-target/combat_apply_skill_damage() shape as
         * entangling roots just above, plus being_heal() for the "to
         * you" half (a genuine drain, not just damage with drain
         * flavor text and no actual transfer). Heal-back is capped at
         * half the damage dealt -- "a SMALL amount of life", not a
         * full 1:1 transfer. */
        if (!atk_target) {
            descriptor_send(d, "Cast that at whom?\r\n");
            return;
        }
        if (!ch->fighting) {
            ch->fighting = atk_target;
            atk_target->fighting = ch;
            being_set_wait(ch, COMBAT_ROUND_PULSES);
        }
        int dmg = spell_damage_for_level(sk->min_level);
        limb_t limb = (limb_t)(rand() % LIMB_REAL_COUNT);
        int limb_hp_before = atk_target->limbs[limb].hp;
        bool defeated = combat_apply_skill_damage(ch, atk_target, dmg, limb);
        const char *intensity = describe_dam(dmg, limb_hp_before, NULL);
        int drained = dmg / 2;
        if (drained < 1)
            drained = 1;
        being_heal(ch, drained);
        snprintf(msg, sizeof(msg),
                 "You cast bramble drain -- thorned vines lash %s %s, and you feel their life flow into you! (+%d HP)\r\n",
                 being_display_name(atk_target), intensity, drained);
        descriptor_send(d, msg);
        if (!defeated && atk_target->desc) {
            char tcapbuf[128];
            snprintf(msg, sizeof(msg), "%s casts bramble drain -- thorned vines lash you %s, draining your life away!\r\n",
                     being_display_name_cap(ch, tcapbuf, sizeof(tcapbuf)), intensity);
            descriptor_notify(atk_target->desc, msg);
        }
    } else if (ci_contains(desc, "damage") || ci_contains(desc, "damag")
               || ci_contains(desc, "bolt")
               || ci_contains(desc, "beam") || ci_contains(desc, "blast")
               || ci_contains(desc, "strike") || ci_contains(desc, "burst")
               || ci_contains(desc, "fury") || ci_contains(desc, "flame")
               /* `hands of flame` (level-4 stub-audit fix): desc says
                * "fiery" not "flame", missing this branch entirely
                * despite "flame" already being a tested keyword. */
               || ci_contains(desc, "fiery")
               /* `shatter` (level-27) / `watery grave` (level-32) stub-
                * audit fixes: their own desc words aren't in the list
                * above at all. */
               || ci_contains(desc, "shatter") || ci_contains(desc, "drowning")) {
        /* No longer gated on ch->fighting (breadth work) -- a spell can
         * now OPEN combat against atk_target, same as `attack`/`kill`,
         * rather than only ever being usable on whoever you're already
         * fighting. If ch is already fighting someone ELSE, this is
         * just a one-off supplemental hit -- the existing fight isn't
         * disturbed (Tobin's `fighting` is strictly 1v1). */
        if (!atk_target) {
            descriptor_send(d, "Cast that at whom?\r\n");
            return;
        }
        if (!ch->fighting) {
            ch->fighting = atk_target;
            atk_target->fighting = ch;
            being_set_wait(ch, COMBAT_ROUND_PULSES);
        }
        int dmg = spell_damage_for_level(sk->min_level);
        /* LIMB_REAL_COUNT, not LIMB_COUNT (Limbs -> wearSlotT, 2026-07-26)
         * -- excludes the mob-only, always-inactive EX_* placeholder
         * slots from ever being randomly hit. */
        limb_t limb = (limb_t)(rand() % LIMB_REAL_COUNT);
        int limb_hp_before = atk_target->limbs[limb].hp;
        bool defeated = combat_apply_skill_damage(ch, atk_target, dmg, limb);
        /* Damage numbers (user 2026-07-12, follow-up "take out the
         * damage number and use it to describe how hard the hit was"):
         * same describe_dam() treatment as combat.c's melee messages,
         * shown to every viewer now, not just immortals. */
        const char *intensity = describe_dam(dmg, limb_hp_before, NULL);
        snprintf(msg, sizeof(msg), "You cast %s at %s, striking them %s!\r\n",
                 sk->name, being_display_name(atk_target), intensity);
        descriptor_send(d, msg);
        if (!defeated && atk_target->desc) {
            char tcapbuf[128];
            snprintf(msg, sizeof(msg), "%s casts %s at you, striking you %s!\r\n",
                     being_display_name_cap(ch, tcapbuf, sizeof(tcapbuf)), sk->name, intensity);
            descriptor_notify(atk_target->desc, msg);
        }
    } else if (strcasecmp(sk->name, "ensorcer") == 0) {
        /* Level-15 stub-audit fix: "Charms or dominates a target" --
         * real charm on an EXISTING mob (not a fresh summon like
         * being_summon_charmed_pet() does for `summon` prayers/pets),
         * same refusal shape: only one charmed pet at a time, only if a
         * follower slot is free. Refuses a PC target outright (no
         * domination-of-players mechanic exists, and real upstream
         * charm spells don't work on other players either). */
        if (!atk_target) {
            descriptor_send(d, "Cast that at whom?\r\n");
            return;
        }
        if (atk_target->base.kind != THING_MOB) {
            descriptor_send(d, "You can only ensorcer a wild creature.\r\n");
            return;
        }
        if (being_find_charmed_pet(ch)) {
            descriptor_send(d, "You already have a charmed minion -- you can't control another.\r\n");
            return;
        }
        if (being_has_affect(atk_target, AFFECT_CHARMED)) {
            descriptor_send(d, "That creature is already charmed by someone else.\r\n");
            return;
        }
        int slot = -1;
        for (int i = 0; i < GROUP_MAX_FOLLOWERS; i++) {
            if (!ch->followers[i]) {
                slot = i;
                break;
            }
        }
        if (slot < 0) {
            descriptor_send(d, "You already have too many followers to take on another.\r\n");
            return;
        }
        if (being_race_resists(atk_target, RESIST_CHARM)) {
            snprintf(msg, sizeof(msg), "You cast ensorcer at %s, but its will proves too strong to bend!\r\n",
                     being_display_name(atk_target));
            descriptor_send(d, msg);
            if (atk_target->desc)
                descriptor_notify(atk_target->desc, "A foreign will claws at your mind, but your kind throws it off!\r\n");
            return;
        }
        ch->followers[slot] = atk_target;
        atk_target->master = ch;
        being_apply_affect(atk_target, AFFECT_CHARMED, 60 * COMBAT_ROUND_PULSES);
        snprintf(msg, sizeof(msg), "You cast ensorcer -- %s's eyes glaze over, and it bends to your will!\r\n",
                 being_display_name(atk_target));
        descriptor_send(d, msg);
        char rmsg[160], capbuf[128];
        snprintf(rmsg, sizeof(rmsg), "%s casts ensorcer over %s, and it bends to their will!\r\n",
                 being_display_name_cap(ch, capbuf, sizeof(capbuf)), being_display_name(atk_target));
        descriptor_room_echo(ch->base.roomp, ch, rmsg);
    } else if (ci_contains(sk->name, "stupidity")) {
        /* Full spell/skill/prayer roster import, Druid's 6 named Shaman
         * spells (user 2026-07-26) -- ported from disc_shaman.cc's real
         * stupidity(): a level-scaled INTELLIGENCE penalty
         * (aff.modifier = -(level/4) there), the first stat-modifying
         * affect Tobin has (being_apply_stat_affect(), affect.c). Opens
         * combat the same way the plain damage branch above does --
         * this is TAR_VIOLENT in the original too, not a buff. */
        if (!atk_target) {
            descriptor_send(d, "Cast that at whom?\r\n");
            return;
        }
        if (!ch->fighting) {
            ch->fighting = atk_target;
            atk_target->fighting = ch;
            being_set_wait(ch, COMBAT_ROUND_PULSES);
        }
        int penalty = sk->min_level / 4;
        if (penalty < 1)
            penalty = 1;
        /* 100 rounds -- same "several real minutes" duration
         * AFFECT_WATERBREATH/AFFECT_FLYING already established for a
         * non-combat-bound utility-ish buff/debuff, standing in for the
         * original's real "10 mud-hours / 2" duration at a comparable
         * scale rather than a literal mudhour-to-round conversion. */
        being_apply_stat_affect(atk_target, AFFECT_STUPIDITY, 100, -penalty);
        snprintf(msg, sizeof(msg), "You cast %s at %s -- their eyes glaze over stupidly!\r\n",
                 sk->name, being_display_name(atk_target));
        descriptor_send(d, msg);
        if (atk_target->desc) {
            char tcapbuf[128];
            snprintf(msg, sizeof(msg), "%s casts %s at you -- your mind fogs with stupidity!\r\n",
                     being_display_name_cap(ch, tcapbuf, sizeof(tcapbuf)), sk->name);
            descriptor_notify(atk_target->desc, msg);
        }
    } else if (strcasecmp(sk->name, "fear") == 0) {
        /* Full spell/skill/prayer roster import continued, level-5+ list
         * (2026-07-27): real upstream (disc_mage_spirit.cc's fear())
         * forces an immediate flee, then a lingering affect keeps
         * compelling the victim to keep running whenever it next comes
         * up. Scoped to: an immediate flee attempt (reusing cmd_flee.c's
         * own logic directly on the victim's descriptor -- only possible
         * for a PC victim, a mob has no descriptor of its own to flee
         * through) plus AFFECT_FEAR (affect.h), a plain flag/timer
         * checked by cmd_attack.c so a feared being can't turn around
         * and swing back while it's active. Deliberately NOT ported: the
         * "isLucky" resist-and-fizzle branch (the outer cast-proficiency
         * roll already stands in for it) and the crit-fail-fears-the-
         * caster-instead branch. */
        if (!atk_target) {
            descriptor_send(d, "Cast that at whom?\r\n");
            return;
        }
        if (!ch->fighting) {
            ch->fighting = atk_target;
            atk_target->fighting = ch;
            being_set_wait(ch, COMBAT_ROUND_PULSES);
        }
        if (iron_will_resists(d, ch, atk_target, "terror"))
            return;
        snprintf(msg, sizeof(msg), "You cast %s at %s -- they're afraid! Look at them run!\r\n",
                 sk->name, being_display_name(atk_target));
        descriptor_send(d, msg);
        if (atk_target->desc) {
            char tcapbuf[128];
            snprintf(msg, sizeof(msg), "%s casts %s at you -- you are afraid of %s! Run for your life!\r\n",
                     being_display_name_cap(ch, tcapbuf, sizeof(tcapbuf)), sk->name, tcapbuf);
            descriptor_notify(atk_target->desc, msg);
        }
        being_apply_affect(atk_target, AFFECT_FEAR, 20 + sk->min_level);
        if (atk_target->desc && atk_target->fighting)
            cmd_flee(atk_target->desc, "");
    } else if (ci_contains(sk->name, "slumber")) {
        /* Full spell/skill/prayer roster import continued, level-5+ list
         * (2026-07-27): real upstream (disc_mage_spirit.cc's slumber()/
         * rawSleep()) puts the victim into POSITION_SLEEPING for a
         * timed duration, with a separate luck-save resist roll on top
         * of the normal cast-success check, plus an optional Sleep Tag
         * Staff branch and a crit-fail-hits-the-caster-instead branch.
         * Scoped to the core effect: this function only runs once the
         * outer proficiency roll (task_cast()'s caller) already
         * succeeded, so that stands in for the real version's
         * bSuccess()/luck-save pair -- no second resist roll here.
         * AFFECT_SLEEP (affect.h) auto-wakes atk_target on expiry.
         * Deliberately NOT ported: the Sleep Tag Staff item (doesn't
         * exist in Tobin) and the crit-fail-hits-caster branch. */
        if (!atk_target) {
            descriptor_send(d, "Cast that at whom?\r\n");
            return;
        }
        if (being_is_immortal(atk_target)) {
            descriptor_send(d, "You can't put an immortal to sleep.\r\n");
            return;
        }
        if (iron_will_resists(d, ch, atk_target, "drowsiness"))
            return;
        if (atk_target->position == POSITION_SLEEPING) {
            snprintf(msg, sizeof(msg), "%s is already asleep.\r\n", being_display_name(atk_target));
            descriptor_send(d, msg);
            return;
        }
        if (dufali_resists(atk_target)) {
            snprintf(msg, sizeof(msg), "You cast %s at %s, but they shrug the magic off through sheer discipline!\r\n",
                     sk->name, being_display_name(atk_target));
            descriptor_send(d, msg);
            if (atk_target->desc)
                descriptor_notify(atk_target->desc, "You feel a wave of drowsiness, but shrug it off through sheer discipline!\r\n");
            return;
        }
        if (being_race_resists(atk_target, RESIST_SLEEP)) {
            snprintf(msg, sizeof(msg), "You cast %s at %s, but their kind shrugs off the drowsiness!\r\n",
                     sk->name, being_display_name(atk_target));
            descriptor_send(d, msg);
            if (atk_target->desc)
                descriptor_notify(atk_target->desc, "A wave of drowsiness washes over you, but your kind resists it!\r\n");
            return;
        }
        atk_target->position = POSITION_SLEEPING;
        being_apply_affect(atk_target, AFFECT_SLEEP, 40 + sk->min_level);
        snprintf(msg, sizeof(msg), "You cast %s at %s -- their eyes grow heavy and they collapse into sleep!\r\n",
                 sk->name, being_display_name(atk_target));
        descriptor_send(d, msg);
        if (atk_target->desc) {
            char tcapbuf[128];
            snprintf(msg, sizeof(msg), "%s casts %s at you -- you can't fight the sudden urge to sleep!\r\n",
                     being_display_name_cap(ch, tcapbuf, sizeof(tcapbuf)), sk->name);
            descriptor_notify(atk_target->desc, msg);
        }
    } else if (strcasecmp(sk->name, "invisibility") == 0) {
        /* Spell/skill functional-completeness audit continued, level 17
         * (2026-07-27): real upstream (disc_mage_spirit.cc's
         * invisibility()) also grants a -40 armor bonus and doubles
         * both duration and the bonus on a crit success -- scoped down
         * to a plain flag/timer affect, same shape as AFFECT_SANCTUARY/
         * AFFECT_BERSERK (no armor bonus, no crit branch). Targets a
         * being like every other buff here (self by default, an ally
         * if named) -- the roster's own "yourself or an OBJECT" framing
         * for the object-target case isn't ported: Tobin's object
         * "INVISIBLE" flag (obj.c's OBJ_ACTION_FLAG_NAMES) is display-
         * only today, nothing in cmd_look.c's room listing actually
         * hides a flagged object yet, so making one truly invisible
         * would need new display plumbing beyond this pass's scope. */
        being_apply_affect(target, AFFECT_INVISIBLE, 60);
        if (target == ch) {
            snprintf(msg, sizeof(msg), "You cast %s -- you shimmer and fade from view!\r\n", sk->name);
            descriptor_send(d, msg);
            if (ch->base.roomp) {
                char capbuf[128];
                snprintf(msg, sizeof(msg), "%s shimmers and fades from view!\r\n",
                         being_display_name_cap(ch, capbuf, sizeof(capbuf)));
                descriptor_room_echo(ch->base.roomp, ch, msg);
            }
        } else {
            snprintf(msg, sizeof(msg), "You cast %s over %s -- they shimmer and fade from view!\r\n",
                     sk->name, being_display_name(target));
            descriptor_send(d, msg);
            if (target->desc) {
                char tcapbuf[128];
                snprintf(msg, sizeof(msg), "%s casts %s over you -- you shimmer and fade from view!\r\n",
                         being_display_name_cap(ch, tcapbuf, sizeof(tcapbuf)), sk->name);
                descriptor_notify(target->desc, msg);
            }
        }
    } else if (strcasecmp(sk->name, "dispel invisible") == 0) {
        /* Its counter (level 17). Real upstream (dispelInvisible())
         * gates on caster power vs. victim level and can crit-fail by
         * accidentally dispelling the CASTER's own invisibility instead
         * -- neither ported (same "no crit-fail branch" precedent as
         * fear/slumber/invisibility above; the outer proficiency roll
         * already stands in for the power-gate). Just strips
         * AFFECT_INVISIBLE from `target` if present. */
        if (being_has_affect(target, AFFECT_INVISIBLE)) {
            being_remove_affect(target, AFFECT_INVISIBLE);
            if (target == ch) {
                snprintf(msg, sizeof(msg), "You cast %s -- you slowly become visible again.\r\n", sk->name);
                descriptor_send(d, msg);
                if (ch->base.roomp) {
                    char capbuf[128];
                    snprintf(msg, sizeof(msg), "%s slowly becomes visible again.\r\n",
                             being_display_name_cap(ch, capbuf, sizeof(capbuf)));
                    descriptor_room_echo(ch->base.roomp, ch, msg);
                }
            } else {
                snprintf(msg, sizeof(msg), "You cast %s at %s -- they slowly become visible again.\r\n",
                         sk->name, being_display_name(target));
                descriptor_send(d, msg);
                if (target->desc) {
                    descriptor_notify(target->desc, "You slowly become visible again.\r\n");
                }
            }
        } else if (target == ch) {
            descriptor_send(d, "You're already visible.\r\n");
        } else {
            snprintf(msg, sizeof(msg), "%s is already visible.\r\n", being_display_name(target));
            descriptor_send(d, msg);
        }
    } else if (strcasecmp(sk->name, "haste") == 0) {
        /* Level-23 audit batch (2026-07-28). See AFFECT_HASTE's own
         * doc comment (affect.h) for the mechanic and the disclosed
         * scope-cut (single-target only, no group-wide no-target case,
         * no crit-success duration doubling). */
        being_apply_affect(target, AFFECT_HASTE, 60);
        if (target == ch) {
            snprintf(msg, sizeof(msg), "You cast %s -- you feel yourself moving with the greatest of ease!\r\n", sk->name);
            descriptor_send(d, msg);
            if (ch->base.roomp) {
                char capbuf[128];
                snprintf(msg, sizeof(msg), "%s has gained a bounce in %s step!\r\n",
                         being_display_name_cap(ch, capbuf, sizeof(capbuf)), gender_possess(ch->gender));
                descriptor_room_echo(ch->base.roomp, ch, msg);
            }
        } else {
            snprintf(msg, sizeof(msg), "You cast %s on %s -- you've given them the speed of the wind!\r\n",
                     sk->name, being_display_name(target));
            descriptor_send(d, msg);
            if (target->desc)
                descriptor_notify(target->desc, "You feel yourself moving with the greatest of ease!\r\n");
        }
    } else if (strcasecmp(sk->name, "enhance weapon") == 0 || strcasecmp(sk->name, "galvanize") == 0) {
        /* Level-24 audit batch (2026-07-29). See AFFECT_ENHANCE_WEAPON's
         * doc comment (affect.h) for the "permanent" -> temporary-buff
         * deviation and why: no per-instance objaffect slot to write a
         * one-off weapon bonus onto. Same single-target/self-or-ally
         * shape as haste, same bonus-scaling convention as rally
         * (being_apply_stat_affect()). */
        int bonus = 4 + ch->progress.level / 10;
        being_apply_stat_affect(target, AFFECT_ENHANCE_WEAPON, 60, bonus);
        if (target == ch) {
            snprintf(msg, sizeof(msg), "You cast %s -- your weapon hand feels supernaturally sure!\r\n", sk->name);
            descriptor_send(d, msg);
            if (ch->base.roomp) {
                char capbuf[128];
                snprintf(msg, sizeof(msg), "%s's weapon hand glows with a brief, unnatural certainty!\r\n",
                         being_display_name_cap(ch, capbuf, sizeof(capbuf)));
                descriptor_room_echo(ch->base.roomp, ch, msg);
            }
        } else {
            snprintf(msg, sizeof(msg), "You cast %s on %s -- their weapon hand glows with a brief, unnatural certainty!\r\n",
                     sk->name, being_display_name(target));
            descriptor_send(d, msg);
            if (target->desc)
                descriptor_notify(target->desc, "Your weapon hand feels supernaturally sure!\r\n");
        }
    } else if (strcasecmp(sk->name, "detect invisibility") == 0) {
        /* Level-25 audit batch (2026-07-29). Real message from upstream
         * (disc_mage_spirit.cc's detectInvisibility()): "Your eyes
         * tingle." / "$n's eyes briefly glow yellow." See
         * AFFECT_DETECT_INVISIBLE's own doc comment (affect.h) for the
         * real functional wiring (combat_find_room_target()/cmd_look.c). */
        being_apply_affect(target, AFFECT_DETECT_INVISIBLE, 100);
        if (target == ch) {
            descriptor_send(d, "Your eyes tingle.\r\n");
            if (ch->base.roomp) {
                char capbuf[128];
                snprintf(msg, sizeof(msg), "%s's eyes briefly glow yellow.\r\n",
                         being_display_name_cap(ch, capbuf, sizeof(capbuf)));
                descriptor_room_echo(ch->base.roomp, ch, msg);
            }
        } else {
            snprintf(msg, sizeof(msg), "You cast %s on %s -- their eyes briefly glow yellow.\r\n",
                     sk->name, being_display_name(target));
            descriptor_send(d, msg);
            if (target->desc)
                descriptor_notify(target->desc, "Your eyes tingle.\r\n");
        }
    } else if (strcasecmp(sk->name, "detect magic") == 0) {
        /* Level-25 audit batch. Real message (disc_mage_alchemy.cc's
         * detectMagic()): "Your eyes tingle." / "$n's eyes twinkle for a
         * brief moment." Flavor-only -- see AFFECT_DETECT_MAGIC's doc
         * comment (affect.h) for why there's no functional backing yet. */
        being_apply_affect(target, AFFECT_DETECT_MAGIC, 100);
        if (target == ch) {
            descriptor_send(d, "Your eyes tingle.\r\n");
            if (ch->base.roomp) {
                char capbuf[128];
                snprintf(msg, sizeof(msg), "%s's eyes twinkle for a brief moment.\r\n",
                         being_display_name_cap(ch, capbuf, sizeof(capbuf)));
                descriptor_room_echo(ch->base.roomp, ch, msg);
            }
        } else {
            snprintf(msg, sizeof(msg), "You cast %s on %s -- their eyes twinkle for a brief moment.\r\n",
                     sk->name, being_display_name(target));
            descriptor_send(d, msg);
            if (target->desc)
                descriptor_notify(target->desc, "Your eyes tingle.\r\n");
        }
    } else if (strcasecmp(sk->name, "bind") == 0) {
        /* Level-25 audit batch. Real message (disc_mage_sorcery.cc's
         * bind()): "You trap $N in a mass of sticky, web-like substance!"
         * See AFFECT_BIND's own doc comment (affect.h) for the real
         * movement-blocking mechanic (do_move()/cmd_move.c). Offensive --
         * falls back to ch->fighting like the damage branches below, same
         * atk_target convention. */
        if (!atk_target) {
            descriptor_send(d, "Cast that at whom?\r\n");
            return;
        }
        if (dufali_resists(atk_target)) {
            snprintf(msg, sizeof(msg), "You try to trap %s in a mass of sticky, web-like substance, but they shrug it off!\r\n",
                     being_display_name(atk_target));
            descriptor_send(d, msg);
            if (atk_target->desc)
                descriptor_notify(atk_target->desc, "A sticky, web-like substance grabs at you, but you shrug it off through sheer discipline!\r\n");
            return;
        }
        being_apply_affect(atk_target, AFFECT_BIND, 30);
        snprintf(msg, sizeof(msg), "You trap %s in a mass of sticky, web-like substance!\r\n",
                 being_display_name(atk_target));
        descriptor_send(d, msg);
        if (atk_target->desc)
            descriptor_notify(atk_target->desc, "You are trapped in a mass of sticky, web-like substance!\r\n");
    } else if (strcasecmp(sk->name, "trail seek") == 0) {
        /* Level-25 audit batch. Real message (disc_mage_earth.cc's
         * trailSeek()): "You become much more attuned to your senses!" /
         * "You whiff the aromas of many whom have passed through here."
         * Real upstream empowers the caster's own `track` skill for the
         * duration -- Tobin has no `track` command ported yet (Thief's
         * own level-13 roster entry, also not built), so this lands as a
         * flavor-only flag/timer affect with no functional hook, same
         * "no subsystem to empower yet" precedent as detect magic above.
         * No affect applied -- purely a one-off flavor message, nothing
         * to time out later. */
        if (target == ch) {
            descriptor_send(d, "You become much more attuned to your senses! You whiff the aromas of many whom have passed through here.\r\n");
            if (ch->base.roomp) {
                char capbuf[128];
                snprintf(msg, sizeof(msg), "%s's eyes glow with a faint blue light for a moment.\r\n",
                         being_display_name_cap(ch, capbuf, sizeof(capbuf)));
                descriptor_room_echo(ch->base.roomp, ch, msg);
            }
        } else {
            snprintf(msg, sizeof(msg), "You cast %s on %s -- you grant them the senses of a bloodhound!\r\n",
                     sk->name, being_display_name(target));
            descriptor_send(d, msg);
            if (target->desc)
                descriptor_notify(target->desc, "You become much more attuned to your senses! You whiff the aromas of many whom have passed through here.\r\n");
        }
    } else if (ci_contains(sk->name, "conjure elemental") || strcasecmp(sk->name, "animal companion") == 0
               || strcasecmp(sk->name, "animate") == 0
               || strcasecmp(sk->name, "beast charm") == 0 || strcasecmp(sk->name, "befriend beast") == 0) {
        /* Pet/charm (Sneezy → Tobin feature audit): Mage's four
         * "conjure elemental air/earth/fire/water" spells already existed
         * in the roster as CLASS-tier placeholders (real Sneezy names,
         * real "Summons a(n) X elemental ally" flavor text) but fell into
         * the generic fallback below like everything else Tobin has no
         * subsystem for -- this is that subsystem. Druid's "animal
         * companion" is new (no Ranger class in Tobin to inherit the real
         * beast-charm mechanic from). All four elemental/animal mob vnums
         * are real seeded world content (mob.c's own `wolf fierce gray`/
         * `fire elemental flame [fire]`/etc.), reused via
         * being_create_mob() the same way cmd_load.c's `load mob` does --
         * not new rows. */
        int vnum;
        const char *flavor;
        if (ci_contains(sk->name, "air")) {
            vnum = 19;
            flavor = "The air around you gathers itself into a swirling elemental form!";
        } else if (ci_contains(sk->name, "earth")) {
            vnum = 18;
            flavor = "The ground churns and rises, taking the shape of an elemental of earth!";
        } else if (ci_contains(sk->name, "fire")) {
            vnum = 16;
            flavor = "Flames roar together and take the shape of a fire elemental!";
        } else if (ci_contains(sk->name, "water")) {
            vnum = 17;
            flavor = "Water rushes together and takes the shape of a water elemental!";
        } else if (strcasecmp(sk->name, "animate") == 0) {
            /* Level-25 audit batch (2026-07-29). Real upstream
             * (disc_mage_sorcery.cc's animate()) targets a nearby corpse
             * or object and brings it to life -- Tobin has no corpse-
             * reanimation subsystem, so this reuses the same charmed-pet
             * machinery as the elementals above with a real seeded
             * construct mob (vnum 27, "stone golem", real seeded `mob`
             * table content, confirmed live) standing in for "an object
             * animated to fight for you". */
            vnum = 27;
            flavor = "Stone and iron groan and shift, rising up to answer your call!";
        } else if (strcasecmp(sk->name, "beast charm") == 0) {
            /* Ranger/Druid beast-charm pair (missing-skill audit batch
             * C, 2026-08-09) -- real upstream SKILL_BEAST_CHARM
             * (disc_ranger_animal.cc) is Ranger's own DISC_ANIMAL
             * specialty; Tobin folds Ranger's real nature/animal
             * skills into Druid (skill.c's roster-import doc comment,
             * same precedent `animal companion` already set). vnum 570
             * ("a gray wolf") confirmed live as the real seeded
             * analog. */
            vnum = 570;
            flavor = "You call out, and a gray wolf answers, baring its loyalty to you!";
        } else if (strcasecmp(sk->name, "befriend beast") == 0) {
            /* Same pair, gentler half -- real upstream SKILL_BEFRIEND_
             * BEAST is Ranger's basic-tier DISC_RANGER skill (as
             * opposed to Beast Charm's more advanced DISC_ANIMAL
             * specialty), same real vnum, distinct flavor text so the
             * two don't read identically. */
            vnum = 570;
            flavor = "You call out gently, and a gray wolf trots over, won over by your friendship!";
        } else {
            vnum = 570;
            flavor = "A loyal beast pads silently out of the wild to your side!";
        }

        if (being_find_charmed_pet(ch)) {
            descriptor_send(d, "You already have a charmed creature under your control.\r\n");
            return;
        }
        being_t *pet = being_summon_charmed_pet(ch, vnum, PET_CHARM_DURATION_ROUNDS);
        if (!pet) {
            descriptor_send(d, "The summoning fizzles -- nothing answers your call.\r\n");
            return;
        }
        snprintf(msg, sizeof(msg), "%s\r\n", flavor);
        descriptor_send(d, msg);
        char capbuf[128], roommsg[256];
        snprintf(roommsg, sizeof(roommsg), "%s casts %s, and %s appears, obedient to their will!\r\n",
                 being_display_name_cap(ch, capbuf, sizeof(capbuf)), sk->name, pet->base.short_descr);
        descriptor_room_echo(ch->base.roomp, ch, roommsg);
    } else if (strcasecmp(sk->name, "beast summon") == 0) {
        /* Beast summon (Druid, Tier-2 port): real upstream beastSummon()
         * calls distant wilderness animals to hunt down and path to the
         * caster. Where befriend beast/beast charm pop a wolf straight
         * into your room, this one spawns the wolf several rooms away and
         * sets it hunting you (mob_ai.c mob_begin_hunt / mob_hunt_tick) --
         * it advances one hop per combat round along a BFS path and, on
         * reaching you, becomes your charmed pet. The wandering-summon
         * flavor is the whole point; a foe in your room can be dealt with
         * before the beast even arrives. Same one-pet-at-a-time cap and
         * real seeded wolf (vnum 570) as the rest of the pet line. */
        if (being_find_charmed_pet(ch)) {
            descriptor_send(d, "You already have a charmed creature under your control.\r\n");
            return;
        }
        if (!ch->base.roomp) {
            descriptor_send(d, "You are nowhere the wild can hear your call.\r\n");
            return;
        }
        /* Random walk a few open hops out from the caster to pick a
         * spawn room; the beast will path back. Falls through to the
         * caster room itself if there is nowhere to walk (the wolf then
         * arrives essentially at once, same as befriend beast). */
        room_t *spawn = ch->base.roomp;
        int hops = 2 + rand() % 3; /* 2..4 rooms away */
        for (int h = 0; h < hops; h++) {
            int dirs[ROOM_NUM_EXITS], nd = 0;
            for (int i = 0; i < ROOM_NUM_EXITS; i++) {
                int dest = spawn->exits[i];
                if (dest < 0)
                    continue;
                if (spawn->exit_door[i] != 0 && (spawn->exit_cond[i] & EXIT_COND_CLOSED))
                    continue;
                dirs[nd++] = i;
            }
            if (!nd)
                break;
            int dest = spawn->exits[dirs[rand() % nd]];
            room_t *nr = world_get_room(dest);
            if (!nr) {
                nr = room_repo_load(dest);
                if (nr)
                    world_register_room(nr);
            }
            if (!nr || (nr->room_flag & ROOM_FLAG_NO_MOB))
                break;
            spawn = nr;
        }
        being_t *beast = being_create_mob(570);
        if (!beast) {
            descriptor_send(d, "You call into the wild, but nothing answers.\r\n");
            return;
        }
        thing_set_room(&beast->base, spawn);
        mob_begin_hunt(beast, ch, true);
        if (spawn == ch->base.roomp) {
            descriptor_send(d, "A gray wolf slips out of the wild, already at your side!\r\n");
        } else {
            descriptor_send(d, "You send your call into the wild; a gray wolf answers, and you sense it loping toward you...\r\n");
        }
        {
            char capbuf[128], roommsg[256];
            snprintf(roommsg, sizeof(roommsg),
                     "%s casts beast summon, calling to the wild for a beast to come.\r\n",
                     being_display_name_cap(ch, capbuf, sizeof(capbuf)));
            descriptor_room_echo(ch->base.roomp, ch, roommsg);
        }
    } else if (strcasecmp(sk->name, "sticks to snakes") == 0) {
        /* Druid conjuration (Tier-2 port; Sneezy sticksToSnakes(),
         * disc_shaman_spider.cc -- DISC_SHAMAN upstream, folded into
         * Tobin's Druid like the rest of the shaman/ranger nature line).
         * A tossed stick becomes a level-scaled serpent that springs at
         * your foe. Reuses the same one-at-a-time charmed-pet machinery as
         * the elemental/beast summons above; the snake mob vnums
         * (7855-7859, "rattlesnake ...") are real seeded content, chosen
         * by caster level to mirror Sneezy's own SNAKES25/30/35/40/50
         * strength tiers. */
        if (!atk_target) {
            descriptor_send(d, "Set your conjured serpent upon whom?\r\n");
            return;
        }
        if (being_is_immortal(atk_target)) {
            descriptor_send(d, "Your serpent recoils from a god.\r\n");
            return;
        }
        if (being_find_charmed_pet(ch)) {
            descriptor_send(d, "You already have a charmed creature under your control.\r\n");
            return;
        }
        int lvl = ch->progress.level;
        int snake_vnum = lvl < 26 ? 7855 : lvl < 31 ? 7856 : lvl < 36 ? 7857
                       : lvl < 41 ? 7858 : 7859;
        being_t *snake = being_summon_charmed_pet(ch, snake_vnum, PET_CHARM_DURATION_ROUNDS);
        if (!snake) {
            descriptor_send(d, "The stick refuses to take the shape of a snake.\r\n");
            return;
        }
        descriptor_send(d, "A strange yellow mist gathers and coils into a hissing serpent!\r\n");
        {
            char capbuf[128], roommsg[256];
            snprintf(roommsg, sizeof(roommsg),
                     "%s casts sticks to snakes, and a serpent coils up from a yellow mist!\r\n",
                     being_display_name_cap(ch, capbuf, sizeof(capbuf)));
            descriptor_room_echo(ch->base.roomp, ch, roommsg);
        }
        /* Loose the serpent on the victim (the pet fights for the caster). */
        if (!atk_target->fighting)
            atk_target->fighting = snake;
        snake->fighting = atk_target;
        snprintf(msg, sizeof(msg), "The serpent bares its fangs and springs at %s!\r\n",
                 being_display_name(atk_target));
        descriptor_send(d, msg);
    } else if (strcasecmp(sk->name, "polymorph") == 0) {
        /* Transformation (Sneezy → Tobin feature audit). Scoped via
         * AskUserQuestion, 2026-07-26: a FIXED form (a brown bear, real
         * seeded vnum 585 -- not a player-chosen target), reusing the
         * exact same descriptor-swap `possess`/`return` already use
         * (being_start_polymorph(), being.c) rather than a second
         * transformation mechanism. `ch` below still refers to the
         * player's OWN body after a successful swap (it isn't freed,
         * just detached from `d`) -- only used here for the room-echo
         * name and roomp, both still valid. */
        room_t *room = ch->base.roomp;
        char capbuf[128];
        being_display_name_cap(ch, capbuf, sizeof(capbuf));
        if (!being_start_polymorph(d, 585, TRANSFORM_DURATION_ROUNDS)) {
            descriptor_send(d, "You cast polymorph, but the transformation fails to take hold.\r\n");
            return;
        }
        descriptor_send(d, "Your body twists and reshapes -- you have become a brown bear!\r\n");
        if (room) {
            snprintf(msg, sizeof(msg), "%s twists and reshapes into a brown bear!\r\n", capbuf);
            descriptor_room_echo(room, NULL, msg);
        }

    } else {
        snprintf(msg, sizeof(msg),
                 "You cast %s, but nothing happens yet -- its real effect isn't implemented.\r\n",
                 sk->name);
        descriptor_send(d, msg);
    }
}

/* Runs the `cast` command: checks the caster is allowed to cast the
 * named spell (class, level, discipline practice), makes sure they
 * have a spell component on hand, then applies the spell's effect and
 * consumes the component. See this file's header comment for the
 * full rules. */
bool cmd_cast(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch)
        return true;

    bool imm = being_is_immortal(ch);
    if (!imm && ch->char_class != CLASS_MAGE && ch->char_class != CLASS_DRUID) {
        descriptor_send(d, "Command not found, maybe submit an idea if you believe TobinMUD should have it.\r\n");
        return true;
    }
    /* `silence` (level-48 stub-audit fix): a silenced caster can't cast
     * at all, checked before anything else below. */
    if (!imm && being_has_affect(ch, AFFECT_SILENCE)) {
        descriptor_send(d, "You try to speak the words, but no sound comes out!\r\n");
        return true;
    }

    while (*args == ' ')
        args++;
    if (!*args) {
        descriptor_send(d, "Cast what? Try 'skills' to see your spells.\r\n");
        return true;
    }

    /* `telepathy <message>` (spell/skill functional-completeness audit
     * continued, level-5+ list, 2026-07-27). Intercepted here, BEFORE
     * find_spell_and_target() below, because that helper only ever
     * captures a single trailing word as the "target" (see its own
     * comment) -- every other spell in this roster targets a being or
     * (identify) a single named item, but telepathy's own "target" is a
     * free-text message that can contain any number of words, which
     * would otherwise get mangled into a bogus multi-word "spell name"
     * lookup that always fails. Real upstream (disc_mage_spirit.cc's
     * telepathy()) reaches every connected character in the WORLD --
     * unlike `shout` (cmd_shout.c), it does NOT skip a sleeping
     * listener or honor the `noshout` toggle (telepathy is mind-to-
     * mind, not sound), a deliberate, disclosed difference from shout's
     * own scope, not a missed check. Garble/drunk-speech distortion
     * (Tobin has no such mechanic to port either) and the 5-Move cost
     * are the two other real pieces dropped. */
    if (strncasecmp(args, "telepathy", 9) == 0 && (args[9] == ' ' || args[9] == '\0')) {
        const skill_def_t *tsk = find_spell(ch->char_class, "telepathy", imm);
        if (!tsk) {
            descriptor_send(d, "You don't know a spell by that name.\r\n");
            return true;
        }
        if (!imm && ch->progress.level < tsk->min_level) {
            char lvlmsg[96];
            snprintf(lvlmsg, sizeof(lvlmsg), "You aren't experienced enough to cast %s yet (level %d).\r\n",
                     tsk->name, tsk->min_level);
            descriptor_send(d, lvlmsg);
            return true;
        }
        if (!imm && tsk->tier == SKILL_TIER_CLASS && ch->progress.basic_disc_pct <= 0) {
            descriptor_send(d, "You haven't practiced your Basic discipline yet -- visit a guildmaster.\r\n");
            return true;
        }
        obj_t *tcomp = component_for_cast(ch, "telepathy", imm);
        if (!tcomp) {
            descriptor_send(d, "You don't have the spell components to cast that.\r\n");
            return true;
        }
        const char *tmsg = args + 9;
        while (*tmsg == ' ')
            tmsg++;
        if (!*tmsg) {
            descriptor_send(d, "Telepathy is a nice spell, but you need to send some sort of message!\r\n");
            consume_component(d, tcomp);
            return true;
        }
        char tout[400];
        snprintf(tout, sizeof(tout), "You telepathically send the message, \"%s\"\r\n", tmsg);
        descriptor_send(d, tout);
        for (descriptor_t *it = g_descriptors; it; it = it->next) {
            if (!it->character || it->character == ch)
                continue;
            snprintf(tout, sizeof(tout),
                     "Your mind is flooded with a telepathic message from %s: \"%s\"\r\n",
                     being_display_name(ch), tmsg);
            descriptor_notify_comm(it, tout);
        }
        consume_component(d, tcomp);
        return true;
    }

    /* `scribe <spell name>` (level-25 audit batch, 2026-07-29). Intercepted
     * here, BEFORE find_spell_and_target() below, for the exact same
     * reason `telepathy` is above: that helper only ever captures a
     * single TRAILING word as the target (its own doc comment), but the
     * spell name being scribed is frequently multi-word ("heal light",
     * "cure poison", ...) -- letting it fall through would silently fail
     * to parse anything but a single-word spell name. See
     * obj_magic_repo_find_scroll_for_spell()'s doc comment for the
     * mechanic itself (no real SPELL_SCRIBE in the upstream source to
     * port from). */
    if (strncasecmp(args, "scribe", 6) == 0 && (args[6] == ' ' || args[6] == '\0')) {
        const skill_def_t *scribe_verb = find_spell(ch->char_class, "scribe", imm);
        if (!scribe_verb) {
            descriptor_send(d, "You don't know a spell by that name.\r\n");
            return true;
        }
        if (!imm && ch->progress.level < scribe_verb->min_level) {
            char lvlmsg[96];
            snprintf(lvlmsg, sizeof(lvlmsg), "You aren't experienced enough to cast %s yet (level %d).\r\n",
                     scribe_verb->name, scribe_verb->min_level);
            descriptor_send(d, lvlmsg);
            return true;
        }
        if (!imm && scribe_verb->tier == SKILL_TIER_ADVANCED &&
            (ch->progress.basic_disc_pct < 100 || ch->progress.combat_disc_pct < 100
             || ch->progress.advanced_disc_pct <= 0)) {
            descriptor_send(d, "Master your Basic and Combat disciplines, and begin Advanced practice, before this.\r\n");
            return true;
        }
        const char *spell_arg = args + 6;
        while (*spell_arg == ' ')
            spell_arg++;
        if (!*spell_arg) {
            descriptor_send(d, "Scribe which spell onto a scroll?\r\n");
            return true;
        }
        obj_t *scomp = component_for_cast(ch, "scribe", imm);
        if (!scomp) {
            descriptor_send(d, "You don't have the spell components to cast that.\r\n");
            return true;
        }
        const skill_def_t *scribe_sk = find_spell(ch->char_class, spell_arg, imm);
        if (!scribe_sk || (!imm && ch->progress.level < scribe_sk->min_level)) {
            descriptor_send(d, "You don't know a spell by that name.\r\n");
            return true;
        }
        int scroll_vnum;
        if (!obj_magic_repo_find_scroll_for_spell(scribe_sk->name, &scroll_vnum)) {
            descriptor_send(d, "You don't know how to inscribe that spell onto a scroll.\r\n");
            return true;
        }
        obj_t *scribed = obj_create_from_proto(scroll_vnum);
        if (!scribed || !ch->base.roomp) {
            descriptor_send(d, "You cast scribe, but nothing happens.\r\n");
            consume_component(d, scomp);
            return true;
        }
        scribed->price = 1; /* not a gold-creation bug -- same precedent as `copy` */
        thing_move_to(&scribed->base, &ch->base.roomp->base);
        const char *slabel = scribed->base.short_descr[0] ? scribed->base.short_descr : scribed->base.name;
        char scribemsg[224];
        snprintf(scribemsg, sizeof(scribemsg), "You inscribe %s onto a fresh scroll -- %s appears!\r\n",
                 scribe_sk->name, slabel);
        descriptor_send(d, scribemsg);
        descriptor_room_echo(ch->base.roomp, ch, scribemsg);
        consume_component(d, scomp);
        return true;
    }

    /* `materialize <item>` (Mage, level 6, stub-audit fix): real
     * upstream (disc_alchemy.cc) never used a spell component at all --
     * "for a price" means the flat MATERIALIZE_PRICE gold cost
     * cmd_materialize.c already charges, not a component pouch. Tobin
     * ALREADY has a real, correct, standalone `materialize <item>`
     * command (cmd_materialize.c, from an earlier audit pass) with its
     * own level/skill-known gating -- roster text promises `cast
     * materialize` too (skill.c lists it as a normal Mage spell), but
     * nothing routed that form to it, so it fell through to the
     * generic "nothing happens yet" placeholder despite the real
     * mechanic existing. Intercepted here, BEFORE find_spell_and_
     * target() and the outer level/discipline gates below (same
     * "handled separately" precedent as telepathy/scribe above), since
     * cmd_materialize() does its own gating already and delegating
     * through the generic component-pouch flow would wrongly demand a
     * component this spell was never supposed to need. */
    if ((strncasecmp(args, "materialize", 11) == 0 && (args[11] == ' ' || args[11] == '\0'))
        || (strncasecmp(args, "spontaneous generation", 22) == 0 && (args[22] == ' ' || args[22] == '\0'))) {
        if (!imm && ch->char_class != CLASS_MAGE) {
            descriptor_send(d, "You don't know a spell by that name.\r\n");
            return true;
        }
        bool is_sg = strncasecmp(args, "spontaneous", 11) == 0;
        const char *mat_arg = args + (is_sg ? 22 : 11);
        while (*mat_arg == ' ')
            mat_arg++;
        return cmd_materialize(d, mat_arg);
    }

    /* `ethereal gate <location>` (level-48 stub-audit fix). Intercepted
     * here, BEFORE find_spell_and_target() below, same multi-word-
     * argument precedent as telepathy/scribe above -- a named location
     * is frequently more than one word, and that helper only ever
     * captures a single trailing word as the target. Real, working
     * teleport to a room found by name (room_repo_find_vnum_by_name(),
     * room_repo.c -- same db_query() %s-escaping safety as obj_repo.c's
     * own obj_find_vnum_by_name()). */
    if (strncasecmp(args, "ethereal gate", 13) == 0 && (args[13] == ' ' || args[13] == '\0')) {
        const skill_def_t *egsk = find_spell(ch->char_class, "ethereal gate", imm);
        if (!egsk) {
            descriptor_send(d, "You don't know a spell by that name.\r\n");
            return true;
        }
        if (!imm && ch->progress.level < egsk->min_level) {
            char lvlmsg[96];
            snprintf(lvlmsg, sizeof(lvlmsg), "You aren't experienced enough to cast %s yet (level %d).\r\n",
                     egsk->name, egsk->min_level);
            descriptor_send(d, lvlmsg);
            return true;
        }
        if (!imm && egsk->tier == SKILL_TIER_ADVANCED &&
            (ch->progress.basic_disc_pct < 100 || ch->progress.combat_disc_pct < 100
             || ch->progress.advanced_disc_pct <= 0)) {
            descriptor_send(d, "Master your Basic and Combat disciplines, and begin Advanced practice, before this.\r\n");
            return true;
        }
        obj_t *egcomp = component_for_cast(ch, "ethereal gate", imm);
        if (!egcomp) {
            descriptor_send(d, "You don't have the spell components to cast that.\r\n");
            return true;
        }
        const char *egtarget = args + 13;
        while (*egtarget == ' ')
            egtarget++;
        if (!*egtarget) {
            descriptor_send(d, "Open a gate to where?\r\n");
            return true;
        }
        int egvnum = room_repo_find_vnum_by_name(egtarget);
        room_t *egdest = egvnum >= 0 ? world_get_room(egvnum) : NULL;
        if (!egdest && egvnum >= 0)
            egdest = room_repo_load(egvnum);
        if (egdest)
            world_register_room(egdest);
        if (!egdest) {
            descriptor_send(d, "You can't sense a place by that name.\r\n");
            consume_component(d, egcomp);
            return true;
        }
        room_t *egorigin = ch->base.roomp;
        descriptor_send(d, "You cast ethereal gate -- a shimmering portal tears open before you!\r\n");
        if (egorigin)
            descriptor_room_echo(egorigin, ch, "A shimmering portal tears open, and someone steps through!\r\n");
        thing_set_room(&ch->base, egdest);
        descriptor_room_echo(egdest, ch, "A shimmering portal tears open, and someone steps out!\r\n");
        cmd_dispatch(d, "look");
        consume_component(d, egcomp);
        return true;
    }

    /* `transform limb <part>` (Druid batch, Tier-2 port 2026-08-16).
     * Intercepted here, BEFORE find_spell_and_target() below, same
     * free-text-argument precedent as telepathy/scribe/ethereal gate
     * above: the <part> keyword ("gills"/"wings"/"claws") is not a being
     * or item target, so letting it fall through to the generic being-
     * targeting flow would drop the one piece of information this spell
     * needs. Real upstream (disc_shaman_frog.cc's transformLimb()) turns
     * one of the caster's OWN limbs into an animal form, each granting a
     * different real effect; Tobin has no per-limb transform subsystem,
     * so the keyword maps onto Tobin's existing real affects (see
     * AFFECT_TRANSFORMED_LIMB's doc comment): gills/neck ->
     * AFFECT_WATERBREATH, wings/arms -> AFFECT_FLYING, claws/hands -> a
     * STRENGTH buff (AFFECT_TRANSFORMED_LIMB stat affect, standing in for
     * upstream's hands-case damroll bonus). Self-only; head/legs scoped
     * out. */
    if (strncasecmp(args, "transform limb", 14) == 0 && (args[14] == ' ' || args[14] == '\0')) {
        const skill_def_t *tlsk = find_spell(ch->char_class, "transform limb", imm);
        if (!tlsk) {
            descriptor_send(d, "You don't know a spell by that name.\r\n");
            return true;
        }
        if (!imm && ch->progress.level < tlsk->min_level) {
            char lvlmsg[96];
            snprintf(lvlmsg, sizeof(lvlmsg), "You aren't experienced enough to cast %s yet (level %d).\r\n",
                     tlsk->name, tlsk->min_level);
            descriptor_send(d, lvlmsg);
            return true;
        }
        if (!imm && tlsk->tier == SKILL_TIER_CLASS && ch->progress.basic_disc_pct <= 0) {
            descriptor_send(d, "You haven't practiced your Basic discipline yet -- visit a guildmaster.\r\n");
            return true;
        }
        const char *part = args + 14;
        while (*part == ' ')
            part++;
        if (!*part) {
            descriptor_send(d, "Transform which limb? Try: gills, wings, or claws.\r\n");
            return true;
        }
        obj_t *tlcomp = component_for_cast(ch, "transform limb", imm);
        if (!tlcomp) {
            descriptor_send(d, "You don't have the spell components to cast that.\r\n");
            return true;
        }
        if (strncasecmp(part, "gills", 5) == 0 || strncasecmp(part, "neck", 4) == 0) {
            being_apply_affect(ch, AFFECT_WATERBREATH, 40 * COMBAT_ROUND_PULSES);
            descriptor_send(d, "Your neck ripples and sprouts fish-like gills -- you can breathe water!\r\n");
        } else if (strncasecmp(part, "wings", 5) == 0 || strncasecmp(part, "arms", 4) == 0) {
            being_apply_affect(ch, AFFECT_FLYING, 40 * COMBAT_ROUND_PULSES);
            descriptor_send(d, "Your arms stretch and feather into broad wings -- you rise into the air!\r\n");
        } else if (strncasecmp(part, "claws", 5) == 0 || strncasecmp(part, "hands", 5) == 0) {
            int bonus = 1 + tlsk->min_level / 8;
            being_apply_stat_affect(ch, AFFECT_TRANSFORMED_LIMB, 40 * COMBAT_ROUND_PULSES, bonus);
            descriptor_send(d, "Your hands harden into raking bestial claws -- your blows land with brutal force!\r\n");
        } else {
            descriptor_send(d, "You can't transform that. Try: gills, wings, or claws.\r\n");
            return true;
        }
        if (ch->base.roomp) {
            char capbuf[128], tlmsg[224];
            snprintf(tlmsg, sizeof(tlmsg), "%s's flesh shimmers and one of their limbs takes on a bestial shape!\r\n",
                     being_display_name_cap(ch, capbuf, sizeof(capbuf)));
            descriptor_room_echo(ch->base.roomp, ch, tlmsg);
        }
        consume_component(d, tlcomp);
        return true;
    }

    char target_buf[64];
    const char *target_name;
    const skill_def_t *sk = find_spell_and_target(ch->char_class, args, imm, target_buf, sizeof(target_buf), &target_name);
    if (!sk) {
        descriptor_send(d, "You don't know a spell by that name.\r\n");
        return true;
    }
    /* `meditate` is NOT a spell (user 2026-08-06) -- caught here, before
     * ANY of the normal cast machinery below (level/discipline gates,
     * component check, mana spend, flavor text, skill-roll/proficiency
     * gain) runs for it. It's a real standalone command now
     * (cmd_meditate.c) -- redirecting this early means `cast meditate`
     * costs nothing and trains nothing, unlike the brief window earlier
     * this same session where it fell through to the normal spell
     * dispatch and did all of that before redirecting. */
    if (strcasecmp(sk->name, "meditate") == 0 || strcasecmp(sk->name, "refresh") == 0) {
        descriptor_send(d, "Meditate isn't something you cast -- just type `meditate`.\r\n");
        return true;
    }
    if (!imm && ch->progress.level < sk->min_level) {
        char msg[96];
        snprintf(msg, sizeof(msg), "You aren't experienced enough to cast %s yet (level %d).\r\n",
                 sk->name, sk->min_level);
        descriptor_send(d, msg);
        return true;
    }
    /* Discipline-percentage gate (user 2026-07-12: "players have to
     * visit a guildmaster to gain skills based upon percentage of
     * discipline learned") -- see cmd_practice.c. Bypassed for
     * immortals, same "no restrictions" spirit as the level/class
     * gates above. */
    if (!imm) {
        if (sk->tier == SKILL_TIER_CLASS && ch->progress.basic_disc_pct <= 0) {
            descriptor_send(d, "You haven't practiced your Basic discipline yet -- visit a guildmaster.\r\n");
            return true;
        }
        if (sk->tier == SKILL_TIER_COMBAT && ch->progress.combat_disc_pct <= 0) {
            descriptor_send(d, "You haven't practiced your Combat discipline yet -- visit a combat guildmaster.\r\n");
            return true;
        }
        if (sk->tier == SKILL_TIER_ADVANCED &&
            (ch->progress.basic_disc_pct < 100 || ch->progress.combat_disc_pct < 100
             || ch->progress.advanced_disc_pct <= 0)) {
            descriptor_send(d, "Master your Basic and Combat disciplines, and begin Advanced practice, before this.\r\n");
            return true;
        }
    }

    /* `entangling roots` (Druid, level 1) -- "only works outdoors" has to
     * be checked HERE, before the generic component-consumption flow
     * below, not inside task_cast()'s own branch for this spell: that
     * function returns void with no way to tell this caller "refused,
     * don't consume anything", and the generic path always runs
     * consume_component() unconditionally right after task_cast()
     * returns (whatever it did or didn't do) -- so a refusal inside
     * task_cast() would still cost the caster their component. Same
     * "intercept before the generic path" shape identify/scribe/
     * telepathy below already use, for the same underlying reason. */
    if (strcasecmp(sk->name, "entangling roots") == 0
        && ch->base.roomp && (ch->base.roomp->room_flag & ROOM_FLAG_INDOORS)) {
        descriptor_send(d, "There's no earth to command in here -- entangling roots only works outdoors.\r\n");
        return true;
    }
    /* `living vines` (Shaman/Druid audit batch C, 2026-08-09) -- real
     * upstream: "You need to be in nature or on land to cast this
     * spell!" (disc_shaman_spider.cc's own outdoor/land gate). Same
     * "intercept before the generic component-consumption path" reason
     * entangling roots' own gate just above already documents. */
    if (strcasecmp(sk->name, "living vines") == 0
        && ch->base.roomp && (ch->base.roomp->room_flag & ROOM_FLAG_INDOORS)) {
        descriptor_send(d, "There's no earth to command in here -- living vines only works outdoors.\r\n");
        return true;
    }
    /* `root control` (Druid batch, 2026-08-16) -- real upstream refuses
     * with "You need to be in nature or on land to cast this spell!"
     * (disc_shaman_spider.cc's rootControl() sector gate). Same
     * "intercept before the generic component-consumption path" reason
     * entangling roots/living vines gates above already document. */
    if (strcasecmp(sk->name, "root control") == 0
        && ch->base.roomp && (ch->base.roomp->room_flag & ROOM_FLAG_INDOORS)) {
        descriptor_send(d, "There's no earth to command in here -- root control only works outdoors.\r\n");
        return true;
    }

    /* `stormy skies` (Druid batch, Tier-2 port 2026-08-16) -- real
     * upstream (disc_shaman_frog.cc's stormySkies()) only works OUTDOORS
     * and only when the weather is actually stormy (RAINY/LIGHTNING/SNOWY
     * upstream; RAINY/STORMY in Tobin's simpler world-wide weather model,
     * weather.h). Gated HERE, before the component is consumed, same
     * pattern/reason as entangling roots' outdoor gate just above -- so a
     * clear-sky or indoor refusal doesn't cost the caster their
     * component. */
    if (strcasecmp(sk->name, "stormy skies") == 0) {
        if (ch->base.roomp && (ch->base.roomp->room_flag & ROOM_FLAG_INDOORS)) {
            descriptor_send(d, "You need open sky above you to call down a storm.\r\n");
            return true;
        }
        weather_t w = weather_current();
        if (w != WEATHER_RAINY && w != WEATHER_STORMY) {
            descriptor_send(d, "The sky is too calm -- there's no storm here to call upon.\r\n");
            return true;
        }
    }

    if (strcasecmp(sk->name, "divination") == 0) {
        /* Level-45 stub-audit fix: object-target, before being-target
         * resolution, same "identify" precedent just below -- scoped to
         * the object half of "reveals information about an object or
         * being" (disclosed gap: no being-inspection mechanic exists). */
        obj_t *divcomp = component_for_cast(ch, "divination", imm);
        if (!divcomp) {
            descriptor_send(d, "You don't have the spell components to cast that.\r\n");
            return true;
        }
        if (!target_name) {
            descriptor_send(d, "Divine information about what?\r\n");
            return true;
        }
        bool divok = cmd_identify(d, target_name);
        consume_component(d, divcomp);
        return divok;
    }

    if (strcasecmp(sk->name, "identify") == 0) {
        /* Full spell/skill/prayer roster import continued, level-5+ list
         * (2026-07-27): real upstream (disc_mage_alchemy.cc's identify())
         * targets an OBJECT, not a being -- every other spell in this
         * roster targets a being via combat_find_room_target(), so this
         * is handled separately, before the being-target resolution/
         * task_cast() below. Found live while building this: Tobin
         * ALREADY has a real, correct, general-purpose `identify`
         * command (cmd_identify.c, from an earlier "Object manipulation
         * depth" audit pass) -- it was deliberately built as a plain,
         * ungated command rather than a spell, since Tobin's val[]
         * payload has nothing real for "accuracy scales with skill" to
         * scale. An earlier version of this branch duplicated that
         * display logic from scratch and got it factually wrong (real
         * weapon damage does NOT come from val[0]/val[1] -- cmd_identify.c's
         * own header comment documents exactly why, verified against
         * real seeded data). Fixed by delegating to the real,
         * already-correct command instead of re-deriving it -- `cast
         * identify` just adds the spell-specific component gate on top
         * of the same real logic every player can already reach via
         * the bare `identify` command. */
        obj_t *idcomp = component_for_cast(ch, "identify", imm);
        if (!idcomp) {
            descriptor_send(d, "You don't have the spell components to cast that.\r\n");
            return true;
        }
        if (!target_name) {
            descriptor_send(d, "Identify what?\r\n");
            return true;
        }
        bool ok = cmd_identify(d, target_name);
        consume_component(d, idcomp);
        return ok;
    }

    if (strcasecmp(sk->name, "copy") == 0) {
        /* Level-23 audit batch (2026-07-28). Ported from Mage's real
         * `copy` (disc_mage_alchemy.cc's TScroll::copyMe()) -- targets an
         * OBJECT the caster is carrying, same "handled separately, before
         * the being-target resolution below" precedent as `identify`
         * above. Real upstream only ever works on a SCROLL (every other
         * object type's copyMe() just fails with "That's not a scroll!"),
         * and refuses unless the caster already knows every spell bound
         * to it -- both checked here against Tobin's real obj_magic
         * table (the same one `use`/cmd_use.c reads a scroll's spell
         * from) and the same class+level "know this spell" gate `cast`
         * itself already enforces on every other spell. Duplicates the
         * exact vnum into the room the caster is standing in, matching
         * the original's `*caster->roomp += *new_obj` placement (not
         * straight into inventory) -- a decaying, worthless-if-sold
         * (cost forced to 1, same "let's not make this a gold creating
         * bug" comment as the original) copy, not a real duplication
         * exploit. */
        obj_t *ccomp = component_for_cast(ch, "copy", imm);
        if (!ccomp) {
            descriptor_send(d, "You don't have the spell components to cast that.\r\n");
            return true;
        }
        if (!target_name) {
            descriptor_send(d, "Copy what?\r\n");
            return true;
        }
        obj_t *scroll = NULL;
        size_t tlen = strlen(target_name);
        for (thing_t *t = ch->base.stuff_head; t; t = t->stuff_next) {
            if (t->kind == THING_OBJ && thing_name_matches(t->name, target_name, tlen)) {
                scroll = (obj_t *)t;
                break;
            }
        }
        if (!scroll) {
            descriptor_send(d, "You aren't carrying that.\r\n");
            return true;
        }
        if (scroll->category != OBJ_CAT_MAGIC_DEVICE || scroll->raw_type != 2) {
            descriptor_send(d, "That's not a scroll!\r\n");
            return true;
        }
        char spell_name[OBJ_MAGIC_SPELL_NAME_LEN];
        int max_charges;
        if (!obj_magic_repo_get(scroll->vnum, spell_name, sizeof(spell_name), &max_charges)) {
            descriptor_send(d, "That's not a scroll!\r\n");
            return true;
        }
        const skill_def_t *scroll_sk = skill_find(ch->char_class, spell_name, false);
        if (!imm && (!scroll_sk || ch->progress.level < scroll_sk->min_level)) {
            descriptor_send(d, "You can only copy scrolls of spells that you know.\r\n");
            return true;
        }
        obj_t *dup = obj_create_from_proto(scroll->vnum);
        if (!dup || !ch->base.roomp) {
            descriptor_send(d, "You cast copy, but nothing happens.\r\n");
            consume_component(d, ccomp);
            return true;
        }
        dup->price = 1; /* not a gold-creation bug -- same original comment */
        thing_move_to(&dup->base, &ch->base.roomp->base);
        const char *label = scroll->base.short_descr[0] ? scroll->base.short_descr : scroll->base.name;
        char copymsg[224];
        snprintf(copymsg, sizeof(copymsg), "In a flash of light, a copy of %s appears!\r\n", label);
        descriptor_send(d, copymsg);
        descriptor_room_echo(ch->base.roomp, ch, copymsg);
        consume_component(d, ccomp);
        return true;
    }

    if (strcasecmp(sk->name, "create food") == 0) {
        /* Level-9 (Druid)/level-3 (Cleric, cmd_pray.c mirrors this)
         * stub-audit fix: "Conjures food from nothing" -- a real,
         * eatable OBJ_CAT_FOOD item (obj_create_ephemeral(), val[0]=
         * nutrition same field cmd_eat.c already reads), dropped into
         * the caster's hands, not just a hunger-meter bump -- matches
         * the roster text literally (an ITEM appears), and the food can
         * be carried, given away, or eaten later like any other food. */
        obj_t *cfcomp = component_for_cast(ch, "create food", imm);
        if (!cfcomp) {
            descriptor_send(d, "You don't have the spell components to cast that.\r\n");
            return true;
        }
        obj_t *food = obj_create_ephemeral("bread loaf conjured", "a conjured loaf of bread",
                                            "A conjured loaf of bread sits here.", OBJ_CAT_FOOD);
        if (!food) {
            descriptor_send(d, "You cast create food, but nothing happens.\r\n");
            consume_component(d, cfcomp);
            return true;
        }
        food->val[0] = 12;
        thing_move_to(&food->base, &ch->base);
        descriptor_send(d, "You cast create food -- a warm loaf of bread appears in your hands!\r\n");
        char cfmsg[128], cfcapbuf[128];
        snprintf(cfmsg, sizeof(cfmsg), "A loaf of bread appears in %s hands!\r\n",
                 being_display_name_cap(ch, cfcapbuf, sizeof(cfcapbuf)));
        descriptor_room_echo(ch->base.roomp, ch, cfmsg);
        consume_component(d, cfcomp);
        return true;
    }

    if (strcasecmp(sk->name, "create water") == 0) {
        /* Level-9 (Druid)/level-3 (Cleric) stub-audit fix: "Fills a
         * container with water" -- targets an OBJECT the caster is
         * carrying, same object-target-before-being-resolution shape
         * as identify/copy/illuminate above. Fills it with real LIQUID
         * water (val[1]=current, val[0]=capacity, val[2]=type -- same
         * fields cmd_fill.c's mundane `fill` uses), refusing only a
         * container that's already full of something else (same mixing
         * refusal cmd_fill.c already enforces) -- unlike `fill`, needs
         * no room-side water source, since the whole point is conjuring
         * it from nothing. */
        obj_t *cwcomp = component_for_cast(ch, "create water", imm);
        if (!cwcomp) {
            descriptor_send(d, "You don't have the spell components to cast that.\r\n");
            return true;
        }
        if (!target_name) {
            descriptor_send(d, "Create water into what?\r\n");
            return true;
        }
        obj_t *jug = liquid_find_carried_container(ch, target_name);
        if (!jug) {
            descriptor_send(d, "You aren't carrying that.\r\n");
            return true;
        }
        if (jug->val[1] >= jug->val[0]) {
            descriptor_send(d, "That is already completely full!\r\n");
            return true;
        }
        if (jug->val[1] > 0 && jug->val[2] != LIQUID_TYPE_DEFAULT) {
            descriptor_send(d, "You can't mix water with what's already in there -- pour it out first.\r\n");
            return true;
        }
        jug->val[2] = LIQUID_TYPE_DEFAULT;
        jug->val[1] = jug->val[0];
        const char *jlabel = jug->base.short_descr[0] ? jug->base.short_descr : jug->base.name;
        char cwmsg[224];
        snprintf(cwmsg, sizeof(cwmsg), "You cast create water -- %s fills to the brim!\r\n", jlabel);
        descriptor_send(d, cwmsg);
        snprintf(cwmsg, sizeof(cwmsg), "%s fills with water out of thin air!\r\n", jlabel);
        descriptor_room_echo(ch->base.roomp, ch, cwmsg);
        consume_component(d, cwcomp);
        return true;
    }

    if (strcasecmp(sk->name, "illuminate") == 0) {
        /* Level-2 stub-audit fix: "Lights up an object" -- targets an
         * OBJECT, not a being, same "handled separately, before the
         * being-target resolution below" precedent as identify/copy/
         * charge stave above. Real, working reuse of the mundane
         * light/extinguish/refuel mechanic (cmd_light.c, val[3]=is lit,
         * val[2]=current burn) rather than a bespoke glow-affect: finds
         * an OBJ_CAT_LIGHT item the caster is carrying (or sees in the
         * room), and magically lights it -- topping off its fuel to
         * full first if needed, so a spent lamp/torch works too, unlike
         * the mundane `light` command which requires fuel already
         * present. Refuses an already-lit item and one with no fuel
         * capacity at all (OBJ_CAT_LIGHT items with maxBurn < 0, e.g.
         * unrefuelable torches once truly spent -- same
         * LIGHT_UNREFUELABLE concept cmd_light.c's refuel already
         * checks, duplicated here rather than exported since it's one
         * field read). */
        obj_t *ilcomp = component_for_cast(ch, "illuminate", imm);
        if (!ilcomp) {
            descriptor_send(d, "You don't have the spell components to cast that.\r\n");
            return true;
        }
        if (!target_name) {
            descriptor_send(d, "Illuminate what?\r\n");
            return true;
        }
        obj_t *litem = NULL;
        size_t iltlen = strlen(target_name);
        for (thing_t *t = ch->base.stuff_head; !litem && t; t = t->stuff_next) {
            if (t->kind == THING_OBJ && thing_name_matches(t->name, target_name, iltlen))
                litem = (obj_t *)t;
        }
        if (!litem && ch->base.roomp) {
            for (thing_t *t = ch->base.roomp->base.stuff_head; !litem && t; t = t->stuff_next) {
                if (t->kind == THING_OBJ && thing_name_matches(t->name, target_name, iltlen))
                    litem = (obj_t *)t;
            }
        }
        if (!litem) {
            descriptor_send(d, "You don't have (or see) that here to illuminate.\r\n");
            return true;
        }
        if (litem->category != OBJ_CAT_LIGHT) {
            descriptor_send(d, "That's not something that can be lit.\r\n");
            return true;
        }
        if (litem->val[3]) {
            descriptor_send(d, "It's already lit.\r\n");
            return true;
        }
        if (litem->val[1] < 0) {
            descriptor_send(d, "That can never hold a flame.\r\n");
            return true;
        }
        if (litem->val[2] <= 0)
            litem->val[2] = litem->val[1];
        litem->val[3] = 1;
        const char *ilabel = litem->base.short_descr[0] ? litem->base.short_descr : litem->base.name;
        char ilmsg[224];
        snprintf(ilmsg, sizeof(ilmsg), "You cast illuminate -- %s bursts into flame!\r\n", ilabel);
        descriptor_send(d, ilmsg);
        if (ch->base.roomp) {
            snprintf(ilmsg, sizeof(ilmsg), "%s bursts into flame with a soft magical light!\r\n", ilabel);
            descriptor_room_echo(ch->base.roomp, ch, ilmsg);
        }
        consume_component(d, ilcomp);
        return true;
    }

    if (strcasecmp(sk->name, "mage repair") == 0) {
        obj_t *mrcomp = component_for_cast(ch, "mage repair", imm);
        if (!mrcomp) {
            descriptor_send(d, "You don't have the spell components to cast that.\r\n");
            return true;
        }
        if (!target_name) {
            descriptor_send(d, "Repair what?\r\n");
            return true;
        }
        obj_t *item = NULL;
        size_t mrlen = strlen(target_name);
        for (thing_t *t = ch->base.stuff_head; t; t = t->stuff_next) {
            if (t->kind == THING_OBJ && thing_name_matches(t->name, target_name, mrlen)) {
                item = (obj_t *)t;
                break;
            }
        }
        if (!item) {
            descriptor_send(d, "You aren't carrying that.\r\n");
            return true;
        }
        if (item->max_struct <= 0) {
            descriptor_send(d, "That isn't the sort of thing that can be damaged or repaired.\r\n");
            return true;
        }
        int ceiling = item->max_struct - item->depreciation;
        if (item->cur_struct >= ceiling) {
            descriptor_send(d, "It's already in as good a condition as magic can get it.\r\n");
            return true;
        }
        item->cur_struct = ceiling;
        const char *mrlabel = item->base.short_descr[0] ? item->base.short_descr : item->base.name;
        char mrmsg[224];
        snprintf(mrmsg, sizeof(mrmsg), "You cast mage repair -- %s knits itself back together!\r\n", mrlabel);
        descriptor_send(d, mrmsg);
        snprintf(mrmsg, sizeof(mrmsg), "%s knits itself back together in a shimmer of light!\r\n", mrlabel);
        descriptor_room_echo(ch->base.roomp, ch, mrmsg);
        consume_component(d, mrcomp);
        return true;
    }

    if (strcasecmp(sk->name, "charge stave") == 0 || strcasecmp(sk->name, "powerstone") == 0) {
        /* `powerstone` (level-19 stub-audit fix) folded in here: Tobin
         * has no mana pool for a "mana battery" item to charge into
         * (same gap attune/devotion already disclosed), so it reuses
         * this real charges-refill mechanic instead of doing nothing. */
        /* Level-25 audit batch. No real SPELL_CHARGE exists in the
         * bundled upstream source to port -- Tobin-original mechanic:
         * refills a carried magic device's current charges (val[0], see
         * obj.c's own doc comment on val[0]=current/val[1]=max) back to
         * full. Object-target, before being-target resolution, same
         * precedent as `copy`/`identify`/`scribe` above. Staff-specific
         * per the roster's own "Charges a magical stave" text -- refuses
         * a scroll (single-use, nothing to recharge) or wand (a
         * deliberate scope-cut, not a bug: staves are the AREA-effect
         * device tier, matching this being a level-25 ADVANCED spell). */
        obj_t *chcomp = component_for_cast(ch, "charge stave", imm);
        if (!chcomp) {
            descriptor_send(d, "You don't have the spell components to cast that.\r\n");
            return true;
        }
        if (!target_name) {
            descriptor_send(d, "Charge what stave?\r\n");
            return true;
        }
        obj_t *stave = NULL;
        size_t chlen = strlen(target_name);
        for (thing_t *t = ch->base.stuff_head; t; t = t->stuff_next) {
            if (t->kind == THING_OBJ && thing_name_matches(t->name, target_name, chlen)) {
                stave = (obj_t *)t;
                break;
            }
        }
        if (!stave) {
            descriptor_send(d, "You aren't carrying that.\r\n");
            return true;
        }
        if (stave->category != OBJ_CAT_MAGIC_DEVICE || stave->raw_type != 4) {
            descriptor_send(d, "That's not a stave!\r\n");
            return true;
        }
        stave->val[0] = stave->val[1];
        char chargemsg[192];
        const char *chlabel = stave->base.short_descr[0] ? stave->base.short_descr : stave->base.name;
        snprintf(chargemsg, sizeof(chargemsg), "Arcane energy pours into %s, restoring its charges to full!\r\n", chlabel);
        descriptor_send(d, chargemsg);
        consume_component(d, chcomp);
        return true;
    }

    if (strcasecmp(sk->name, "eyes of Fertuman") == 0) {
        /* Fresh Sneezy -> Tobin audit item (docs/systems don't cover
         * individual spells, so verified against the real source directly:
         * disc/disc_mage_alchemy.cc's eyesOfFertuman()). Real upstream is a
         * world-wide LOCATE by name -- scans every live object AND every
         * character for a name match, reporting which room each is in,
         * skipping the caster's own belongings, shopkeepers, and immortals,
         * with an item's own ITEM_MAGIC flag making it partly resistant to
         * being found (a random chance to skip it) and a critical-success
         * roll widening the result cap. Scoped down for Tobin: no critical-
         * success tier (no bSuccess()-style crit roll exists here to hook
         * into), no ITEM_MAGIC-flag resistance chance (Tobin's obj_t has no
         * such flag), a flat result cap (5) instead of a skill/level-scaled
         * one, and object results are limited to items lying loose in a
         * room (world_for_each_obj() visits every live obj_t, but roomp is
         * only reliable for a room-floor item -- see thing.h's own doc
         * comment: it goes stale for anything nested in a container or
         * carried in inventory, so those are honestly out of scope here
         * rather than reported with a wrong location). Still a real,
         * useful world-wide search, not a fake stub. */
        obj_t *fecomp = component_for_cast(ch, "eyes of Fertuman", imm);
        if (!fecomp) {
            descriptor_send(d, "You don't have the spell components to cast that.\r\n");
            return true;
        }
        if (!target_name) {
            descriptor_send(d, "Locate what (or whom)?\r\n");
            return true;
        }
        descriptor_send(d, "The eyes of Fertuman look far and wide across the world and find:\r\n");
        size_t felen = strlen(target_name);
        int found = 0;
        char febuf[512];

        for (descriptor_t *it = g_descriptors; it; it = it->next) {
            if (found >= 5)
                break;
            being_t *b = it->character;
            if (!b || b == ch || !b->base.roomp)
                continue;
            if (being_is_immortal(b))
                continue;
            if (!thing_name_matches(b->base.name, target_name, felen))
                continue;
            snprintf(febuf, sizeof(febuf), "  %s is in %s.\r\n",
                     being_display_name(b), b->base.roomp->base.name);
            descriptor_send(d, febuf);
            found++;
        }

        g_fertuman_ch = ch;
        g_fertuman_target = target_name;
        g_fertuman_found = found;
        world_for_each_mob(fertuman_mob_visit);
        world_for_each_obj(fertuman_obj_visit);
        found = g_fertuman_found;

        if (found == 0)
            descriptor_send(d, "  ...nothing.\r\n");
        consume_component(d, fecomp);
        return true;
    }

    if (strcasecmp(sk->name, "farlook") == 0) {
        /* Level-25 audit batch. Real upstream (disc_mage_alchemy.cc's
         * farlook()) scries a REMOTE room -- targets a being anywhere in
         * the world, not just the caster's own room, unlike every other
         * spell in this roster (combat_find_room_target() below is room-
         * scoped only). Same global-lookup-by-name-prefix pattern
         * cmd_tell.c/cmd_transfer.c already use for reaching anyone
         * connected anywhere. Shows the target's room description to the
         * caster without moving anyone -- not the real upstream's fuller
         * "shows everyone in that room what THEY see too" broadcast (no
         * clean Tobin equivalent without a temporary viewpoint-swap
         * mechanism this v1 doesn't have). */
        obj_t *fcomp = component_for_cast(ch, "farlook", imm);
        if (!fcomp) {
            descriptor_send(d, "You don't have the spell components to cast that.\r\n");
            return true;
        }
        if (!target_name) {
            descriptor_send(d, "Farlook whom?\r\n");
            return true;
        }
        size_t flen = strlen(target_name);
        being_t *fartarget = NULL;
        for (descriptor_t *it = g_descriptors; it; it = it->next) {
            if (it->character && it->character->base.roomp
                && strncasecmp(it->character->base.name, target_name, flen) == 0) {
                fartarget = it->character;
                break;
            }
        }
        if (!fartarget || !fartarget->base.roomp) {
            descriptor_send(d, "You can't seem to locate them.\r\n");
            return true;
        }
        descriptor_send(d, "You conjure up a large cloud which shimmers slightly before revealing...\r\n");
        char farmsg[192];
        snprintf(farmsg, sizeof(farmsg), "<c>%s<z>\r\n", fartarget->base.roomp->base.name);
        descriptor_send(d, farmsg);
        descriptor_send(d, fartarget->base.roomp->description);
        descriptor_send(d, "\r\n");
        consume_component(d, fcomp);
        return true;
    }

    /* Defaults to self, same as always (heal/buff spells with no target
     * mean self) -- task_cast()'s offensive branches separately fall
     * back to ch->fighting when target is still `ch` at that point, so
     * "cast magic missile" with no target keeps hitting whoever you're
     * already fighting, unchanged from before this breadth pass. */
    being_t *target = ch;
    if (target_name) {
        target = combat_find_room_target(ch, target_name);
        if (!target) {
            descriptor_send(d, "You don't see them here.\r\n");
            return true;
        }
    }

    obj_t *component = component_for_cast(ch, sk->name, imm);
    if (!component) {
        const char *needname = spell_bound_component_name(sk->name);
        char needmsg[192];
        if (needname)
            snprintf(needmsg, sizeof(needmsg), "You need %s to cast that.\r\n", needname);
        else
            snprintf(needmsg, sizeof(needmsg), "You don't have the spell components to cast that.\r\n");
        descriptor_send(d, needmsg);
        return true;
    }

    /* Real mana cost (user 2026-08-06: "implement it just like sneezy"),
     * Mage only -- Druid has no mana pool at all (a separate, not-yet-
     * built Lifeforce resource in the real game, see
     * being_calc_max_mana()'s own doc comment), so gating Druid's cast
     * on mana here would wrongly refuse every Druid spell outright
     * (max_mana is always 0 for them). You must still be able to afford
     * the WHOLE spell to begin it, but the cost is no longer drawn all at
     * once here (user 2026-08-16: "mana should deplete for each round of
     * casting ... if they lose concentration it doesn't cost the same as
     * a full cast") -- a successful cast hands `cast_cost` to the
     * multi-round task below, which charges a proportional slice each
     * round; only a fumble (no rounds to prorate) still pays in full. An
     * immortal never pays, same "no restrictions" spirit as every other
     * immortal bypass in this function. */
    int cast_cost = 0;
    if (!imm && (ch->char_class == CLASS_MAGE || ch->char_class == CLASS_DRUID)) {
        int cost = spell_mana_cost(sk->name, sk->min_level);
        if (ch->progress.mana < cost) {
            const char *rlabel = ch->char_class == CLASS_DRUID ? "Lifeforce" : "mana";
            char lowmsg[112];
            snprintf(lowmsg, sizeof(lowmsg), "You don't have enough %s to cast %s (need %d).\r\n",
                     rlabel, sk->name, cost);
            descriptor_send(d, lowmsg);
            return true;
        }
        cast_cost = cost;
    }

    /* Per-skill proficiency (Sneezy-style learn-by-doing, user 2026-07-17)
     * -- separate from the discipline-percentage ACCESS gate above, this
     * is the caster's own success chance at THIS specific spell, and it
     * climbs with every attempt. Immortals always succeed, same "no
     * restrictions" spirit as their other gate bypasses. */
    bool cast_ok = imm || skill_roll_success(skill_learn_from_doing(ch, sk));

    /* `wizardry` and `mana` proficiency (user 2026-08-06: "wizardry
     * should also gain automatically from casting") -- both are Mage-
     * only base skills that back EVERY cast ("the core skill of casting
     * itself" / "governs the size of your mana pool", skill.c), not any
     * one specific spell, so they train on every attempt regardless of
     * which spell was cast or whether it succeeded -- same "gain by
     * doing, win or lose" spirit `sk` itself just used above. `mana`
     * training here is also what makes being_calc_max_mana()'s pool
     * actually grow over time, mirroring real SneezyMUD's own
     * SKILL_MANA-drives-manaLimit() relationship. */
    if (!imm && ch->char_class == CLASS_DRUID) {
        /* Druid analog of the Mage mana-training block below: the
         * "lifeforce" skill trains on every cast (win or lose) and
         * drives being_calc_max_mana()s Lifeforce pool, so a
         * Druid's Lifeforce grows with use just as a Mage's mana
         * does. Recompute the max now that it may have moved. */
        const skill_def_t *lf_sk = skill_find(CLASS_DRUID, "lifeforce", false);
        if (lf_sk && lf_sk != sk)
            skill_learn_from_doing(ch, lf_sk);
        ch->progress.max_mana = being_calc_max_mana(ch);
        if (ch->progress.mana > ch->progress.max_mana)
            ch->progress.mana = ch->progress.max_mana;
    }
    if (!imm && ch->char_class == CLASS_MAGE) {
        const skill_def_t *wizardry_sk = skill_find(CLASS_MAGE, "wizardry", false);
        if (wizardry_sk && wizardry_sk != sk)
            skill_learn_from_doing(ch, wizardry_sk);
        /* `casting` (docs/Spell Assignments.xlsx gap audit, 2026-08-08)
         * -- real upstream keeps SKILL_CASTING and SKILL_WIZARDRY as two
         * genuinely separate passive stats; ported the same "trains on
         * every cast, no direct read-effect" way as `wizardry` right
         * above. */
        const skill_def_t *casting_sk = skill_find(CLASS_MAGE, "casting", false);
        if (casting_sk && casting_sk != sk)
            skill_learn_from_doing(ch, casting_sk);
        const skill_def_t *mana_sk = skill_find(CLASS_MAGE, "mana", false);
        if (mana_sk && mana_sk != sk)
            skill_learn_from_doing(ch, mana_sk);
        /* max_mana is DERIVED from the mana skill's own proficiency
         * (being_calc_max_mana()) -- recompute now that training just
         * above may have moved it, same "recompute on demand" pattern
         * being_calc_max_hp() already uses after a level-up. */
        ch->progress.max_mana = being_calc_max_mana(ch);
        if (ch->progress.mana > ch->progress.max_mana)
            ch->progress.mana = ch->progress.max_mana;
    }

    if (cast_ok) {
        /* Multi-round delay (user 2026-08-09) -- every gate above
         * (class/level/discipline/mana/component-about-to-be-consumed/
         * proficiency roll) has already passed; the effect itself is now
         * deferred 2-3 rounds via spellcast.c instead of landing
         * instantly. A failed roll (the `else` below) still fizzles
         * immediately, same as before this change -- there's no
         * "committed" cast to delay when it was never going to work. */
        /* Hand the full cost to the casting task, which draws it a
         * proportional share at a time as the rounds tick by (user
         * 2026-08-16) -- a cast broken by lost concentration only ever
         * pays for the rounds it actually spent. */
        ch->cast_mana_cost = cast_cost;
        ch->cast_mana_paid = 0;
        spellcast_start(d, ch, sk, target);
    } else {
        /* A fumbled incantation now ALSO plays out over the multi-round
         * task (user 2026-08-16: "make that failure play out over a round
         * or two too") -- a shorter 1-2 round botched cast that draws its
         * mana a slice at a time (spellcast_pay_round) and fizzles at the
         * end instead of resolving an effect. Broken mid-way (lost
         * concentration/distraction), it therefore costs only the rounds
         * it spent, exactly like the successful per-round-charged path,
         * rather than the old instant full-cost burn. Immortals never
         * reach here (imm forces cast_ok true). */
        char msg[128];
        snprintf(msg, sizeof(msg), "The words of %s tangle on your tongue...\r\n", sk->name);
        descriptor_send(d, msg);
        ch->cast_mana_cost = cast_cost;
        ch->cast_mana_paid = 0;
        spellcast_start_fumble(d, ch, sk);
    }
    consume_component(d, component);
    return true;
}
