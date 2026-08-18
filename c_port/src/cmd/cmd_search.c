/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "being.h"
#include "pulse.h"
#include "room.h"
#include "skill.h"
#include "thing.h"

/* `search` (Unimplemented skills/spells backlog, Session 158 audit:
 * Thief "search", skill.c level 1 -- "hidden exits/items"). Real upstream
 * (task/task_search.cc) is a multi-round task turning up hidden exits and
 * concealed objects. Tobin models a hidden exit as the EXIT_COND_SECRET
 * bit (room.h) -- an exit `look`/`exits` deliberately omit but that is
 * still walkable if you know the direction (see cmd_look.c's own "still
 * walkable" note). So `search` here does the one real, useful thing that
 * bit enables: on a successful roll it reveals to the searcher which
 * directions hide a secret passage, which they can then simply walk.
 *
 * Non-destructive on purpose -- it does NOT clear the SECRET bit (that
 * would strip a builder's hidden door globally and permanently for
 * everyone the first time any Thief searched the room); the discovery is
 * the searcher's own knowledge, matching how the exit stays secret to
 * everyone else. Hidden-OBJECT discovery is a disclosed scope-cut: Tobin
 * has no per-object "concealed" flag to reveal. Costs a combat-lag round,
 * win or lose, same anti-spam brake as the other active skills. */

bool cmd_search(descriptor_t *d, const char *args) {
    (void)args;
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    bool imm = being_is_immortal(ch);
    if (!imm && !being_knows_skill(ch, "search")) {
        descriptor_send(d, "You don't know how to search for what's hidden.\r\n");
        return true;
    }

    const skill_def_t *sk = skill_find(ch->char_class, "search", imm);
    bool success = imm || !sk || skill_roll_success(skill_learn_from_doing(ch, sk));
    being_set_wait(ch, COMBAT_ROUND_PULSES);

    descriptor_send(d, "You search the area carefully...\r\n");
    if (ch->base.roomp) {
        char capbuf[128], echo[160];
        snprintf(echo, sizeof(echo), "%s searches the area carefully.\r\n",
                 being_display_name_cap(ch, capbuf, sizeof(capbuf)));
        descriptor_room_echo(ch->base.roomp, ch, echo);
    }

    if (!success) {
        descriptor_send(d, "You turn up nothing hidden.\r\n");
        return true;
    }

    room_t *r = ch->base.roomp;
    int found = 0;
    for (int i = 0; i < ROOM_NUM_EXITS; i++) {
        if (r->exits[i] < 0)
            continue;
        if (!(r->exit_cond[i] & EXIT_COND_SECRET))
            continue;
        char msg[128];
        snprintf(msg, sizeof(msg), "You discover a hidden passage to the %s!\r\n", DIR_NAMES[i]);
        descriptor_send(d, msg);
        found++;
    }
    if (!found)
        descriptor_send(d, "You turn up nothing hidden.\r\n");
    return true;
}
