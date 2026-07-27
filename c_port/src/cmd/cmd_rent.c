/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <time.h>

#include "log.h"
#include "player_repo.h"

/* `rent`: Sneezy port (user 2026-07-12: "make rent work from sneezy").
 * Per the original's own help text, renting stores your belongings and
 * cleanly ends your session -- "the RECOMMENDED way to leave the game,
 * simply dropping link is risky" -- and you regenerate HP while rented
 * out. Tobin's inventory already persists across any clean session end
 * (quit! or rent alike, via player_save()), so the real difference
 * `rent` adds is the offline HP regen: this just stamps
 * `progress.rented_at` with the current time before saving/leaving;
 * player_load() (player_repo.c) does the actual healing math the next
 * time this character logs back in, and clears the stamp.
 *
 * Deliberately NOT ported: per-item storage cost and inn/home-room
 * restriction (both blocked -- cost needs the not-yet-built Money
 * system, task 29; the room restriction needs a room-flag decision not
 * yet made) and NPC follower/pet storage (blocked on the not-yet-built
 * Pet/charm system, task 35). Available anywhere, free, PC-only for now. */
bool cmd_rent(descriptor_t *d, const char *args) {
    (void)args;
    being_t *ch = d->character;
    if (!ch)
        return true;

    if (ch->fighting) {
        descriptor_send(d, "You can't rent while fighting!\r\n");
        return true;
    }

    ch->progress.rented_at = (long)time(NULL);

    char msg[160];
    snprintf(msg, sizeof(msg),
             "You rent a room and store your belongings safely away.\r\n"
             "Your hard-earned gains are safe until you return.\r\n");
    descriptor_send(d, msg);

    if (ch->base.roomp) {
        snprintf(msg, sizeof(msg), "%s rents a room and disappears.\r\n", ch->base.name);
        descriptor_room_echo(ch->base.roomp, ch, msg);
    }
    log_info("%s has rented. [%s]", ch->base.name, descriptor_display_host(d));

    descriptor_leave_to_menu(d);
    return true;
}
