/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "being.h"
#include "descriptor.h"

/* `reply <message>` (2026-07-26 docs/systems review -- original's
 * `doReply()`/`desc->last_teller`): sends a tell to whoever most recently
 * `tell`'d this descriptor, without retyping their name. `last_teller` is
 * pure live descriptor state (descriptor.h) -- empty until someone tells
 * you this session, and never persisted, same as the original. Delivery
 * (history log, PLR_NOTELL/ignore/PLR_AFK checks, last_teller/last_told
 * bookkeeping) is shared with `tell` via cmd_tell.c's tell_deliver(),
 * just with the target resolved from `d->last_teller` instead of a
 * typed name. */
bool cmd_reply(descriptor_t *d, const char *args) {
    if (!d->character)
        return true;
    if (d->character->pflags & PLR_MUTED) {
        descriptor_send(d, "You have been muted and cannot tell anyone.\r\n");
        return true;
    }

    if (!*args) {
        descriptor_send(d, "Reply what?\r\n");
        return true;
    }
    if (!d->last_teller[0]) {
        descriptor_send(d, "No one has told you anything yet.\r\n");
        return true;
    }

    being_t *target = NULL;
    for (descriptor_t *it = g_descriptors; it; it = it->next) {
        if (it->character && strcasecmp(it->character->base.name, d->last_teller) == 0) {
            target = it->character;
            break;
        }
    }
    if (!target) {
        char out[128];
        snprintf(out, sizeof(out), "%s is no longer in the game.\r\n", d->last_teller);
        descriptor_send(d, out);
        return true;
    }

    tell_deliver(d, target, args);
    return true;
}
