/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

/* `catchup`: replays the real COMMUNICATION (tell/say/shout/whisper/
 * wiznet/the newbie channel) that arrived while you were in an editor
 * (redit / hedit / addnews) -- user 2026-07-26: "catchup command should
 * only record communications not theme messages". Ambient/flavor
 * messages (room echoes, combat, mob AI, weather, ...) are simply
 * dropped while editing instead, never held -- see descriptor.h's
 * descriptor_notify() vs descriptor_notify_comm() split. Held messages
 * are cleared here once read (or automatically after five minutes --
 * see descriptor_held_expire). */
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
