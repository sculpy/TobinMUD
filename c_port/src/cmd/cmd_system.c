/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>

/* `system <message>`: an immortal broadcasts an atmospheric/announcement line
 * to the whole game. Everyone reads the bare message ("You hear a thud."),
 * while the sender sees it prefixed ("system You hear a thud.") as
 * confirmation. Like the INFO channel, but author-driven. Immortal-only
 * (command table gate). Held for anyone mid-editor (descriptor_notify). */
bool cmd_system(descriptor_t *d, const char *args) {
    if (!d->character)
        return true;
    if (!args || !args[0]) {
        descriptor_send(d, "Broadcast what to everyone?\r\n");
        return true;
    }

    char self[400], msg[400];
    snprintf(self, sizeof(self), "<c>system<z> %s\r\n", args);
    snprintf(msg, sizeof(msg), "%s\r\n", args);
    descriptor_send(d, self);

    for (descriptor_t *it = g_descriptors; it; it = it->next) {
        if (it == d || !it->character)
            continue;
        descriptor_notify(it, msg);
    }
    return true;
}
