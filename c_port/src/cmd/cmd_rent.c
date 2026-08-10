/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <time.h>

#include "log.h"
#include "player_repo.h"
#include "rent.h"

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
 * Rent now charges a level-scaled tax (SneezyMUD misc/rent.cc's
 * charge_rent_tax port -- see rent.c/rent.h): tax_at_max * level^3 /
 * MORTAL_LEVEL_MAX^3, paid from the wallet first and the bank for any
 * shortfall, free at/below RENT_FREE_LEVEL and for immortals, all tunable
 * live via `balance rent`. Deliberately NOT ported: per-item storage cost
 * (a flat per-character tax fits Tobin's single-wallet money model better
 * than the original's per-object sum), the inn/home-room restriction (a
 * room-flag decision not yet made), and NPC follower/pet storage (blocked
 * on the Pet/charm system, task 35). Available anywhere, PC-only. */
bool cmd_rent(descriptor_t *d, const char *args) {
    (void)args;
    being_t *ch = d->character;
    if (!ch)
        return true;

    if (ch->fighting) {
        descriptor_send(d, "You can't rent while fighting!\r\n");
        return true;
    }

    int cost = rent_cost_for(ch);
    int bank_used = 0;
    int paid = rent_apply_charge(ch, cost, &bank_used);
    if (paid > 0)
        player_progress_save(ch->player_id, &ch->progress);

    ch->progress.rented_at = (long)time(NULL);

    descriptor_send(d,
        "You rent a room and store your belongings safely away.\r\n"
        "Your hard-earned gains are safe until you return.\r\n");
    if (paid > 0) {
        char cmsg[128];
        if (bank_used > 0)
            snprintf(cmsg, sizeof(cmsg),
                     "The receptionist collects %d gold for your stay (%d drawn from your bank).\r\n",
                     paid, bank_used);
        else
            snprintf(cmsg, sizeof(cmsg),
                     "The receptionist collects %d gold for your stay.\r\n", paid);
        descriptor_send(d, cmsg);
    }

    if (ch->base.roomp) {
        char rmsg[128];
        snprintf(rmsg, sizeof(rmsg), "%s rents a room and disappears.\r\n", ch->base.name);
        descriptor_room_echo(ch->base.roomp, ch, rmsg);
    }
    log_info("%s has rented for %d gold (%d from bank). [%s]",
             ch->base.name, paid, bank_used, descriptor_display_host(d));

    descriptor_leave_to_menu(d);
    return true;
}
