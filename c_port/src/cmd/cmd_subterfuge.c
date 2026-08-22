/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <string.h>

#include "being.h"
#include "combat.h"
#include "pulse.h"
#include "room.h"
#include "skill.h"
#include "thing.h"

/* `subterfuge <target>` (Thief, level 25, level-25 audit batch: "Deceive
 * or redirect an opponent mid-combat."). The mirror image of `taunt`
 * (cmd_taunt.c, level 22): taunt PULLS a mob's aggro onto the caster;
 * subterfuge PUSHES the caster's own current opponent's aggro onto a
 * different named mob in the room instead, letting the Thief slip out
 * of a fight without an outright flee. Same mob-only restriction
 * taunt's own doc comment established (redirecting aggro onto an
 * unconsenting PC would be a PvP mechanic Tobin doesn't otherwise
 * have). Fails safe: refuses if there's no one else in the room to
 * redirect onto. */
bool cmd_subterfuge(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }
    if (!ch->fighting) {
        descriptor_send(d, "You're not fighting anyone to deceive away from you.\r\n");
        return true;
    }
    if (ch->fighting->base.kind != THING_MOB) {
        descriptor_send(d, "You can only redirect a mob's aggression this way.\r\n");
        return true;
    }

    char raw[64];
    if (sscanf(args, "%63s", raw) != 1) {
        descriptor_send(d, "Redirect your opponent onto whom?\r\n");
        return true;
    }

    bool imm = being_is_immortal(ch);
    if (!imm && !being_knows_skill(ch, "subterfuge")) {
        descriptor_send(d, "You don't know how to pull off that kind of subterfuge.\r\n");
        return true;
    }

    size_t len = strlen(raw);
    being_t *decoy = NULL;
    for (thing_t *t = ch->base.roomp->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_PC && t->kind != THING_MOB)
            continue;
        being_t *b = (being_t *)t;
        if (b == ch || b == ch->fighting)
            continue;
        if (thing_name_matches(t->name, raw, len)) {
            decoy = b;
            break;
        }
    }
    if (!decoy) {
        descriptor_send(d, "You don't see them here.\r\n");
        return true;
    }

    const skill_def_t *sk = skill_find(ch->char_class, "subterfuge", imm);
    bool success = imm || !sk || skill_roll_success(skill_learn_from_doing(ch, sk));
    being_set_wait(ch, COMBAT_ROUND_PULSES);

    char msg[160];
    if (!success) {
        descriptor_send(d, "You try to slip away and redirect their attention, but they see through it!\r\n");
        return true;
    }

    being_t *mob = ch->fighting;
    ch->fighting = NULL;
    mob->fighting = decoy;
    decoy->fighting = mob;

    snprintf(msg, sizeof(msg), "You slip out of %s's notice, leaving them fixed on %s instead!\r\n",
             being_display_name(mob), being_display_name(decoy));
    descriptor_send(d, msg);
    if (decoy->desc) {
        char capbuf[128];
        snprintf(msg, sizeof(msg), "%s deceives %s into attacking you instead!\r\n",
                 being_display_name_cap(ch, capbuf, sizeof(capbuf)), being_display_name(mob));
        descriptor_notify(decoy->desc, msg);
    }
    return true;
}
