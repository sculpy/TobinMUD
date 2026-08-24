/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>

#include "affect.h"
#include "being.h"
#include "room.h"
#include "skill.h"

/* `encamp` (missing-skill audit, generic/cross-class, Tier-3 port
 * 2026-08-16): real upstream (disc_advanced_adventuring.cc's encamp())
 * pitches a persistent camp in a natural, non-city outdoor room that
 * speeds HP/move recovery for the camper -- and, for a grouped party, for
 * their groupmates at a fraction of the camper's skill. Tobin has no
 * group-fraction hook, so this lands as a camper-only timed regen buff
 * (AFFECT_ENCAMP): while it's up, regen_tick_run() heals an extra flat
 * increment of HP and vitality each tick (see affect.h/regen.c). The
 * outdoor/nature gate reuses room_can_plant() -- the same "outdoors, not
 * water, not indoor" test forage uses -- rather than re-listing upstream's
 * long per-sector allow/deny table (a disclosed, small divergence: it
 * doesn't specially permit caves the way upstream does). The camp wears
 * off on its own (Tobin has no "break camp" plumbing) instead of lasting
 * until you leave. */
bool cmd_encamp(descriptor_t *d, const char *args) {
    (void)args;
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }
    if (!being_is_immortal(ch) && !being_knows_skill(ch, "encamp")) {
        descriptor_send(d, "You know nothing about setting up camp.\r\n");
        return true;
    }
    if (ch->fighting) {
        descriptor_send(d, "You can't make camp in the middle of a fight!\r\n");
        return true;
    }
    if (being_has_affect(ch, AFFECT_ENCAMP)) {
        descriptor_send(d, "You already have a camp set up.\r\n");
        return true;
    }
    if (!room_can_plant(ch->base.roomp)) {
        descriptor_send(d, "You need to be out in nature to make camp.\r\n");
        return true;
    }

    const skill_def_t *sk = skill_find(ch->char_class, "encamp",
                                       being_is_immortal(ch));
    int pct = being_is_immortal(ch) ? 100
              : (sk ? skill_learn_from_doing(ch, sk) : 0);

    /* A clumsy camp still works, just for a shorter spell (half the
     * duration) -- upstream's failed roll likewise halves the camp's
     * effective level, a weaker-but-not-useless camp. */
    int rounds = skill_roll_success(pct) ? ENCAMP_DURATION_ROUNDS
                                         : ENCAMP_DURATION_ROUNDS / 2;
    being_apply_affect(ch, AFFECT_ENCAMP, rounds);

    if (rounds == ENCAMP_DURATION_ROUNDS)
        descriptor_send(d, "You stop and set up a snug camp; it'll be good to rest here.\r\n");
    else
        descriptor_send(d, "You stop and throw together a rough camp -- it'll do for a while.\r\n");

    char cap[128], msg[192];
    being_display_name_cap(ch, cap, sizeof(cap));
    snprintf(msg, sizeof(msg), "%s stops and sets up camp.\r\n", cap);
    descriptor_room_echo(ch->base.roomp, ch, msg);
    return true;
}
