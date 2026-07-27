/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "being.h"
#include "descriptor.h"
#include "ignore_repo.h"
#include "tell_history_repo.h"

/* `reply <message>` (2026-07-26 docs/systems review -- original's
 * `doReply()`/`desc->last_teller`): sends a tell to whoever most recently
 * `tell`'d this descriptor, without retyping their name. `last_teller` is
 * pure live descriptor state (descriptor.h) -- empty until someone tells
 * you this session, and never persisted, same as the original. Delivery
 * mirrors cmd_tell.c exactly (ignore check, history log, last_teller
 * chain), just with the target resolved from `d->last_teller` instead of
 * a typed name. */
bool cmd_reply(descriptor_t *d, const char *args) {
    if (!d->character)
        return true;

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

    char out[400];
    snprintf(out, sizeof(out), "<p>You tell %s, \"<z>%s<p>\"<z>\r\n", target->base.name, args);
    descriptor_send(d, out);
    tell_history_add(d->character->player_id, target->player_id, args);
    if (target->desc && !ignore_repo_is_ignored(target->player_id, d->character->base.name)) {
        snprintf(out, sizeof(out), "<p>%s tells you, \"<z>%s<p>\"<z>\r\n",
                 d->character->base.name, args);
        descriptor_notify_comm(target->desc, out);
        snprintf(target->desc->last_teller, sizeof(target->desc->last_teller), "%s",
                 d->character->base.name);
    }
    return true;
}
