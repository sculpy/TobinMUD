/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include "being.h"
#include "skill.h"

/* `skulk` (Unimplemented skills/spells backlog, Session 158 audit: Thief,
 * skill.c level 25). Real upstream (disc_thief_stealth.cc's doSkulk) is a
 * stealthy-movement mode that keeps a thief unnoticed as they move. Tobin
 * already has two neighbouring stealth verbs -- `sneak` (muffles your own
 * arrival/departure echo) and `hide` (drops you from a stationary room's
 * person list) -- so skulk takes the third, non-overlapping niche that
 * matches its upstream role: while skulking you can keep MOVING and still
 * avoid drawing an aggressive mob's eye (mob_try_aggress() skips a
 * skulking PC, same gate as `feign death`/`hide`). A plain toggle, same
 * shape as cmd_sneak.c; the `skulking` flag is in-memory only (no relog
 * persistence, same rule as sneaking/hiding). Broken implicitly by
 * combat -- the aggro loop only ever targets a non-fighting PC anyway. */
bool cmd_skulk(descriptor_t *d, const char *args) {
    (void)args;
    being_t *ch = d->character;
    if (!ch) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }
    if (!being_is_immortal(ch) && !being_knows_skill(ch, "skulk")) {
        descriptor_send(d, "You don't know how to skulk about unseen.\r\n");
        return true;
    }
    if (ch->fighting) {
        descriptor_send(d, "Not in the middle of a fight!\r\n");
        return true;
    }

    ch->skulking = !ch->skulking;
    if (ch->skulking)
        descriptor_send(d, "You slip into the shadows and begin skulking, keeping out of sight.\r\n");
    else
        descriptor_send(d, "You stop skulking.\r\n");

    if (ch->skulking && !being_is_immortal(ch) && ch->base.kind == THING_PC) {
        const skill_def_t *learn_sk = skill_find(ch->char_class, "skulk", true);
        if (learn_sk)
            skill_learn_from_doing(ch, learn_sk);
    }
    return true;
}
