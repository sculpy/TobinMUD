/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <strings.h>

#include "multiplay.h"

/* `multiplay [on|off]`: level 59+ -- the global switch for whether MORTALS may
 * run more than one character at once. Off by default; immortals are always
 * exempt. The setting persists (game_config table). */
bool cmd_multiplay(descriptor_t *d, const char *args) {
    if (!args || !args[0]) {
        char msg[96];
        snprintf(msg, sizeof(msg),
                 "Multiplay is currently %s.\r\nUsage: multiplay <on|off>\r\n",
                 multiplay_allowed() ? "ON" : "OFF");
        descriptor_send(d, msg);
        return true;
    }

    if (strcasecmp(args, "on") == 0) {
        multiplay_set(true);
        descriptor_send(d, "Multiplay is now ON -- mortals may run multiple characters.\r\n");
    } else if (strcasecmp(args, "off") == 0) {
        multiplay_set(false);
        descriptor_send(d, "Multiplay is now OFF -- mortals may run only one character.\r\n");
    } else {
        descriptor_send(d, "Usage: multiplay <on|off>\r\n");
    }
    return true;
}
