/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
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
#include "obj_magic_repo.h"
#include "pulse.h"
#include "skill.h"
#include "thing.h"

/* `use <item> [target]` -- scrolls, wands, and staves (Magic items,
 * Sneezy -> Tobin feature audit). Checked the original's own doc first
 * (scrolls: single-use, up to three spells; wands: rechargeable, player-
 * targeted; staves: rechargeable, room-wide) -- scoped to Tobin's real
 * infrastructure: ONE spell per item (obj_magic.sql, a fresh Tobin table
 * -- the real upstream val[] fields on these items turned out to be
 * unreliable import noise, see that file's own comment), any character
 * can use one regardless of class/level (matches the original: "any
 * character use stored spells"), and the effect itself reuses the SAME
 * generic category dispatch `cast`/`pray` already have (heal/buff/
 * damage/area, keyed off the spell's own description) rather than a
 * third full copy of task_cast()/task_pray() -- see apply_item_effect()
 * below for the (deliberately smaller) subset covered. No mana cost
 * (nothing in Tobin has one yet), no recharge command for wands/staves
 * yet (an empty one just sits inert until a future `edobject`-style tool
 * exists to refill it) -- an honest Tobin-scale slice of a large
 * original system, not the full thing. */

/* Case-insensitive "does haystack contain needle" -- same duplicated
 * helper as cmd_cast.c/cmd_pray.c. */
static bool ci_contains(const char *haystack, const char *needle) {
    size_t nlen = strlen(needle);
    for (const char *p = haystack; *p; p++)
        if (strncasecmp(p, needle, nlen) == 0)
            return true;
    return false;
}

/* Looks through everything `ch` is carrying, wearing, or holding for an
 * item whose name/keywords contain `keyword` -- same duplicated pattern
 * as cmd_cast.c's find_keyword_item(), just without the being_t owner
 * distinction that one needs. */
static obj_t *find_item(const being_t *ch, const char *tok) {
    size_t len = strlen(tok);
    for (thing_t *t = ch->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind == THING_OBJ && thing_name_matches(t->name, tok, len))
            return (obj_t *)t;
    }
    return NULL;
}

/* Rough damage roll for a magic item's spell, scaled by the underlying
 * skill's min_level -- shared by the area-damage staff path and
 * apply_item_effect()'s single-target damage branch so both scale the
 * same way. */
static int spell_damage_for_level(int min_level) {
    return 4 + min_level + (rand() % (min_level / 3 + 4));
}

/* Real room-wide effect for a staff -- identical shape to cmd_cast.c's
 * cast_area_damage()/cmd_pray.c's pray_area_damage() (duplicated per this
 * codebase's small-static-helper convention), just with a fixed "the
 * item" phrasing instead of "you cast %s". */
static void use_area_damage(descriptor_t *d, being_t *ch, const skill_def_t *sk, const char *item_label) {
    if (!ch->base.roomp) {
        descriptor_send(d, "You aren't anywhere.\r\n");
        return;
    }
    int dmg = spell_damage_for_level(sk->min_level);
    int hit_count = 0;
    room_t *r = ch->base.roomp;
    for (thing_t *t = r->base.stuff_head; t; ) {
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
    char msg[192];
    if (hit_count > 0)
        snprintf(msg, sizeof(msg), "You use %s, unleashing %s -- it catches everyone nearby!\r\n", item_label, sk->name);
    else
        snprintf(msg, sizeof(msg), "You use %s, unleashing %s, but there's no one else here to catch in it.\r\n",
                 item_label, sk->name);
    descriptor_send(d, msg);
    if (hit_count > 0) {
        snprintf(msg, sizeof(msg), "%s uses %s, unleashing %s -- it catches everyone nearby!\r\n",
                 being_display_name(ch), item_label, sk->name);
        descriptor_room_echo(r, ch, msg);
    }
}

/* Applies one magic item's stored spell effect -- a smaller subset of
 * task_cast()/task_pray()'s category dispatch (heal, protective ward,
 * single-target damage; area handled separately by the staff branch in
 * cmd_use() before this is even called). Deliberately NOT covering cure-
 * poison/disease or the water-breathing/flying utility spells here --
 * those feel like a stretch for a "point this wand and zap" item and
 * would just bloat this further; an honest scope-down, not an oversight.
 * `target` is already resolved by the caller (self for heal/buff, an
 * explicit or ch->fighting-fallback target for damage). */
static void apply_item_effect(descriptor_t *d, being_t *ch, being_t *target,
                              const skill_def_t *sk, const char *item_label) {
    char msg[224];
    if (ci_contains(sk->desc, "heal")) {
        int amount = 8 + sk->min_level / 2;
        being_heal(target, amount);
        if (target == ch)
            snprintf(msg, sizeof(msg), "You use %s -- %s heals you! (+%d HP)\r\n", item_label, sk->name, amount);
        else
            snprintf(msg, sizeof(msg), "You use %s on %s -- %s heals them! (+%d HP)\r\n",
                     item_label, being_display_name(target), sk->name, amount);
        descriptor_send(d, msg);
        if (target != ch && target->desc) {
            char tcapbuf[128];
            snprintf(msg, sizeof(msg), "%s uses %s on you -- %s heals you! (+%d HP)\r\n",
                     being_display_name_cap(ch, tcapbuf, sizeof(tcapbuf)), item_label, sk->name, amount);
            descriptor_notify(target->desc, msg);
        }
    } else if (ci_contains(sk->desc, "armor bonus") || ci_contains(sk->desc, "reduces incoming damage")
               || ci_contains(sk->desc, "resistance to") || ci_contains(sk->desc, "reflective shield")
               || ci_contains(sk->desc, "self-ward") || ci_contains(sk->name, "shield")
               || ci_contains(sk->name, "stone skin") || ci_contains(sk->name, "barkskin")) {
        being_apply_affect(target, AFFECT_SANCTUARY, 12);
        if (target == ch)
            snprintf(msg, sizeof(msg), "You use %s -- %s wards you!\r\n", item_label, sk->name);
        else
            snprintf(msg, sizeof(msg), "You use %s on %s -- %s wards them!\r\n",
                     item_label, being_display_name(target), sk->name);
        descriptor_send(d, msg);
        if (target != ch && target->desc)
            descriptor_notify(target->desc, "A protective ward settles over you!\r\n");
    } else if (ci_contains(sk->desc, "damage") || ci_contains(sk->desc, "bolt") || ci_contains(sk->desc, "beam")
               || ci_contains(sk->desc, "blast") || ci_contains(sk->desc, "strike") || ci_contains(sk->desc, "burst")
               || ci_contains(sk->desc, "fury") || ci_contains(sk->desc, "flame")) {
        if (target == ch) {
            descriptor_send(d, "Use that on whom?\r\n");
            return;
        }
        if (!ch->fighting) {
            ch->fighting = target;
            target->fighting = ch;
            being_set_wait(ch, COMBAT_ROUND_PULSES);
        }
        int dmg = spell_damage_for_level(sk->min_level);
        /* LIMB_REAL_COUNT, not LIMB_COUNT (Limbs -> wearSlotT, 2026-07-26)
         * -- excludes the mob-only, always-inactive EX_* placeholder
         * slots from ever being randomly hit. */
        limb_t limb = (limb_t)(rand() % LIMB_REAL_COUNT);
        int limb_hp_before = target->limbs[limb].hp;
        bool defeated = combat_apply_skill_damage(ch, target, dmg, limb);
        const char *intensity = describe_dam(dmg, limb_hp_before, NULL);
        snprintf(msg, sizeof(msg), "You use %s -- %s hits %s %s!\r\n",
                 item_label, sk->name, being_display_name(target), intensity);
        descriptor_send(d, msg);
        if (!defeated && target->desc) {
            char tcapbuf[128];
            snprintf(msg, sizeof(msg), "%s uses %s -- %s hits you %s!\r\n",
                     being_display_name_cap(ch, tcapbuf, sizeof(tcapbuf)), item_label, sk->name, intensity);
            descriptor_notify(target->desc, msg);
        }
    } else {
        snprintf(msg, sizeof(msg), "You use %s, but nothing happens -- %s's effect isn't one this can channel yet.\r\n",
                 item_label, sk->name);
        descriptor_send(d, msg);
    }
}

/* `use <item> [target]` command -- see file-top comment for the full
 * scope. Validates the item is a scroll/wand/staff with a real
 * obj_magic_repo spell row and (for wand/staff) remaining charges,
 * resolves the target (room-wide for staves via use_area_damage(), a
 * named/self/fighting-fallback target otherwise via
 * apply_item_effect()), then consumes a charge or destroys a scroll
 * outright. */
bool cmd_use(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch)
        return true;

    char tok[64];
    int consumed = 0;
    if (sscanf(args, "%63s %n", tok, &consumed) != 1) {
        descriptor_send(d, "Use what?\r\n");
        return true;
    }
    const char *target_name = args + consumed;
    while (*target_name == ' ')
        target_name++;

    obj_t *o = find_item(ch, tok);
    if (!o) {
        descriptor_send(d, "You aren't carrying that.\r\n");
        return true;
    }

    /* Raw upstream itemTypeT: 2=ITEM_SCROLL, 3=ITEM_WAND, 4=ITEM_STAFF --
     * see obj.h's o->raw_type doc comment. Potions (10) aren't wired up
     * yet (a stretch for `use` -- `drink`/`quaff` would fit better, and
     * that's a separate command entirely) -- an honest scope-down. */
    if (o->category != OBJ_CAT_MAGIC_DEVICE
        || (o->raw_type != 2 && o->raw_type != 3 && o->raw_type != 4)) {
        descriptor_send(d, "You can't use that.\r\n");
        return true;
    }

    char spell_name[OBJ_MAGIC_SPELL_NAME_LEN];
    int max_charges;
    if (!obj_magic_repo_get(o->vnum, spell_name, sizeof(spell_name), &max_charges)) {
        descriptor_send(d, "Nothing happens -- it doesn't seem to be magical after all.\r\n");
        return true;
    }

    bool is_scroll = o->raw_type == 2;
    bool is_staff = o->raw_type == 4;
    const char *label = o->base.short_descr[0] ? o->base.short_descr : o->base.name;

    if (!is_scroll && o->val[0] <= 0) {
        char msg[224];
        snprintf(msg, sizeof(msg), "%s has no charges left.\r\n", label);
        msg[0] = (char)toupper((unsigned char)msg[0]);
        descriptor_send(d, msg);
        return true;
    }

    const skill_def_t *sk = skill_find(CLASS_MAGE, spell_name, true);
    if (!sk) {
        char msg[224];
        snprintf(msg, sizeof(msg), "%s fizzles -- whatever it once channeled is gone from the world.\r\n", label);
        descriptor_send(d, msg);
        return true;
    }

    /* `read magic` (missing-skill audit, generic/cross-class,
     * 2026-08-10): real upstream (other.cc's doRecite) gates a scroll's
     * success on SKILL_READ_MAGIC, Mages effectively always succeeding.
     * Ported the same way for SCROLLS: deciphering rolls read magic
     * (learn-by-doing); a Mage's own training always succeeds; immortals
     * are exempt. On a failed roll the arcane script won't fire -- but,
     * unlike upstream (which consumes the scroll on a flub), Tobin does
     * NOT destroy it (a disclosed, player-kinder scope-cut). Wands/staves
     * are devices, not written script, so they are unaffected. */
    if (is_scroll && !being_is_immortal(ch) && ch->char_class != CLASS_MAGE) {
        const skill_def_t *rm_sk = skill_find(ch->char_class, "read magic", true);
        if (rm_sk && !skill_roll_success(skill_learn_from_doing(ch, rm_sk))) {
            char rmsg[224];
            snprintf(rmsg, sizeof(rmsg), "You stumble over the arcane script on %s; the magic doesn't fire.\r\n", label);
            descriptor_send(d, rmsg);
            return true;
        }
    }

    if (is_staff) {
        use_area_damage(d, ch, sk, label);
    } else {
        /* Same target-resolution shape as `cast`/`pray`: an explicit name
         * is looked up in the room; with none given, heal/buff spells
         * default to self while offensive ones fall back to whoever ch
         * is already fighting (apply_item_effect()'s own "Use that on
         * whom?" catches the remaining "no target, not fighting" case). */
        being_t *target = ch;
        if (*target_name) {
            target = combat_find_room_target(ch, target_name);
            if (!target) {
                descriptor_send(d, "You don't see them here.\r\n");
                return true;
            }
        } else if (ch->fighting && (ci_contains(sk->desc, "damage") || ci_contains(sk->desc, "bolt")
                                     || ci_contains(sk->desc, "beam") || ci_contains(sk->desc, "blast")
                                     || ci_contains(sk->desc, "strike") || ci_contains(sk->desc, "burst")
                                     || ci_contains(sk->desc, "fury") || ci_contains(sk->desc, "flame"))) {
            target = ch->fighting;
        }
        apply_item_effect(d, ch, target, sk, label);
    }

    if (is_scroll) {
        char msg[224];
        snprintf(msg, sizeof(msg), "%s crumbles to dust.\r\n", label);
        msg[0] = (char)toupper((unsigned char)msg[0]);
        descriptor_send(d, msg);
        obj_destroy(o);
    } else {
        o->val[0]--;
    }
    return true;
}
