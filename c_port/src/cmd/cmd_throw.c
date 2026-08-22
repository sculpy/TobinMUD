/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "affect.h"
#include "being.h"
#include "combat.h"
#include "obj.h"
#include "pulse.h"
#include "spellcast.h"
#include "thing.h"

/* `throw <item> <target>` -- Tobin's first thrown-weapon command
 * (user decision 2026-08-22, "build both": TODO.md's `set trap
 * (grenade)` needed a real throw mechanic to spring on, same way
 * `set trap (arrow)` needed cmd_shoot.c to exist first). Same-room,
 * same shape as cmd_shoot.c's ranged combat: throws a loose carried
 * item at a target, spending it as one-shot ammo. Two ways an item
 * counts as throwable: the real upstream WEAR_THROW flag (obj.h,
 * already seeded on 522 real items -- daggers, knives, darts, ...),
 * or a keyword-matched "grenade" item regardless of that flag (not
 * every seeded grenade item carries WEAR_THROW).
 *
 * No reload lag like `shoot` (nothing to reload) -- just a flat
 * one-round wait so it isn't free to spam. No ranged proficiency/
 * specialization bonus either; those are shoot-specific skills
 * (skill.c), not a general throwing bonus.
 *
 * Disclosed scope-down from upstream (same as cmd_shoot.c's own):
 * same-room only, and the thrown item is always destroyed on landing
 * -- Tobin objects carry no stack count and there is no recover-a-
 * missed-throw mechanic. */
#define THROW_WAIT_PULSES (COMBAT_ROUND_PULSES)
/* Bit 15 (1 << 15) of wear_flag, same verbatim upstream layout obj.c's
 * own file-local WEAR_PAIRED (bit 9) reuses -- not exported via obj.h,
 * so re-declared file-local here too. */
#define WEAR_THROW 32768

/* Case-insensitive substring test, same shape as cmd_shoot.c's own
 * file-local copy. */
static bool throw_sc_contains(const char *haystack, const char *needle) {
    if (!haystack || !needle)
        return false;
    size_t nlen = strlen(needle);
    for (const char *p = haystack; *p; p++)
        if (strncasecmp(p, needle, nlen) == 0)
            return true;
    return false;
}
static bool throw_obj_kw(const obj_t *o, const char *kw) {
    return throw_sc_contains(o->base.name, kw) || throw_sc_contains(o->base.short_descr, kw);
}
static bool is_throwable(const obj_t *o) {
    return (o->wear_flag & WEAR_THROW) || throw_obj_kw(o, "grenade");
}
/* is_loose(): not worn, not held -- same rule as cmd_shoot.c/cmd_object.c's
 * own copies. */
static bool throw_is_loose(const being_t *ch, const obj_t *o) {
    if (ch->held[0] == o || ch->held[1] == o)
        return false;
    for (int i = 0; i < LIMB_COUNT; i++)
        if (ch->equipment[i] == o)
            return false;
    return true;
}
static obj_t *find_throwable(const being_t *ch, const char *tok) {
    size_t len = strlen(tok);
    for (thing_t *t = ch->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_OBJ)
            continue;
        obj_t *o = (obj_t *)t;
        if (throw_is_loose(ch, o) && is_throwable(o) && thing_name_matches(t->name, tok, len))
            return o;
    }
    return NULL;
}

bool cmd_throw(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }
    char item_tok[64], targ_tok[64];
    int n = sscanf(args, "%63s %63s", item_tok, targ_tok);
    if (n < 1) {
        descriptor_send(d, "Usage: throw <item> <target>\r\n");
        return true;
    }
    obj_t *missile = find_throwable(ch, item_tok);
    if (!missile) {
        descriptor_send(d, "You have nothing throwable by that name.\r\n");
        return true;
    }
    /* Same open-a-fight shape as cmd_shoot/cmd_kick: a target argument
     * is required only when not already fighting. */
    if (!ch->fighting) {
        if (n < 2) {
            descriptor_send(d, "Throw it at whom?\r\n");
            return true;
        }
        if (ch->position == POSITION_SLEEPING) {
            descriptor_send(d, "You can't fight in your sleep!\r\n");
            return true;
        }
        if (being_has_affect(ch, AFFECT_FEAR)) {
            descriptor_send(d, "You're too afraid to fight!\r\n");
            return true;
        }
        ch->feigning = false;
        being_t *opener = combat_find_room_target(ch, targ_tok);
        if (!opener) {
            descriptor_send(d, "They aren't here.\r\n");
            return true;
        }
        if (ch->position != POSITION_STANDING && ch->position != POSITION_MOUNTED) {
            ch->position = POSITION_STANDING;
            descriptor_send(d, "You scramble to your feet.\r\n");
        }
        ch->fighting = opener;
        opener->fighting = ch;
        ch->sneaking = false;
        opener->sneaking = false;
    }
    being_t *target = ch->fighting;
    const char *missile_name = missile->base.short_descr[0] ? missile->base.short_descr : missile->base.name;
    being_set_wait(ch, THROW_WAIT_PULSES);
    /* Flat damage + DEX, same fallback cmd_shoot.c uses for a dice-less
     * weapon -- deliberately NOT val[0]/val[1] dice here (unlike
     * cmd_shoot.c's own attempt at that): real melee/ranged weapon
     * damage actually comes from a separate combat-mods lookup
     * (obj_load_combat_mods(), combat.c), not val[0]/val[1] -- those
     * fields hold item-type-specific data that varies wildly per
     * category (a seeded dagger's val[0] is in the thousands, not a
     * die count). GRENADE_TRAPPED also lives in val[0]'s bit 0, so
     * treating it as a dice count would corrupt the rig flag too. */
    int dmg = 2 + rand() % 5;
    dmg += (ch->attrs.dexterity - ATTR_BASE) / 3;
    if (dmg < 1)
        dmg = 1;
    char msg[384], capbuf[128];
    snprintf(msg, sizeof(msg), "You hurl %s and strike %s!\r\n",
             missile_name, being_display_name(target));
    descriptor_send(d, msg);
    if (target->desc) {
        snprintf(msg, sizeof(msg), "%s hurls %s and strikes you!\r\n",
                 being_display_name_cap(ch, capbuf, sizeof(capbuf)), missile_name);
        descriptor_send(target->desc, msg);
    }
    {
        char room[384];
        snprintf(room, sizeof(room), "%s hurls %s at %s!\r\n",
                 being_display_name_cap(ch, capbuf, sizeof(capbuf)), missile_name,
                 being_display_name(target));
        descriptor_room_echo(ch->base.roomp, ch, room);
    }
    spellcast_distract(target, 1); /* a hit rattles a caster mid-cast */
    /* `set trap (grenade)` (cmd_trap.c's `settrap grenade`) springs
     * here, on a landed hit -- same flat random-limb damage as the
     * door/arrow traps, single-use, then the item is destroyed as
     * thrown ammo normally is either way. No "detect trap" dodge
     * here either, same reasoning as the arrow trap: a throw already
     * in flight can't be spotted and stepped around. */
    if (missile->val[0] & GRENADE_TRAPPED) {
        int trap_dmg = 5 + rand() % 10;
        limb_t trap_limb = (limb_t)(rand() % LIMB_REAL_COUNT);
        int trap_hp_before = target->limbs[trap_limb].hp;
        being_hurt_limb(target, trap_limb, trap_dmg);
        descriptor_send(d, "The trap rigged into your throw springs!\r\n");
        if (target->desc) {
            char trap_msg[160];
            snprintf(trap_msg, sizeof(trap_msg),
                     "A trap rigged into the throw springs! It catches your %s %s!\r\n",
                     limb_name(trap_limb), describe_dam(trap_dmg, trap_hp_before, NULL));
            descriptor_send(target->desc, trap_msg);
        }
    }
    obj_destroy(missile);
    combat_apply_skill_damage(ch, target, dmg, LIMB_BODY);
    return true;
}
