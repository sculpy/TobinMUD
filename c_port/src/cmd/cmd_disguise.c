/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <string.h>

#include "being.h"
#include "descriptor.h"
#include "room.h"
#include "skill.h"
#include "thing.h"

/* `disguise` (Transformation, Sneezy → Tobin feature audit -- Thief
 * "Alter your apparent identity.", skill.c). Scoped via AskUserQuestion,
 * 2026-07-26: a FIXED disguise ("a hooded stranger"), not a player-typed
 * custom description -- same "fixed form" call made for the Mage
 * polymorph spell right alongside this. Deliberately lighter than
 * polymorph: no descriptor swap, no stat transfer, no temporary mob body
 * -- just a cosmetic override of the being's OWN short_descr, which
 * cmd_look.c's room-listing (render_room_item()) already prefers over
 * `name` whenever it's non-empty. A PC's short_descr is otherwise always
 * empty (only mobs populate theirs at creation, see mob_repo.c), so
 * toggling it on/off IS the whole mechanic -- no new being_t field
 * needed. Real name still resolves normally for anyone who types it
 * directly (combat_find_room_target()/thing_name_matches() both still
 * match against `name`, untouched) -- this hides you from a passive
 * glance around the room, not from someone who already knows who you
 * are, matching the "conceals identity, not presence" spirit of the
 * skill's own one-line description rather than the heavier full body-
 * swap DisguiseStuff() gives in the real upstream (see
 * docs/systems/critical/24-transformation-system.md's own Disguise row
 * -- that also transfers stats/equipment, which this doesn't attempt).
 * A disguise persists until toggled off again -- no duration timer and
 * no auto-reveal on entering combat (disclosed simplification; refused
 * outright while ALREADY fighting, below, rather than silently allowed
 * then never broken). */
bool cmd_disguise(descriptor_t *d, const char *args) {
    (void)args;
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }
    if (!being_is_immortal(ch) && !being_knows_skill(ch, "disguise")) {
        descriptor_send(d, "You don't know how to disguise yourself.\r\n");
        return true;
    }
    if (ch->fighting) {
        descriptor_send(d, "Not while you're fighting!\r\n");
        return true;
    }

    room_t *r = ch->base.roomp;
    char msg[160];

    if (ch->base.short_descr[0]) {
        ch->base.short_descr[0] = '\0';
        descriptor_send(d, "You drop your disguise.\r\n");
        snprintf(msg, sizeof(msg), "%s drops their disguise, revealing themself as %s!\r\n",
                 "Someone", ch->base.name);
        /* The room-wide line can't name them by their OWN pre-reveal
         * short_descr (just cleared) -- but it also shouldn't lead with
         * their real name before the reveal reads naturally, so this
         * uses a generic opener. Small, disclosed wording compromise --
         * not worth carrying a second buffer just for one message. */
        descriptor_room_echo(r, ch, msg);
        return true;
    }

    snprintf(ch->base.short_descr, sizeof(ch->base.short_descr), "a hooded stranger");
    descriptor_send(d, "You pull up your hood and become a hooded stranger.\r\n");
    snprintf(msg, sizeof(msg), "%s pulls up %s hood, becoming a hooded stranger!\r\n",
             ch->base.name, gender_possess(ch->gender));
    descriptor_room_echo(r, ch, msg);
    return true;
}
