/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <ctype.h>
#include <stdio.h>

#include "being.h"
#include "combat.h"
#include "obj.h"
#include "obj_repo.h"
#include "pulse.h"
#include "skill.h"
#include "thing.h"

/* Same duplication precedent as cmd_look.c's own cap_first(). */
static const char *cap_first_local(const char *label, char *buf, size_t bufsz) {
    snprintf(buf, bufsz, "%s", label);
    if (buf[0])
        buf[0] = (char)toupper((unsigned char)buf[0]);
    return buf;
}

/* Finds whichever held weapon `combat_strike()`'s own messaging would
 * name (combat.c's static combat_wielded_weapon() isn't exported, and
 * this needs the same "dominant hand first" rule) -- duplicated locally
 * rather than exported, same small-helper duplication precedent as
 * cmd_look.c's own is_loose()/cap_first(). NULL if bare-handed. */
static obj_t *find_wielded_weapon(const being_t *b) {
    int primary = b->handed_right ? 0 : 1;
    int secondary = b->handed_right ? 1 : 0;
    if (b->held[primary] && b->held[primary]->category == OBJ_CAT_WEAPON)
        return b->held[primary];
    if (b->held[secondary] && b->held[secondary]->category == OBJ_CAT_WEAPON)
        return b->held[secondary];
    return NULL;
}

/* `disarm` (Sneezy → Tobin feature audit, "Skill-based combat"). Checked
 * Sneezy's own cmd/cmd_disarm.cc first: the real version is a multi-
 * stage gauntlet -- a defensive save check (failing it self-inflicts a
 * `SPELL_FUMBLE` -1-hitroll debuff on the CASTER), then an attack roll,
 * then a CRIT-gated final check (a mere "success" usually just fumbles
 * the victim's grip without the weapon actually leaving their hand;
 * only a crit lets it fly free, and even then shields/`SKILL_WEAPON_
 * RETENTION`/`SKILL_TRANCE_OF_BLADES` can still save it). Tobin has no
 * crit-roll layer or temporary-hitroll-debuff affect to port that
 * faithfully, so this is scoped down to one `skill_roll_success()` roll
 * (same shape as bash/kick) that either fully succeeds (weapon knocked
 * to the floor) or fully fails (no effect beyond the lag cost) --
 * matching this port's existing "one roll, not a multi-stage gauntlet"
 * convention for every other ported skill this session. Available to
 * Warrior/Thief/Monk, whichever the roster (skill.c) actually lists it
 * for at the caller's level.
 *
 * Lighter lag than bash/kick (Sneezy: LAG_2 vs LAG_3) -- disarm doesn't
 * deal damage, so it's mechanically cheaper to attempt. No knockdown,
 * no combat_apply_skill_damage() call at all. */
bool cmd_disarm(descriptor_t *d, const char *args) {
    (void)args;
    being_t *ch = d->character;
    if (!ch)
        return true;
    if (!ch->fighting) {
        descriptor_send(d, "Disarm whom? You're not fighting anyone.\r\n");
        return true;
    }

    bool imm = being_is_immortal(ch);
    if (!imm && !being_knows_skill(ch, "disarm")) {
        descriptor_send(d, "You don't know how to disarm an opponent.\r\n");
        return true;
    }

    being_t *target = ch->fighting;
    obj_t *weapon = find_wielded_weapon(target);
    if (!weapon) {
        descriptor_send(d, "They aren't even holding a weapon.\r\n");
        return true;
    }

    const skill_def_t *sk = skill_find(ch->char_class, "disarm", imm);
    bool success = imm || !sk || skill_roll_success(skill_learn_from_doing(ch, sk));

    /* `weapon retention` (Warrior, level 25, level-25 audit batch: "A
     * passive chance to keep your grip on your weapon when disarmed or
     * fumbling."). No active command of its own -- the real upstream
     * lists it as one of several passive saves disarm's own multi-stage
     * gauntlet checks (this file's own header comment already names it).
     * A defender who knows it gets one extra skill_roll_success() roll
     * to keep their weapon even after the attacker's disarm roll
     * succeeds -- not stacked onto the attacker's roll, a genuinely
     * separate defensive check. */
    if (success && !being_is_immortal(target) && being_knows_skill(target, "weapon retention")) {
        const skill_def_t *ret_sk = skill_find(target->char_class, "weapon retention", false);
        if (ret_sk && skill_roll_success(skill_learn_from_doing(target, ret_sk)))
            success = false;
    }

    /* Attacker's lag -- Sneezy's LAG_2, lighter than bash/kick's LAG_3. */
    being_set_wait(ch, COMBAT_ROUND_PULSES);

    char msg[200];
    char weapon_capbuf[128];
    const char *wlabel = weapon->base.short_descr[0] ? weapon->base.short_descr : weapon->base.name;

    if (!success) {
        snprintf(msg, sizeof(msg), "You try to disarm %s, but can't get a grip on their weapon!\r\n",
                 being_display_name(target));
        descriptor_send(d, msg);
        if (target->desc) {
            char capbuf[128];
            snprintf(msg, sizeof(msg), "%s tries to disarm you, but can't get a grip on your weapon!\r\n",
                     being_display_name_cap(ch, capbuf, sizeof(capbuf)));
            descriptor_send(target->desc, msg);
        }
        return true;
    }

    for (int i = 0; i < 2; i++)
        if (target->held[i] == weapon)
            target->held[i] = NULL;
    thing_move_to(&weapon->base, &target->base.roomp->base);
    if (target->base.kind == THING_PC)
        player_inventory_save(target->player_id, target);

    snprintf(msg, sizeof(msg), "You knock %s from %s's grip!\r\n",
             cap_first_local(wlabel, weapon_capbuf, sizeof(weapon_capbuf)), being_display_name(target));
    descriptor_send(d, msg);
    if (target->desc) {
        char capbuf[128];
        snprintf(msg, sizeof(msg), "%s knocks %s from your grip!\r\n",
                 being_display_name_cap(ch, capbuf, sizeof(capbuf)), wlabel);
        descriptor_send(target->desc, msg);
    }
    return true;
}
