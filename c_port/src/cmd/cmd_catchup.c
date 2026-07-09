/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

/* `catchup`: replays the game messages that arrived while you were in an
 * editor (redit / hedit / addnews). Those messages are held instead of
 * interrupting the editor (descriptor_notify), and cleared here once read (or
 * automatically after five minutes -- see descriptor_held_expire). */
bool cmd_catchup(descriptor_t *d, const char *args) {
    (void)args;

    if (d->held_count == 0) {
        descriptor_send(d, "You haven't missed anything.\r\n");
        return true;
    }

    descriptor_send(d, "\r\n<c>-- What you missed while editing --<z>\r\n");
    for (int i = 0; i < d->held_count; i++)
        descriptor_send(d, d->held[i].text);
    descriptor_send(d, "<c>-- end of held messages --<z>\r\n");
    d->held_count = 0;
    return true;
}
