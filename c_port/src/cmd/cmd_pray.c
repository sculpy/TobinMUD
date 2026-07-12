/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "affect.h"
#include "combat.h"
#include "obj.h"
#include "skill.h"
#include "thing.h"

/* `pray <spell> [target]` -- Cleric spellcasting (user 2026-07-11:
 * "clerics should require a holy symbol to pray successfully...
 * implement task_pray task_cast etc"). Mage/Druid's `cast` (cmd_cast.c)
 * is the sibling command, gated on a consumed component instead of a
 * holy symbol.
 *
 * A "holy symbol" is any object anywhere in the caster's own containment
 * chain (carried, worn, or held) whose keyword list contains the word
 * "symbol" -- a generic convention, same spirit as `cast`'s "component"
 * keyword, so a builder can create any symbol item ("a holy symbol",
 * "a tarnished silver symbol") without a new object category. CONSUMED
 * on every successful pray (user 2026-07-12: "holy symbols should use
 * the same logic as components for mages and druids") -- no longer a
 * permanent keepsake as originally shipped.
 *
 * Healing prayers ("heal light" etc) may now target someone else in the
 * room ("pray heal light <target>") instead of only the caster -- see
 * find_spell_and_target() below for how the optional trailing target
 * name is told apart from a (possibly multi-word, possibly abbreviated)
 * spell name. A successful heal-type prayer is remembered on the
 * caster (being_t.last_heal_target/last_heal_spell) so the new
 * `continue` command (cmd_continue.c) can keep re-praying it
 * automatically until the target is fully healed or the caster's holy
 * symbols run out ("their holy symbol breaks").
 *
 * v1 scope: same honest placeholder-effect approach as cmd_cast.c's
 * task_cast() -- see that file's header comment for why a full
 * per-spell mechanic isn't implemented for the whole roster yet. */

/* Case-insensitive "does haystack contain needle" -- used to spot
 * keywords like "heal" or "damage" inside a spell's description text. */
static bool ci_contains(const char *haystack, const char *needle) {
    size_t nlen = strlen(needle);
    for (const char *p = haystack; *p; p++)
        if (strncasecmp(p, needle, nlen) == 0)
            return true;
    return false;
}

/* Looks through everything `ch` is carrying, wearing, or holding for
 * an item whose name/keywords contain `keyword` -- used here to find
 * a holy symbol. Returns NULL if there isn't one. */
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

/* Splits `args` into a spell (possibly abbreviated, possibly multi-word)
 * and an optional trailing target name. Tries the WHOLE string against
 * find_spell() first -- if that matches, there's no target (self-pray,
 * the original/common case, left byte-for-byte backward compatible).
 * Only if that fails does it peel off the last word and retry the
 * remainder as the spell name, treating the peeled word as a target
 * ("pray heal light joe" -> spell "heal light", target "joe"; "pray
 * heal lig joe" -> abbreviated "heal lig" still matches "heal light",
 * target "joe"). `*out_target` is set to NULL for a self-pray. */
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

/* Applies a heal-type prayer's effect to `target` (may be `ch` itself)
 * and reports it to both. cmd_continue.c duplicates this same formula/
 * messaging rather than depending on this file, matching this
 * codebase's established "small static helpers get duplicated per
 * command file" convention (see ci_contains/find_keyword_item above,
 * also duplicated in cmd_cast.c). */
static void pray_apply_heal(descriptor_t *d, being_t *ch, being_t *target, const char *spell_name) {
    int amount = 8 + ch->progress.level / 2;
    being_heal(target, amount);
    char msg[192];
    if (target == ch) {
        snprintf(msg, sizeof(msg), "You pray for %s and feel restored! (+%d HP)\r\n", spell_name, amount);
        descriptor_send(d, msg);
    } else {
        char tcapbuf[128];
        snprintf(msg, sizeof(msg), "You pray for %s, and %s is restored! (+%d HP)\r\n",
                 spell_name, being_display_name(target), amount);
        descriptor_send(d, msg);
        if (target->desc) {
            snprintf(msg, sizeof(msg), "%s prays for %s, restoring you! (+%d HP)\r\n",
                     being_display_name_cap(ch, tcapbuf, sizeof(tcapbuf)), spell_name, amount);
            descriptor_notify(target->desc, msg);
        }
    }
}

/* Works out what a successful prayer actually does: heals the target
 * if the spell's description mentions healing, damages the caster's
 * current opponent if it mentions an attack, or otherwise just prints
 * an honest "nothing happens yet" placeholder. Remembers heal-type
 * prayers so `continue` (cmd_continue.c) can keep repeating them. */
static void task_pray(descriptor_t *d, being_t *ch, being_t *target, const skill_def_t *sk) {
    char msg[192];
    if (ci_contains(sk->desc, "heal") || ci_contains(sk->desc, "cure")) {
        pray_apply_heal(d, ch, target, sk->name);
        ch->last_heal_target = target;
        snprintf(ch->last_heal_spell, sizeof(ch->last_heal_spell), "%s", sk->name);
    } else if (ci_contains(sk->desc, "reduces incoming damage")) {
        /* Affects system (user 2026-07-11's "buffs/debuffs/status"
         * backlog item) -- flagship example: "sanctuary"'s own
         * description ("A strong aura that reduces incoming damage.")
         * now actually does that (combat.c's combat_strike() halves
         * damage against anyone with AFFECT_SANCTUARY active). */
        ch->last_heal_target = NULL;
        being_apply_affect(target, AFFECT_SANCTUARY, 12);
        if (target == ch) {
            snprintf(msg, sizeof(msg), "You pray for %s -- a shimmering aura surrounds you!\r\n", sk->name);
            descriptor_send(d, msg);
        } else {
            snprintf(msg, sizeof(msg), "You pray for %s over %s -- a shimmering aura surrounds them!\r\n",
                     sk->name, being_display_name(target));
            descriptor_send(d, msg);
            if (target->desc)
                descriptor_notify(target->desc, "A shimmering aura surrounds you!\r\n");
        }
    } else if (ch->fighting && (ci_contains(sk->desc, "damage") || ci_contains(sk->desc, "bolt")
                                 || ci_contains(sk->desc, "strike"))) {
        ch->last_heal_target = NULL;
        int dmg = 4 + ch->progress.level / 3;
        being_t *foe = ch->fighting;
        limb_t limb = (limb_t)(rand() % LIMB_COUNT);
        being_hurt_limb(foe, limb, dmg);
        /* Damage numbers (user 2026-07-12): hidden from a plain mortal,
         * kept for an immortal (balancing/testing), same rule as
         * combat.c's melee messages. */
        if (being_is_immortal(ch))
            snprintf(msg, sizeof(msg), "You pray for %s, striking %s for %d damage!\r\n",
                     sk->name, being_display_name(foe), dmg);
        else
            snprintf(msg, sizeof(msg), "You pray for %s, striking %s.\r\n",
                     sk->name, being_display_name(foe));
        descriptor_send(d, msg);
        if (foe->desc) {
            char tcapbuf[128];
            if (being_is_immortal(foe))
                snprintf(msg, sizeof(msg), "%s prays for %s, striking you for %d damage!\r\n",
                         being_display_name_cap(ch, tcapbuf, sizeof(tcapbuf)), sk->name, dmg);
            else
                snprintf(msg, sizeof(msg), "%s prays for %s, striking you.\r\n",
                         being_display_name_cap(ch, tcapbuf, sizeof(tcapbuf)), sk->name);
            descriptor_notify(foe->desc, msg);
        }
    } else {
        ch->last_heal_target = NULL;
        snprintf(msg, sizeof(msg),
                 "You pray for %s, but nothing happens yet -- its real effect isn't implemented.\r\n",
                 sk->name);
        descriptor_send(d, msg);
    }
}

/* Runs the `pray` command: checks the caster is allowed to pray the
 * named spell (class, level, discipline practice), makes sure they
 * have a holy symbol on hand, then applies the spell's effect. See
 * this file's header comment for the full rules. */
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

    char target_buf[64];
    const char *target_name;
    const skill_def_t *sk = find_spell_and_target(CLASS_CLERIC, args, imm, target_buf, sizeof(target_buf), &target_name);
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

    being_t *target = ch;
    if (target_name) {
        target = combat_find_room_target(ch, target_name);
        if (!target) {
            descriptor_send(d, "You don't see them here.\r\n");
            return true;
        }
    }

    obj_t *symbol = find_keyword_item(ch, "symbol");
    if (!symbol) {
        descriptor_send(d, "You need a holy symbol to pray successfully.\r\n");
        return true;
    }

    task_pray(d, ch, target, sk);
    obj_destroy(symbol);
    return true;
}
