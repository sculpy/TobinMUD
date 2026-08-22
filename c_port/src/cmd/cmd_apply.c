/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "being.h"
#include "descriptor.h"
#include "pulse.h"
#include "skill.h"
#include "thing.h"

/* `apply [herbs] [target]` -- the Druid's "apply herbs" nature-heal
 * (Tier-2 port priority list; Sneezy discArray[SKILL_APPLY_HERBS], a
 * SKILL_RANGER / DISC_PLANTS skill, STAT_EXT, no mana and no reagent,
 * usable even while badly hurt). A Druid presses a poultice of gathered
 * herbs to a wound, restoring hit points to themselves or an ally in the
 * room. Real upstream ships only the discArray registration (no dedicated
 * doApplyHerbs()); this fills it in as a straightforward skill command in
 * the same shape as cmd_bandage.c -- a skill-roll gate, a level- and
 * proficiency-scaled heal, and a short recovery lag so it's steady field
 * healing rather than a spammable full-heal. Thematic companion to the
 * wild-component foraging engine (component_placement.c). Immortals always
 * succeed; no reagent is consumed (matching the spell's own COMP_0). */
bool cmd_apply(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    bool imm = being_is_immortal(ch);
    if (!imm && !being_knows_skill(ch, "apply herbs")) {
        descriptor_send(d, "You don't know how to apply healing herbs.\r\n");
        return true;
    }
    if (ch->position == POSITION_SLEEPING) {
        descriptor_send(d, "You can't tend wounds in your sleep.\r\n");
        return true;
    }

    /* Optional leading "herbs" keyword, then an optional target. */
    char a[64] = "", b[64] = "";
    sscanf(args, "%63s %63s", a, b);
    const char *tgt_tok = a;
    if (strcasecmp(a, "herbs") == 0 || strcasecmp(a, "herb") == 0)
        tgt_tok = b;

    being_t *target = ch;
    if (tgt_tok[0]) {
        being_t *found = NULL;
        size_t len = strlen(tgt_tok);
        for (thing_t *t = ch->base.roomp->base.stuff_head; t; t = t->stuff_next) {
            if ((t->kind == THING_PC || t->kind == THING_MOB)
                && thing_name_matches(t->name, tgt_tok, len)) {
                found = (being_t *)t;
                break;
            }
        }
        if (!found) {
            descriptor_send(d, "There is no one here by that name to tend.\r\n");
            return true;
        }
        target = found;
    }

    if (target->progress.hp >= target->progress.max_hp) {
        descriptor_send(d, target == ch ? "You are already in perfect health.\r\n"
                                        : "They are already in perfect health.\r\n");
        return true;
    }

    const skill_def_t *sk = skill_find(ch->char_class, "apply herbs", false);
    int prof = (imm || !sk) ? 100 : skill_learn_from_doing(ch, sk);
    bool success = imm || (sk && skill_roll_success(prof));

    being_set_wait(ch, COMBAT_ROUND_PULSES); /* brief tend; Sneezy LAG_0 + skill cooldown */

    if (!success) {
        descriptor_send(d, "Your poultice crumbles apart before it can do any good.\r\n");
        return true;
    }

    /* Heal scales with level and how practiced the herbalism is; a novice
     * still gets a useful fraction (floor 30%), a master the full effect. */
    int scale = prof < 30 ? 30 : prof;
    int heal = ((8 + ch->progress.level) * scale) / 100;
    if (heal < 1)
        heal = 1;
    heal += rand() % 4;
    being_heal(target, heal);

    char msg[200], capbuf[128];
    if (target == ch) {
        descriptor_send(d, "<g>You press a poultice of healing herbs to your wounds.<1>\r\n");
        snprintf(msg, sizeof(msg), "%s tends %s own wounds with a poultice of herbs.\r\n",
                 being_display_name_cap(ch, capbuf, sizeof(capbuf)),
                 "their");
        descriptor_room_echo(ch->base.roomp, ch, msg);
    } else {
        snprintf(msg, sizeof(msg),
                 "<g>You press a poultice of healing herbs to %s's wounds.<1>\r\n",
                 being_display_name(target));
        descriptor_send(d, msg);
        if (target->desc) {
            snprintf(msg, sizeof(msg),
                     "<g>%s presses a poultice of healing herbs to your wounds.<1>\r\n",
                     being_display_name_cap(ch, capbuf, sizeof(capbuf)));
            descriptor_send(target->desc, msg);
        }
    }
    return true;
}
