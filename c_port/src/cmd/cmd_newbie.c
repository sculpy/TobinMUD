/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>

#include "being.h"

/* `newbie <message>`: a help channel for new players. Reaches everyone who
 * has the newbie flag on (PLR_NEWBIE) -- on by default, so newcomers see it
 * automatically, and veterans can keep it on to answer questions or turn it
 * off with `toggle newbie`. You must be on the channel to speak on it. */
bool cmd_newbie(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch)
        return true;

    while (*args == ' ')
        args++;
    if (!*args) {
        descriptor_send(d, "Usage: newbie <message>   (the newbie help channel)\r\n");
        return true;
    }
    if (!(ch->pflags & PLR_NEWBIE)) {
        descriptor_send(d,
            "You have left the newbie channel. Type `toggle newbie` to rejoin.\r\n");
        return true;
    }

    char self[600];
    snprintf(self, sizeof(self), "<g>[newbie]<z> You: %s\r\n", args);
    descriptor_send(d, self);

    char other[600];
    snprintf(other, sizeof(other), "<g>[newbie]<z> %s: %s\r\n", ch->base.name, args);
    for (descriptor_t *it = g_descriptors; it; it = it->next) {
        if (it == d || !it->character)
            continue;
        if (!(it->character->pflags & PLR_NEWBIE))
            continue;
        descriptor_notify_comm(it, other); /* held if the listener is mid-editor */
    }
    return true;
}
