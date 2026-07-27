/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>

#include "being.h"
#include "descriptor.h"

/* `shout <message>` (user, 2026-07-11: "add a shout channel, use sneezy
 * for implementation ideas"). Modeled on the original's sendShout()
 * (misc/talk.cc): unlike `say` (room-only), a shout reaches every
 * connected, playing character in the WORLD. Scoped down from the
 * original -- not replicated: garble/drunk distortion, the move-point
 * cost, charmed-mob restrictions, and the ignore-list check (Tobin has
 * no ignore-list feature yet). What IS kept: a sleeping listener never
 * hears it, and the `noshout` toggle (cmd_toggle.c) lets a player opt out
 * -- except an IMMORTAL's shout always gets through regardless of
 * noshout, matching the original's "if I'm mortal and the shouter is
 * immortal, hear it no matter what" rule. */
bool cmd_shout(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch)
        return true;
    if (!*args) {
        descriptor_send(d, "Yes, but WHAT do you want to shout?\r\n");
        return true;
    }

    char msg[336];
    snprintf(msg, sizeof(msg), "<g>You shout, \"<z>%s<g>\"<z>\r\n", args);
    descriptor_send(d, msg);

    bool shouter_immortal = being_is_immortal(ch);
    for (descriptor_t *it = g_descriptors; it; it = it->next) {
        if (!it->character || it->character == ch)
            continue;
        being_t *other = it->character;
        if (other->position <= POSITION_SLEEPING)
            continue;
        if ((other->pflags & PLR_NOSHOUT) && !shouter_immortal)
            continue;
        snprintf(msg, sizeof(msg), "<g>%s shouts, \"<z>%s<g>\"<z>\r\n",
                 ch->base.name, args);
        descriptor_notify_comm(it, msg);
    }
    return true;
}
