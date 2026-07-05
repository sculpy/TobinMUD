/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>

#include "being.h"

/* `wiznet <message>`: a private broadcast channel for immortals -- the
 * message reaches every online immortal (including the sender), so staff can
 * talk out of sight of mortals. Held for any immortal who is mid-editor
 * (descriptor_notify). Gated immortal-only by the command table. */
bool cmd_wiznet(descriptor_t *d, const char *args) {
    if (!d->character)
        return true;
    if (!args || !args[0]) {
        descriptor_send(d, "Broadcast what to the immortals?\r\n");
        return true;
    }

    char msg[400];
    snprintf(msg, sizeof(msg), "<p>%s:<z> %s\r\n", d->character->base.name, args);

    for (descriptor_t *it = g_descriptors; it; it = it->next) {
        if (!it->character || !being_is_immortal(it->character))
            continue;
        descriptor_notify(it, msg);
    }
    return true;
}
