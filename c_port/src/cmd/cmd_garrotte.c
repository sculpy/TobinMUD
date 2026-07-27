/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "affect.h"
#include "being.h"
#include "combat.h"
#include "pulse.h"
#include "skill.h"
#include "thing.h"

/* `garrotte <target>` (spell/skill functional-completeness audit,
 * 2026-07-27: Thief roster entry "Strangle a victim from behind with a
 * cord.", skill.c level 1, SKILL_TIER_COMBAT). Checked the real
 * upstream's doGarrotte()/garotteMe() (disc/disc_thief_murder.cc)
 * first: it requires holding a dedicated TOOL_GARROTTE item (a specific
 * tool object type, worn onto the victim's neck slot, that wears down
 * with use and snaps after a fixed number of uses) -- Tobin has no
 * per-object tool-type system to port that faithfully, same "no per-
 * object weight/tool data" gap noted in cmd_bash.c/cmd_backstab.c's own
 * header comments. Scoped down to a bare-handed opener, same
 * "unaware/from-behind" shape as backstab (only works before either
 * side is already fighting) -- but instead of a damage multiplier, it
 * applies AFFECT_DISEASE_GARROTTE (already modeled in Tobin's affect
 * system, duration 90 matching cmd_drink.c's own DRINK_DISEASE_
 * DURATIONS table for the same disease), a real ongoing suffocation
 * effect rather than a one-time damage number -- closer to the real
 * mechanic's spirit (a strangling cord that keeps hurting until cured)
 * than backstab's instant burst. No effect against an immune/undead
 * target, matching the original's own "doesn't seem to need to
 * breathe"/"can't garrotte the undead" refusals -- Tobin has no undead
 * race/immunity flag yet, so only the sleeping-target auto-success
 * bonus (also real, "!victim->awake()") is ported. */
bool cmd_garrotte(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    char raw[64];
    if (sscanf(args, "%63s", raw) != 1) {
        descriptor_send(d, "Garrotte whom?\r\n");
        return true;
    }

    bool imm = being_is_immortal(ch);
    if (!imm && !being_knows_skill(ch, "garrotte")) {
        descriptor_send(d, "You don't know how to garrotte anyone.\r\n");
        return true;
    }
    if (ch->fighting) {
        descriptor_send(d, "You're already in a fight -- there's no sneaking up now.\r\n");
        return true;
    }

    const char *tok;
    int ordinal = thing_parse_ordinal(raw, &tok);
    size_t len = strlen(tok);

    being_t *target = NULL;
    int seen = 0;
    for (thing_t *t = ch->base.roomp->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_PC && t->kind != THING_MOB)
            continue;
        if (t == &ch->base)
            continue;
        if (thing_name_matches(t->name, tok, len)) {
            seen++;
            if (seen == ordinal) {
                target = (being_t *)t;
                break;
            }
        }
    }
    if (!target) {
        descriptor_send(d, "They aren't here.\r\n");
        return true;
    }
    if (target->fighting) {
        descriptor_send(d, "They're already alert and fighting -- you can't catch them off guard.\r\n");
        return true;
    }

    const skill_def_t *sk = skill_find(ch->char_class, "garrotte", imm);
    bool success = imm || target->position == POSITION_SLEEPING
                   || (sk && skill_roll_success(skill_learn_from_doing(ch, sk)));

    ch->fighting = target;
    target->fighting = ch;
    ch->sneaking = false;
    target->sneaking = false;
    being_set_wait(ch, 2 * COMBAT_ROUND_PULSES);

    char msg[160];
    if (!success) {
        snprintf(msg, sizeof(msg), "You try to loop a cord around %s's neck, but they twist free!\r\n",
                 being_display_name(target));
        descriptor_send(d, msg);
        if (target->desc) {
            char capbuf[128];
            snprintf(msg, sizeof(msg), "%s tries to loop a cord around your neck, but you twist free!\r\n",
                     being_display_name_cap(ch, capbuf, sizeof(capbuf)));
            descriptor_send(target->desc, msg);
        }
        return true;
    }

    snprintf(msg, sizeof(msg), "You loop a cord around %s's neck and pull it tight!\r\n",
             being_display_name(target));
    descriptor_send(d, msg);
    if (target->desc) {
        char capbuf[128];
        snprintf(msg, sizeof(msg), "%s loops a cord around your neck and pulls it tight!\r\n",
                 being_display_name_cap(ch, capbuf, sizeof(capbuf)));
        descriptor_send(target->desc, msg);
    }

    being_apply_affect(target, AFFECT_DISEASE_GARROTTE, 90);
    return true;
}
