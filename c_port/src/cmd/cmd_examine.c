/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

/* `examine <target>` (Sneezy port, user 2026-07-12: "port the sneezy
 * commands consider and examine and sip and show and tell and
 * whisper"). Sneezy's own help text says it plainly: "Examine is
 * synonymous with 'look at'" -- so this is a thin wrapper around
 * cmd_look.c's `look_at_target()`, the exact same resolver `look
 * <name>` already uses, just requiring an argument (bare `examine`
 * makes no sense the way bare `look` does). */
bool cmd_examine(descriptor_t *d, const char *args) {
    if (!d->character || !d->character->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }
    while (*args == ' ')
        args++;
    if (!*args) {
        descriptor_send(d, "Examine what?\r\n");
        return true;
    }
    return look_at_target(d, args);
}
