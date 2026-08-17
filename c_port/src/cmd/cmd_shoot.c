/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
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
#include "skill.h"
#include "spellcast.h"

/* Ranged combat (Sneezy -> Tobin feature audit). STATUS.md long recorded
 * that Tobin had NO ranged/missile subsystem at all -- no shoot command,
 * no ranged bucket in weapon_verb() -- which is why Fast load and any
 * bow/arrow gameplay were deferred. This is that subsystem, kept small
 * and same-room:
 *
 *   `shoot <target>` -- fire a wielded ranged weapon (bow/crossbow/sling,
 *   detected by keyword the same substring way weapon_verb() buckets melee
 *   weapons) at a target in the room, spending one matching piece of
 *   ammunition from your inventory (arrow for a bow, bolt/quarrel for a
 *   crossbow, stone/pellet/bullet for a sling). Like `kick`, it can open
 *   a fight or hit whoever you are already fighting.
 *
 * The distinguishing mechanic vs a melee skill is the RELOAD lag: after a
 * shot you are locked out for RANGED_RELOAD_PULSES before you can act
 * again -- unless you know Fast load (below), which cuts the reload to a
 * single round. Damage comes off the weapon's own val[0]dval[1] dice plus
 * a DEX bonus, routed through combat_apply_skill_damage() like every other
 * skill strike so a kill is scored correctly.
 *
 * Disclosed scope-down from upstream: same-room only (no true firing into
 * an adjacent room / cross-room volley), and ammo is consumed one object
 * at a time (Tobin objects do not carry a stack count). */

#define RANGED_RELOAD_PULSES   (3 * COMBAT_ROUND_PULSES)
#define FAST_LOAD_RELOAD_PULSES (COMBAT_ROUND_PULSES)

/* Case-insensitive substring test, same helper shape as combat.c/room.c. */
static bool sc_contains(const char *haystack, const char *needle) {
    if (!haystack || !needle)
        return false;
    size_t nlen = strlen(needle);
    for (const char *p = haystack; *p; p++)
        if (strncasecmp(p, needle, nlen) == 0)
            return true;
    return false;
}

static bool obj_kw(const obj_t *o, const char *kw) {
    return sc_contains(o->base.name, kw) || sc_contains(o->base.short_descr, kw);
}

/* A wielded weapon counts as ranged if its keywords name a launcher. */
static bool is_ranged_weapon(const obj_t *w) {
    return w && (obj_kw(w, "bow") || obj_kw(w, "crossbow") || obj_kw(w, "sling")
                 || obj_kw(w, "arbalest"));
}

/* True if loose object o is ammunition of the kind weapon w launches. */
static bool is_ammo_for(const obj_t *w, const obj_t *o) {
    if (obj_kw(w, "crossbow") || obj_kw(w, "arbalest"))
        return obj_kw(o, "bolt") || obj_kw(o, "quarrel");
    if (obj_kw(w, "sling"))
        return obj_kw(o, "stone") || obj_kw(o, "pellet") || obj_kw(o, "bullet");
    /* plain bow */
    return obj_kw(o, "arrow");
}

/* is_loose(): not worn, not held. Same rule as cmd_object.c's own copy. */
static bool shoot_is_loose(const being_t *ch, const obj_t *o) {
    if (ch->held[0] == o || ch->held[1] == o)
        return false;
    for (int i = 0; i < LIMB_COUNT; i++)
        if (ch->equipment[i] == o)
            return false;
    return true;
}

static obj_t *find_ammo(const being_t *ch, const obj_t *w) {
    for (thing_t *t = ch->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_OBJ)
            continue;
        obj_t *o = (obj_t *)t;
        if (shoot_is_loose(ch, o) && is_ammo_for(w, o))
            return o;
    }
    return NULL;
}

bool cmd_shoot(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    obj_t *weapon = combat_wielded_weapon(ch);
    if (!is_ranged_weapon(weapon)) {
        descriptor_send(d, "You need to wield a ranged weapon -- a bow, crossbow, or sling -- to shoot.\r\n");
        return true;
    }

    obj_t *ammo = find_ammo(ch, weapon);
    if (!ammo) {
        descriptor_send(d, "You have nothing to shoot -- you are out of ammunition.\r\n");
        return true;
    }

    /* Same open-a-fight shape as cmd_kick: a target argument is required
     * only when not already fighting. */
    if (!ch->fighting) {
        char tok[64];
        if (sscanf(args, "%63s", tok) != 1) {
            descriptor_send(d, "Shoot whom?\r\n");
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
        being_t *opener = combat_find_room_target(ch, tok);
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
    const char *ammo_name = ammo->base.short_descr[0] ? ammo->base.short_descr : ammo->base.name;
    const char *weap_name = weapon->base.short_descr[0] ? weapon->base.short_descr : weapon->base.name;

    /* Reload lag -- the whole point of the subsystem. Fast load (a skill,
     * skill.c) cuts it to one round. Immortals never wait. */
    bool imm = being_is_immortal(ch);
    bool fast = false;
    if (!imm && being_knows_skill(ch, "fast load")) {
        const skill_def_t *fl = skill_find(ch->char_class, "fast load", imm);
        if (fl && skill_roll_success(skill_learn_from_doing(ch, fl)))
            fast = true;
    }
    being_set_wait(ch, fast ? FAST_LOAD_RELOAD_PULSES : RANGED_RELOAD_PULSES);

    /* Damage off the weapon's dice + DEX. Ephemeral/dice-less weapons
     * fall back to a small flat roll so a bow always does something. */
    int dice = weapon->val[0], sides = weapon->val[1];
    int dmg;
    if (dice > 0 && sides > 0) {
        dmg = 0;
        for (int i = 0; i < dice; i++)
            dmg += 1 + rand() % sides;
    } else {
        dmg = 2 + rand() % 5;
    }
    dmg += (ch->attrs.dexterity - ATTR_BASE) / 3;
    if (dmg < 1)
        dmg = 1;

    char msg[384], capbuf[128];
    snprintf(msg, sizeof(msg), "You loose %s from %s and strike %s!\r\n",
             ammo_name, weap_name, being_display_name(target));
    descriptor_send(d, msg);
    if (target->desc) {
        snprintf(msg, sizeof(msg), "%s looses %s from %s and strikes you!\r\n",
                 being_display_name_cap(ch, capbuf, sizeof(capbuf)), ammo_name, weap_name);
        descriptor_send(target->desc, msg);
    }
    {
        char room[384];
        snprintf(room, sizeof(room), "%s shoots %s at %s!\r\n",
                 being_display_name_cap(ch, capbuf, sizeof(capbuf)), ammo_name,
                 being_display_name(target));
        descriptor_room_echo(ch->base.roomp, ch, room);
    }

    spellcast_distract(target, 1); /* a hit rattles a caster mid-cast */
    obj_destroy(ammo);             /* one shot spent */
    combat_apply_skill_damage(ch, target, dmg, LIMB_BODY);
    if (fast)
        descriptor_send(d, "You nock your next shot in a blur.\r\n");
    return true;
}
