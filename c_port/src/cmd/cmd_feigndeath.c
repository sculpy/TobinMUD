/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>

#include "being.h"
#include "room.h"
#include "skill.h"

/* `feign death` (Monk, level 25, level-25 audit batch: "Play dead to
 * avoid detection or attack."). Toggles `being_t.feigning` (see its own
 * doc comment, being.h) -- checked by mob_ai.c's mob_try_aggress() so a
 * feigning PC is skipped when an aggressive mob is picking a new
 * target (the "avoid attack" half of the roster text). The "avoid
 * detection" half (hiding from `look`'s person listing, same as
 * `invisibility`) isn't ported -- feigning death is a passive,
 * temporary act, not full concealment, and the roster's own real-world
 * framing (playing dead in front of someone who's already looking at
 * you) doesn't call for it. Can't feign while already fighting --
 * breaks the fight off first, same refusal shape `cmd_yoginsa.c` uses
 * for meditate. */
bool cmd_feigndeath(descriptor_t *d, const char *args) {
    (void)args;
    being_t *ch = d->character;
    if (!ch) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    if (ch->feigning) {
        ch->feigning = false;
        descriptor_send(d, "You stop playing dead and pick yourself back up.\r\n");
        if (ch->base.roomp) {
            char msg[128], capbuf[128];
            snprintf(msg, sizeof(msg), "%s stops playing dead and picks %s back up.\r\n",
                     being_display_name_cap(ch, capbuf, sizeof(capbuf)), gender_possess(ch->gender));
            descriptor_room_echo(ch->base.roomp, ch, msg);
        }
        return true;
    }

    bool imm = being_is_immortal(ch);
    if (!imm && !being_knows_skill(ch, "feign death")) {
        descriptor_send(d, "You don't know how to convincingly play dead.\r\n");
        return true;
    }
    if (ch->fighting) {
        descriptor_send(d, "You can't feign death while fighting!\r\n");
        return true;
    }

    ch->feigning = true;
    descriptor_send(d, "You go limp and play dead.\r\n");
    if (ch->base.roomp) {
        char msg[128], capbuf[128];
        snprintf(msg, sizeof(msg), "%s suddenly goes limp and appears dead!\r\n",
                 being_display_name_cap(ch, capbuf, sizeof(capbuf)));
        descriptor_room_echo(ch->base.roomp, ch, msg);
    }
    return true;
}
