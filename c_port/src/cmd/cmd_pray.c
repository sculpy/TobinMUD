/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "obj.h"
#include "skill.h"
#include "thing.h"

/* `pray <spell>` -- Cleric spellcasting (user 2026-07-11: "clerics should
 * require a holy symbol to pray successfully... implement task_pray
 * task_cast etc"). Mage/Druid's `cast` (cmd_cast.c) is the sibling
 * command, gated on a consumed component instead of a holy symbol.
 *
 * A "holy symbol" is any object anywhere in the caster's own containment
 * chain (carried, worn, or held) whose keyword list contains the word
 * "symbol" -- a generic convention, same spirit as `cast`'s "component"
 * keyword, so a builder can create any symbol item ("a holy symbol",
 * "a tarnished silver symbol") without a new object category. NOT
 * consumed -- a holy symbol is a keepsake, prayed through repeatedly,
 * unlike a mage's material component.
 *
 * v1 scope: same honest placeholder-effect approach as cmd_cast.c's
 * task_cast() -- see that file's header comment for why a full
 * per-spell mechanic isn't implemented for the whole roster yet. */

static bool ci_contains(const char *haystack, const char *needle) {
    size_t nlen = strlen(needle);
    for (const char *p = haystack; *p; p++)
        if (strncasecmp(p, needle, nlen) == 0)
            return true;
    return false;
}

static obj_t *find_keyword_item(const being_t *ch, const char *keyword) {
    size_t len = strlen(keyword);
    for (thing_t *t = ch->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind == THING_OBJ && thing_name_matches(t->name, keyword, len))
            return (obj_t *)t;
    }
    return NULL;
}

/* `any_class` (immortals -- user 2026-07-12: "immortals can use any
 * skill or spell in game, no class restrictions") searches the whole
 * roster instead of just `cls`. */
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

static void task_pray(descriptor_t *d, being_t *ch, const skill_def_t *sk) {
    char msg[192];
    if (ci_contains(sk->desc, "heal") || ci_contains(sk->desc, "cure")) {
        int amount = 8 + ch->progress.level / 2;
        being_heal(ch, amount);
        snprintf(msg, sizeof(msg), "You pray for %s and feel restored! (+%d HP)\r\n", sk->name, amount);
        descriptor_send(d, msg);
    } else if (ch->fighting && (ci_contains(sk->desc, "damage") || ci_contains(sk->desc, "bolt")
                                 || ci_contains(sk->desc, "strike"))) {
        int dmg = 4 + ch->progress.level / 3;
        being_t *target = ch->fighting;
        limb_t limb = (limb_t)(rand() % LIMB_COUNT);
        being_hurt_limb(target, limb, dmg);
        snprintf(msg, sizeof(msg), "You pray for %s, striking %s for %d damage!\r\n",
                 sk->name, being_display_name(target), dmg);
        descriptor_send(d, msg);
        if (target->desc) {
            char tcapbuf[128];
            snprintf(msg, sizeof(msg), "%s prays for %s, striking you for %d damage!\r\n",
                     being_display_name_cap(ch, tcapbuf, sizeof(tcapbuf)), sk->name, dmg);
            descriptor_notify(target->desc, msg);
        }
    } else {
        snprintf(msg, sizeof(msg),
                 "You pray for %s, but nothing happens yet -- its real effect isn't implemented.\r\n",
                 sk->name);
        descriptor_send(d, msg);
    }
}

bool cmd_pray(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch)
        return true;

    bool imm = being_is_immortal(ch);
    if (!imm && ch->char_class != CLASS_CLERIC) {
        descriptor_send(d, "Huh?!\r\n");
        return true;
    }

    while (*args == ' ')
        args++;
    if (!*args) {
        descriptor_send(d, "Pray for what? Try 'skills' to see your prayers.\r\n");
        return true;
    }

    const skill_def_t *sk = find_spell(CLASS_CLERIC, args, imm);
    if (!sk) {
        descriptor_send(d, "You don't know a prayer by that name.\r\n");
        return true;
    }
    if (!imm && ch->progress.level < sk->min_level) {
        char msg[96];
        snprintf(msg, sizeof(msg), "You aren't experienced enough to pray for %s yet (level %d).\r\n",
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

    if (!find_keyword_item(ch, "symbol")) {
        descriptor_send(d, "You need a holy symbol to pray successfully.\r\n");
        return true;
    }

    task_pray(d, ch, sk);
    return true;
}
