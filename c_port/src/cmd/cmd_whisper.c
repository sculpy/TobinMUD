/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "being.h"
#include "ignore_repo.h"
#include "room.h"
#include "thing.h"

/* `whisper <name> <message>` (Sneezy port, user 2026-07-12). Per
 * Sneezy's help text: "transmit a message to the person referenced
 * and in addition lets everyone in the room know that a conversation
 * is going on, but not what was said" -- unlike `tell` (anywhere in
 * the game), whisper only reaches someone in the SAME room. */
bool cmd_whisper(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    char tok[64] = "";
    int consumed = 0;
    if (sscanf(args, "%63s %n", tok, &consumed) < 1 || !tok[0]) {
        descriptor_send(d, "Whisper to whom, what?\r\n");
        return true;
    }
    const char *msg_text = args + consumed;
    while (*msg_text == ' ')
        msg_text++;
    if (!*msg_text) {
        descriptor_send(d, "Whisper to them what?\r\n");
        return true;
    }

    size_t len = strlen(tok);
    being_t *target = NULL;
    for (thing_t *t = ch->base.roomp->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_PC || t == &ch->base)
            continue;
        if (strncasecmp(t->name, tok, len) == 0) {
            target = (being_t *)t;
            break;
        }
    }
    if (!target) {
        char out[128];
        snprintf(out, sizeof(out), "You don't see '%s' here to whisper to.\r\n", tok);
        descriptor_send(d, out);
        return true;
    }

    char out[400];
    snprintf(out, sizeof(out), "<p>You whisper to %s, \"<z>%s<p>\"<z>\r\n",
             target->base.name, msg_text);
    descriptor_send(d, out);
    /* Ignore lists (Sneezy → Tobin feature audit): fails SILENTLY -- ch
     * already saw "You whisper to ..." above. Bystanders still see the
     * "whispers something to" line below either way (they're not the
     * one being blocked). */
    if (target->desc && !ignore_repo_is_ignored(target->player_id, ch->base.name)) {
        snprintf(out, sizeof(out), "<p>%s whispers to you, \"<z>%s<p>\"<z>\r\n",
                 ch->base.name, msg_text);
        descriptor_notify(target->desc, out);
    }

    /* Bystanders in the room see that a conversation happened, not what
     * was said. */
    for (thing_t *t = ch->base.roomp->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_PC || t == &ch->base || t == &target->base)
            continue;
        being_t *other = (being_t *)t;
        if (!other->desc)
            continue;
        snprintf(out, sizeof(out), "%s whispers something to %s.\r\n",
                 ch->base.name, target->base.name);
        descriptor_notify(other->desc, out);
    }

    return true;
}
