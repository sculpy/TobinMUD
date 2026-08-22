/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>

#include "being.h"
#include "descriptor.h"
#include "skill.h"
#include "thing.h"

/* `sign <message>` (Sneezy -> Tobin feature audit, "sign language").
 * Checked the real upstream first (docs/systems/important/
 * communication-system.md, misc/talk.cc's doSign()): silent room-only
 * speech, requiring free hands and uninjured arms, that only fellow
 * SKILL_SIGN holders actually read -- everyone else just sees a vague
 * "makes funny motions with hands" line, except a Thief signer, whose
 * hand-signs are common enough (a "stealth class exemption" in the
 * original) that anyone recognizes them.
 *
 * Every class gets `sign` (skill.c) at the same tier/level -- the
 * original lists it under DISC_ADVENTURING, a general skill every class
 * gets, not a per-class one; duplicating one entry across every class's
 * table is the same "genuinely universal skill" precedent `riding`
 * already established.
 *
 * Gating: not fighting, not asleep (same floor cmd_shout.c's listener
 * check already uses for "too out of it to notice"), both hands empty
 * (held[0]/held[1] both NULL -- signing needs your hands, not the
 * original's separate transformed-limb check, which Tobin has no
 * equivalent system for), neither arm at `limb_status_text()`'s own
 * real "hurt" threshold (<20%) or worse. Not replicated: the original's
 * POSITION_CRAWLING-exact minimum position (Tobin's `position_t` isn't
 * actively driven to CRAWLING/ENGAGED/FIGHTING values by anything
 * today, so gating on the literal enum value would be a no-op at best,
 * confusing at worst) and garble/drunk distortion. */
bool cmd_sign(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }
    if (!*args) {
        descriptor_send(d, "Yes, but WHAT do you want to sign?\r\n");
        return true;
    }
    if (!being_is_immortal(ch) && !being_knows_skill(ch, "sign")) {
        descriptor_send(d, "You don't know sign language.\r\n");
        return true;
    }
    if (ch->fighting) {
        descriptor_send(d, "You can't spare your hands for that while fighting!\r\n");
        return true;
    }
    if (ch->position <= POSITION_SLEEPING) {
        descriptor_send(d, "Not while you're asleep.\r\n");
        return true;
    }
    if (ch->held[0] || ch->held[1]) {
        descriptor_send(d, "Your hands are full.\r\n");
        return true;
    }
    if (being_limb_pct(ch, LIMB_LEFT_ARM) < 20 || being_limb_pct(ch, LIMB_RIGHT_ARM) < 20) {
        descriptor_send(d, "Your arms are too hurt to sign clearly.\r\n");
        return true;
    }

    /* Learn-by-doing: using the skill trains it toward its discipline
     * ceiling (skill_learn_from_doing() self-throttles via its own
     * cooldown). PCs only; immortals already read as maxed. */
    if (!being_is_immortal(ch) && ch->base.kind == THING_PC) {
        const skill_def_t *learn_sk = skill_find(ch->char_class, "sign", true);
        if (learn_sk)
            skill_learn_from_doing(ch, learn_sk);
    }

    char msg[336];
    snprintf(msg, sizeof(msg), "<m>You sign, \"<z>%s<m>\"<z>\r\n", args);
    descriptor_send(d, msg);

    bool signer_thief = ch->char_class == CLASS_THIEF && ch->base.kind == THING_PC;
    room_t *r = ch->base.roomp;
    for (thing_t *t = r->base.stuff_head; t; t = t->stuff_next) {
        if (t == &ch->base || t->kind != THING_PC)
            continue;
        being_t *other = (being_t *)t;
        if (!other->desc)
            continue;
        if (signer_thief || being_knows_skill(other, "sign")) {
            snprintf(msg, sizeof(msg), "<m>%s signs, \"<z>%s<m>\"<z>\r\n",
                     ch->base.name, args);
        } else {
            snprintf(msg, sizeof(msg), "<m>%s makes funny motions with %s hands.<z>\r\n",
                     ch->base.name, gender_possess(ch->gender));
        }
        descriptor_notify(other->desc, msg);
    }

    return true;
}
