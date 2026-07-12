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

#include "obj.h"
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
 * spell components") without a new object category. Consumed (destroyed)
 * on a SUCCESSFUL cast only -- a failed level/class/component check
 * costs nothing.
 *
 * v1 scope: no mana cost (Tobin has no mana/resource pool yet -- see
 * TODO.md's spell-framework backlog) and no bespoke per-spell mechanic
 * for the full ~150-entry roster (that's the "Offensive spell system"/
 * "flagship mechanics" follow-up work) -- this implements the casting
 * GATE the user asked for (class + level + component), with a small,
 * honest set of real effects (heal/damage-flavored by keyword in the
 * spell's own description) and a plain "nothing happens yet" fallback
 * for everything else in the roster, so every listed spell is at least
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

/* Applies a small, honest placeholder effect based on keywords in the
 * spell's own one-line description -- real per-spell mechanics are
 * follow-up work (see this file's header comment). */
static void task_cast(descriptor_t *d, being_t *ch, const skill_def_t *sk) {
    char msg[192];
    if (ci_contains(sk->desc, "heal")) {
        int amount = 8 + ch->progress.level / 2;
        being_heal(ch, amount);
        snprintf(msg, sizeof(msg), "You cast %s and feel restored! (+%d HP)\r\n", sk->name, amount);
        descriptor_send(d, msg);
    } else if (ch->fighting && (ci_contains(sk->desc, "damage") || ci_contains(sk->desc, "bolt")
                                 || ci_contains(sk->desc, "beam") || ci_contains(sk->desc, "blast")
                                 || ci_contains(sk->desc, "strike") || ci_contains(sk->desc, "burst")
                                 || ci_contains(sk->desc, "fury") || ci_contains(sk->desc, "flame"))) {
        int dmg = 4 + ch->progress.level / 3;
        being_t *target = ch->fighting;
        limb_t limb = (limb_t)(rand() % LIMB_COUNT);
        being_hurt_limb(target, limb, dmg);
        /* Damage numbers (user 2026-07-12): hidden from a plain mortal,
         * kept for an immortal (balancing/testing), same rule as
         * combat.c's melee messages. */
        if (being_is_immortal(ch))
            snprintf(msg, sizeof(msg), "You cast %s at %s for %d damage!\r\n",
                     sk->name, being_display_name(target), dmg);
        else
            snprintf(msg, sizeof(msg), "You cast %s at %s.\r\n",
                     sk->name, being_display_name(target));
        descriptor_send(d, msg);
        if (target->desc) {
            char tcapbuf[128];
            if (being_is_immortal(target))
                snprintf(msg, sizeof(msg), "%s casts %s at you for %d damage!\r\n",
                         being_display_name_cap(ch, tcapbuf, sizeof(tcapbuf)), sk->name, dmg);
            else
                snprintf(msg, sizeof(msg), "%s casts %s at you.\r\n",
                         being_display_name_cap(ch, tcapbuf, sizeof(tcapbuf)), sk->name);
            descriptor_notify(target->desc, msg);
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
        descriptor_send(d, "Huh?!\r\n");
        return true;
    }

    while (*args == ' ')
        args++;
    if (!*args) {
        descriptor_send(d, "Cast what? Try 'skills' to see your spells.\r\n");
        return true;
    }

    const skill_def_t *sk = find_spell(ch->char_class, args, imm);
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
        if (sk->tier == SKILL_TIER_ADVANCED &&
            (ch->progress.basic_disc_pct < 95 || ch->progress.advanced_disc_pct <= 0)) {
            descriptor_send(d, "You need 95% in your Basic discipline, and some Advanced practice, before this.\r\n");
            return true;
        }
    }

    obj_t *component = find_keyword_item(ch, "component");
    if (!component) {
        descriptor_send(d, "You don't have the spell components to cast that.\r\n");
        return true;
    }

    task_cast(d, ch, sk);
    obj_destroy(component);
    return true;
}
