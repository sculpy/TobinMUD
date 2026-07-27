/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>

#include "being.h"
#include "log.h"
#include "obj.h"
#include "obj_repo.h"
#include "thing.h"

/* `quit!` while playing leaves the current character and returns to the
 * account menu -- it does NOT disconnect. Only reachable via the exact,
 * full literal "quit!" (see cmd_table.c -- deliberately excluded from
 * abbreviation matching so a typo/prefix can never trigger it). To
 * actually leave the game, quit! again from the account menu (handled
 * directly in descriptor.c's CONN_ACCOUNT_MENU case, not through this
 * command dispatch path). */
bool cmd_quit(descriptor_t *d, const char *args) {
    (void)args;

    /* `possess`/`return` (cmd_possess.c): quitting out from inside a
     * puppeted mob would try to player_save()/drop-items a being that
     * isn't a real player row at all. Simplest, safest guard -- `return`
     * to your own body first. */
    if (d->possess_original) {
        descriptor_send(d, "You're possessing a mob -- `return` to your own body first.\r\n");
        return true;
    }

    char msg[128];
    snprintf(msg, sizeof(msg), "You leave %s and return to the character menu.\r\n",
             d->character ? d->character->base.name : "the game");
    descriptor_send(d, msg);

    /* The room shouldn't watch someone silently evaporate (user
     * requirement). Announced before leave_to_menu frees the character. */
    if (d->character && d->character->base.roomp) {
        snprintf(msg, sizeof(msg), "%s has left the game.\r\n", d->character->base.name);
        descriptor_room_echo(d->character->base.roomp, d->character, msg);
    }

    /* Quitting drops everything on the floor, gold included (user,
     * 2026-07-12: "after rent goes in quitting the game will drop all
     * possessions on the ground where the quit command was executed,
     * gold included") -- now that `rent` (cmd_rent.c) exists as the
     * safe way to leave with belongings intact, plain quit! is the
     * risky option Sneezy's own `rent` help text warns about. Carried,
     * worn, and held items are all the same `stuff_head` chain (see
     * combat_defeat()'s corpse population for the identical pattern);
     * gold itself has no field to drop yet -- there is no Money system
     * (TODO.md task 29) -- so this covers items only until one exists.
     *
     * Immortals instead get everything PURGED outright (user, 2026-07-26:
     * "immortal inventory and worn items must be purged upon quit!") --
     * an immortal's inventory is almost always test/debug props (`load`),
     * and leaving those scattered on the ground on every quit is clutter
     * a mortal's real belongings shouldn't be treated the same as. */
    if (d->character && d->character->base.roomp) {
        being_t *ch = d->character;
        bool is_imm = being_is_immortal(ch);
        int affected = 0;
        thing_t *t = ch->base.stuff_head;
        while (t) {
            thing_t *next = t->stuff_next;
            if (t->kind == THING_OBJ) {
                if (is_imm)
                    obj_destroy((obj_t *)t);
                else
                    thing_move_to(t, &ch->base.roomp->base);
                affected++;
            }
            t = next;
        }
        for (int i = 0; i < LIMB_COUNT; i++)
            ch->equipment[i] = NULL;
        for (int i = 0; i < 2; i++)
            ch->held[i] = NULL;
        if (affected > 0) {
            player_inventory_save(ch->player_id, ch);
            if (is_imm) {
                descriptor_send(d, "Your belongings vanish as you leave.\r\n");
            } else {
                snprintf(msg, sizeof(msg), "%s's belongings spill onto the ground!\r\n", ch->base.name);
                descriptor_room_echo(ch->base.roomp, ch, msg);
                descriptor_send(d, "Your belongings spill onto the ground as you leave!\r\n");
            }
        }
    }

    if (d->character)
        log_info("%s has left the game. [%s]", d->character->base.name,
                 descriptor_display_host(d));

    descriptor_leave_to_menu(d);
    return true; /* stay connected -- back at the account menu */
}
