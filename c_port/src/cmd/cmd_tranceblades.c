/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>

#include "affect.h"
#include "being.h"
#include "pulse.h"
#include "room.h"
#include "skill.h"

/* `trance of blades` (Warrior, level 25, level-25 audit batch: "A
 * defensive stance that sharpens your reflexes at the cost of
 * offense."). Reuses the same AFFECT_SANCTUARY damage-reduction affect
 * `sanctuary`/every other Mage/Druid ward spell already shares (cmd_cast.c's
 * own doc comment: "one real shared buff, not ~30 bespoke systems") --
 * the "at the cost of offense" half is dropped, same one-directional
 * scope-cut precedent `haste` (no crit-doubling) and `curse` (no
 * paralysis-immunity half) already established for this whole audit.
 * Self-only -- a defensive STANCE, not a castable buff for allies. */
bool cmd_tranceblades(descriptor_t *d, const char *args) {
    (void)args;
    being_t *ch = d->character;
    if (!ch) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    bool imm = being_is_immortal(ch);
    if (!imm && !being_knows_skill(ch, "trance of blades")) {
        descriptor_send(d, "You don't know how to enter a trance of blades.\r\n");
        return true;
    }

    being_apply_affect(ch, AFFECT_SANCTUARY, 12);
    descriptor_send(d, "You slip into a trance of blades, your reflexes sharpening!\r\n");
    /* Learn-by-doing: using the skill trains it toward its discipline
     * ceiling (skill_learn_from_doing() self-throttles via its own
     * cooldown). PCs only; immortals already read as maxed. */
    if (!being_is_immortal(ch) && ch->base.kind == THING_PC) {
        const skill_def_t *learn_sk = skill_find(ch->char_class, "trance of blades", true);
        if (learn_sk)
            skill_learn_from_doing(ch, learn_sk);
    }
    if (ch->base.roomp) {
        char msg[128], capbuf[128];
        snprintf(msg, sizeof(msg), "%s slips into a defensive trance, blade held ready!\r\n",
                 being_display_name_cap(ch, capbuf, sizeof(capbuf)));
        descriptor_room_echo(ch->base.roomp, ch, msg);
    }
    return true;
}
