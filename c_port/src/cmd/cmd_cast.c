/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "affect.h"
#include "combat.h"
#include "obj.h"
#include "pulse.h"
#include "skill.h"
#include "thing.h"

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

/* Case-insensitive "does haystack contain needle" (strcasestr is GNU-only,
 * same style already duplicated in cmd_exec.c/cmd_scan.c/combat.c). */
static bool ci_contains(const char *haystack, const char *needle) {
    size_t nlen = strlen(needle);
    for (const char *p = haystack; *p; p++)
        if (strncasecmp(p, needle, nlen) == 0)
            return true;
    return false;
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
static const skill_def_t *find_spell(player_class_t cls, const char *name, bool any_class) {
    size_t len = strlen(name);
    int count = skill_count();
    for (int i = 0; i < count; i++) {
        const skill_def_t *sk = skill_at(i);
        if ((!any_class && sk->cls != cls) || sk->tier == SKILL_TIER_COMBAT)
            continue;
        if (strncasecmp(sk->name, name, len) == 0)
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
        limb_t limb = (limb_t)(rand() % LIMB_COUNT);
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
static void task_cast(descriptor_t *d, being_t *ch, being_t *target, const skill_def_t *sk) {
    char msg[192];
    /* Resolved target for the OFFENSIVE (damage) branch only: an
     * explicit target (target != ch) is used as-is; with none given,
     * falls back to whoever ch is already fighting, same as before this
     * breadth pass. NULL (no explicit target AND not fighting anyone)
     * is handled with a "who?" message rather than silently doing
     * nothing. Heal/buff branches keep using `target` directly (self by
     * default), unaffected -- see cmd_pray.c's identical atk_target for
     * the full rationale. */
    being_t *atk_target = (target != ch) ? target : ch->fighting;
    if (strcasecmp(sk->name, "cure poison") == 0) {
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
    } else if (ci_contains(sk->desc, "heal")) {
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
    } else if (ci_contains(sk->desc, "armor bonus") || ci_contains(sk->desc, "reduces incoming damage")
               || ci_contains(sk->desc, "resistance to") || ci_contains(sk->desc, "reflective shield")
               || ci_contains(sk->desc, "self-ward") || ci_contains(sk->name, "shield")
               || ci_contains(sk->name, "stone skin") || ci_contains(sk->name, "barkskin")) {
        being_apply_affect(target, AFFECT_SANCTUARY, 12);
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
    } else if (ci_contains(sk->desc, "breathe underwater")) {
        /* "gills of flesh" (Sneezy → Tobin feature audit, "Water,
         * drowning, flight") -- a real, longer-than-combat duration
         * (100 rounds, ~2 real minutes) since this is a travel-utility
         * buff meant to outlast a swim across several rooms, not a
         * combat-round ward like Sanctuary's 12. */
        being_apply_affect(ch, AFFECT_WATERBREATH, 100);
        snprintf(msg, sizeof(msg), "You cast %s -- gills split open along your neck!\r\n", sk->name);
        descriptor_send(d, msg);
    } else if (ci_contains(sk->desc, "float above the ground")) {
        being_apply_affect(ch, AFFECT_FLYING, 100);
        snprintf(msg, sizeof(msg), "You cast %s -- you rise gently off the ground!\r\n", sk->name);
        descriptor_send(d, msg);
    } else if (ci_contains(sk->desc, "area-effect")) {
        /* Real room-wide effect (breadth work) -- previously fell into
         * the single-target branch below like everything else. */
        cast_area_damage(d, ch, sk);
    } else if (ci_contains(sk->desc, "damage") || ci_contains(sk->desc, "bolt")
               || ci_contains(sk->desc, "beam") || ci_contains(sk->desc, "blast")
               || ci_contains(sk->desc, "strike") || ci_contains(sk->desc, "burst")
               || ci_contains(sk->desc, "fury") || ci_contains(sk->desc, "flame")) {
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
        limb_t limb = (limb_t)(rand() % LIMB_COUNT);
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
    } else if (ci_contains(sk->name, "conjure elemental") || strcasecmp(sk->name, "animal companion") == 0) {
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

    while (*args == ' ')
        args++;
    if (!*args) {
        descriptor_send(d, "Cast what? Try 'skills' to see your spells.\r\n");
        return true;
    }

    char target_buf[64];
    const char *target_name;
    const skill_def_t *sk = find_spell_and_target(ch->char_class, args, imm, target_buf, sizeof(target_buf), &target_name);
    if (!sk) {
        descriptor_send(d, "You don't know a spell by that name.\r\n");
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

    obj_t *component = find_keyword_item(ch, "component");
    if (!component) {
        descriptor_send(d, "You don't have the spell components to cast that.\r\n");
        return true;
    }

    /* Per-skill proficiency (Sneezy-style learn-by-doing, user 2026-07-17)
     * -- separate from the discipline-percentage ACCESS gate above, this
     * is the caster's own success chance at THIS specific spell, and it
     * climbs with every attempt. Immortals always succeed, same "no
     * restrictions" spirit as their other gate bypasses. */
    if (imm || skill_roll_success(skill_learn_from_doing(ch, sk))) {
        task_cast(d, ch, target, sk);
    } else {
        char msg[128];
        snprintf(msg, sizeof(msg), "You fumble the casting of %s -- nothing happens.\r\n", sk->name);
        descriptor_send(d, msg);
    }
    consume_component(d, component);
    return true;
}
