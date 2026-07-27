/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include "being.h"

/* `wiznet <message>` / `wiznet @<level> <message>`: a private broadcast
 * channel for immortals -- the message reaches every online immortal
 * (including the sender) by default, so staff can talk out of sight of
 * mortals. The `@<level>` prefix (Sneezy → Tobin feature audit, "OOC
 * channels" -- checked Sneezy's own communication-system.md doc: real
 * `commune @<level> <message>` restricts delivery to gods at or above
 * that level) narrows delivery the same way -- e.g. `wiznet @59 <msg>`
 * reaches only Administrator+ immortals, useful for something not every
 * 51+ builder needs to see. Held for any immortal who is mid-editor
 * (descriptor_notify). Gated immortal-only by the command table. */
bool cmd_wiznet(descriptor_t *d, const char *args) {
    if (!d->character)
        return true;
    if (!args || !args[0]) {
        descriptor_send(d, "Broadcast what to the immortals?\r\n");
        return true;
    }

    int min_level = IMMORTAL_LEVEL_MIN;
    const char *text = args;
    if (args[0] == '@' && isdigit((unsigned char)args[1])) {
        min_level = atoi(args + 1);
        if (min_level < IMMORTAL_LEVEL_MIN)
            min_level = IMMORTAL_LEVEL_MIN;
        text = args + 1;
        while (isdigit((unsigned char)*text))
            text++;
        while (*text == ' ')
            text++;
        if (!*text) {
            descriptor_send(d, "Broadcast what to the immortals?\r\n");
            return true;
        }
    }

    char msg[400];
    snprintf(msg, sizeof(msg), "<p>%s:<z> %s\r\n", d->character->base.name, text);

    for (descriptor_t *it = g_descriptors; it; it = it->next) {
        if (!it->character || it->character->progress.level < min_level)
            continue;
        descriptor_notify_comm(it, msg);
    }
    return true;
}
