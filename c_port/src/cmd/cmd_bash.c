/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <strings.h>

#include "being.h"
#include "combat.h"
#include "obj.h"
#include "pulse.h"
#include "skill.h"
#include "spellcast.h"

/* Same local-copy convention every other cmd_*.c using this helper
 * already follows (cmd_cast.c, cmd_say.c, ...). */
static bool ci_contains(const char *haystack, const char *needle) {
    if (!haystack || !needle || !*needle)
        return false;
    size_t hlen = strlen(haystack), nlen = strlen(needle);
    if (nlen > hlen)
        return false;
    for (size_t i = 0; i + nlen <= hlen; i++)
        if (strncasecmp(haystack + i, needle, nlen) == 0)
            return true;
    return false;
}

/* True iff `ch` is currently holding a shield in either hand -- no
 * dedicated OBJ_CAT_SHIELD exists (shields are OBJ_CAT_ARMOR, held[]
 * items same as a weapon), so this matches the "shield" keyword the
 * seed data's own shield names carry (e.g. whittle.c's "simple wooden
 * shield"), same keyword-match idiom mob_ai.c's lamplighter uses for
 * "lamppost". */
static bool wielding_shield(const being_t *ch) {
    for (int i = 0; i < 2; i++) {
        obj_t *o = ch->held[i];
        if (o && ci_contains(o->base.name, "shield"))
            return true;
    }
    return false;
}

/* `bash` (Sneezy → Tobin feature audit, "Skill-based combat"). Checked
 * Sneezy's own cmd/cmd_bash.cc first: the real version is a heavy multi-
 * stage gauntlet (weight comparison vs the victim, shield/weapon-hand
 * bonuses, body-type exclusions, a self-`stumble()` on failure). Scoped
 * way down here: one `skill_roll_success()` roll (same learn-by-doing
 * shape cast/pray/peek already use), reusing the STR-based flavor of
 * combat_strike()'s own placeholder damage formula rather than porting
 * Sneezy's weight-ratio math (Tobin has no per-object/per-race weight
 * data to run it against). Warrior-only, matches Sneezy's real
 * DISC_BRAWLING gate.
 *
 * An EXTRA action layered on top of the automatic per-round exchange
 * (combat_process_run()) -- not a replacement for a normal swing.
 * `being_set_wait()` on the attacker (always, win or lose, same as
 * Sneezy's unconditional `addSkillLag()`) is what stops it from being
 * spammed every pulse; the automatic round-based combat keeps running
 * in parallel regardless, same as a real Sneezy fight where lag and the
 * round timer are separate clocks. On success, knocks the defender down
 * (POSITION_SITTING, an easier target per combat_strike()'s own non-
 * standing bonus) and costs THEM a round too (Sneezy's own "should NOT
 * cause loss of attacks, or do damage [directly]; only prevent skill-
 * use" -- so bash still deals a little limb damage here, a deliberate,
 * disclosed deviation kept for the same reason peek/kick/disarm all
 * deal at least placeholder damage or effect: an ability that does
 * literally nothing on success reads as broken, not "faithful"). */
bool cmd_bash(descriptor_t *d, const char *args) {
    (void)args;
    being_t *ch = d->character;
    if (!ch)
        return true;
    if (!ch->fighting) {
        descriptor_send(d, "Bash whom? You're not fighting anyone.\r\n");
        return true;
    }

    bool imm = being_is_immortal(ch);
    if (!imm && !being_knows_skill(ch, "bash")) {
        descriptor_send(d, "You don't know how to bash.\r\n");
        return true;
    }

    /* User 2026-08-03: "bash should only work if holding a shield".
     * Real upstream (cmd_bash.cc's isHoldingShield check) actually
     * ALLOWS a shieldless bash once advanced discipline is high enough
     * (10%/50%/90% thresholds for weapon-assisted/two-handed/barehanded
     * bash respectively) -- a graduated system Tobin's flat single-roll
     * bash has no discipline-threshold equivalent for. Tightened here
     * into a hard requirement per the user's explicit ask, a deliberate
     * simplification rather than porting the threshold ladder. */
    if (!imm && !wielding_shield(ch)) {
        descriptor_send(d, "You need to be holding a shield to bash.\r\n");
        return true;
    }

    being_t *target = ch->fighting;
    const skill_def_t *sk = skill_find(ch->char_class, "bash", imm);
    bool success = imm || !sk || skill_roll_success(skill_learn_from_doing(ch, sk));

    /* Attacker's own lag -- unconditional, same as Sneezy's addSkillLag()
     * regardless of outcome. Roughly Sneezy's LAG_3 (a heavier action
     * than a normal swing). */
    being_set_wait(ch, 2 * COMBAT_ROUND_PULSES);

    char msg[160];
    if (!success) {
        snprintf(msg, sizeof(msg), "You try to bash %s, but they twist out of the way!\r\n",
                 being_display_name(target));
        descriptor_send(d, msg);
        if (target->desc) {
            char capbuf[128];
            snprintf(msg, sizeof(msg), "%s tries to bash you, but you twist out of the way!\r\n",
                     being_display_name_cap(ch, capbuf, sizeof(capbuf)));
            descriptor_send(target->desc, msg);
        }
        return true;
    }

    snprintf(msg, sizeof(msg), "You bash into %s, knocking them to the ground!\r\n",
             being_display_name(target));
    descriptor_send(d, msg);
    if (target->desc) {
        char capbuf[128];
        snprintf(msg, sizeof(msg), "%s bashes into you, knocking you to the ground!\r\n",
                 being_display_name_cap(ch, capbuf, sizeof(capbuf)));
        descriptor_send(target->desc, msg);
    }

    int dmg = 1 + (ch->attrs.strength - ATTR_BASE) / 4 + rand() % 4;
    if (dmg < 1)
        dmg = 1;
    spellcast_distract(target, 2); /* bash distracts a caster mid-`cast` (Sneezy: bash 1-2) */
    bool defeated = combat_apply_skill_damage(ch, target, dmg, LIMB_BODY);
    if (!defeated) {
        target->position = POSITION_SITTING;
        being_set_wait(target, COMBAT_ROUND_PULSES);
    }
    return true;
}
