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

/* `tell <name> <message>` (Sneezy port, user 2026-07-12). Per Sneezy's
 * help text: "send a message strictly to the person referenced,
 * regardless of where they are in the mud" -- so this reaches anyone
 * connected and playing anywhere, unlike `say` (room-only) or
 * `whisper` (same-room-only, cmd_whisper.c). Same global-lookup-by-
 * name-prefix pattern as `transfer` (cmd_transfer.c). Not replicated:
 * the original's "can you actually see them" (blind/dark) check --
 * Tobin has no blindness/darkness system yet. Every tell is logged to
 * `tell_history` (2026-07-26 docs/systems review) and sets the
 * recipient's `last_teller` for `reply` (cmd_reply.c) -- both mirror the
 * original's tellhistory table + `desc->last_teller`. */
bool cmd_tell(descriptor_t *d, const char *args) {
    if (!d->character)
        return true;

    char tok[64] = "";
    int consumed = 0;
    if (sscanf(args, "%63s %n", tok, &consumed) < 1 || !tok[0]) {
        descriptor_send(d, "Tell whom what?\r\n");
        return true;
    }
    const char *msg_text = args + consumed;
    while (*msg_text == ' ')
        msg_text++;
    if (!*msg_text) {
        descriptor_send(d, "Tell them what?\r\n");
        return true;
    }

    size_t len = strlen(tok);
    being_t *target = NULL;
    for (descriptor_t *it = g_descriptors; it; it = it->next) {
        if (it->character && strncasecmp(it->character->base.name, tok, len) == 0) {
            target = it->character;
            break;
        }
    }
    if (!target) {
        char out[128];
        snprintf(out, sizeof(out), "No one named '%s' is in the game.\r\n", tok);
        descriptor_send(d, out);
        return true;
    }

    char out[400];
    snprintf(out, sizeof(out), "<p>You tell %s, \"<z>%s<p>\"<z>\r\n", target->base.name, msg_text);
    descriptor_send(d, out);
    tell_history_add(d->character->player_id, target->player_id, msg_text);
    /* Ignore lists (Sneezy → Tobin feature audit): fails SILENTLY -- the
     * sender already saw "You tell ..." above and never learns the target
     * blocked them, matching the original's own documented behavior. */
    if (target->desc && !ignore_repo_is_ignored(target->player_id, d->character->base.name)) {
        snprintf(out, sizeof(out), "<p>%s tells you, \"<z>%s<p>\"<z>\r\n",
                 d->character->base.name, msg_text);
        descriptor_notify_comm(target->desc, out);
        snprintf(target->desc->last_teller, sizeof(target->desc->last_teller), "%s",
                 d->character->base.name);
    }
    return true;
}
