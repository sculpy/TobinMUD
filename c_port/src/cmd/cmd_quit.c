#include "cmd_internal.h"

#include <stdio.h>

/* `quit!` while playing leaves the current character and returns to the
 * account menu -- it does NOT disconnect. Only reachable via the exact,
 * full literal "quit!" (see cmd_table.c -- deliberately excluded from
 * abbreviation matching so a typo/prefix can never trigger it). To
 * actually leave the game, quit! again from the account menu (handled
 * directly in descriptor.c's CONN_ACCOUNT_MENU case, not through this
 * command dispatch path). */
bool cmd_quit(descriptor_t *d, const char *args) {
    (void)args;

    char msg[128];
    snprintf(msg, sizeof(msg), "You leave %s and return to the character menu.\r\n",
             d->character ? d->character->base.name : "the game");
    descriptor_send(d, msg);

    descriptor_leave_to_menu(d);
    return true; /* stay connected -- back at the account menu */
}
