/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#include "being.h"
#include "descriptor.h"
#include "ignore_repo.h"
#include "language.h"
#include "tell_history_repo.h"

/* Shared delivery logic for `tell` (below) and `reply` (cmd_reply.c) --
 * once the target being is resolved, everything else is identical:
 * history logging, the PLR_NOTELL block + its last_told exception, the
 * ignore-list silent block, delivery + last_teller bookkeeping, and the
 * PLR_AFK notice. Both callers have already confirmed the SENDER isn't
 * PLR_MUTED before reaching here (a mute blocks composing a tell at all,
 * independent of who it's addressed to). */
void tell_deliver(descriptor_t *d, being_t *target, const char *msg_text) {
    /* Language garble (Tier-4, 2026-08-16): the sender sees their own words
     * clear (tagged with the tongue if foreign); the recipient's copy is
     * garbled per their proficiency in it, below. The history log keeps the
     * clear text -- it's the sender's own record of what they meant. */
    int lang = d->character->spoken_language;
    char out[768];
    if (lang != LANG_COMMON)
        snprintf(out, sizeof(out), "<p>You tell %s (in %s), \"<z>%s<p>\"<z>\r\n",
                 target->base.name, language_name(lang), msg_text);
    else
        snprintf(out, sizeof(out), "<p>You tell %s, \"<z>%s<p>\"<z>\r\n", target->base.name, msg_text);
    descriptor_send(d, out);
    language_speaker_practice(d->character, lang);
    tell_history_add(d->character->player_id, target->player_id, msg_text);
    snprintf(d->last_told, sizeof(d->last_told), "%s", target->base.name);

    if (!target->desc)
        return;

    /* PLR_NOTELL (being.h): blocked unless the sender is who the TARGET
     * themselves last told -- an explicit failure, unlike the silent
     * ignore-list block below, since this is the target's own stated
     * preference rather than a hidden block list. */
    if ((target->pflags & PLR_NOTELL)
        && strcasecmp(target->desc->last_told, d->character->base.name) != 0) {
        snprintf(out, sizeof(out), "%s is not accepting tells right now.\r\n", target->base.name);
        descriptor_send(d, out);
        return;
    }

    /* Ignore lists (Sneezy → Tobin feature audit): fails SILENTLY -- the
     * sender already saw "You tell ..." above and never learns the target
     * blocked them, matching the original's own documented behavior. */
    if (ignore_repo_is_ignored(target->player_id, d->character->base.name))
        return;

    if (lang != LANG_COMMON) {
        char g[512];
        language_garble(lang, d->character, target, msg_text, g, sizeof(g));
        snprintf(out, sizeof(out), "<p>%s tells you (in %s), \"<z>%s<p>\"<z>\r\n",
                 d->character->base.name, language_name(lang), g);
    } else {
        snprintf(out, sizeof(out), "<p>%s tells you, \"<z>%s<p>\"<z>\r\n",
                 d->character->base.name, msg_text);
    }
    descriptor_notify_comm(target->desc, out);
    snprintf(target->desc->last_teller, sizeof(target->desc->last_teller), "%s",
             d->character->base.name);

    /* PLR_AFK (being.h): an extra notice for the sender once the target's
     * actually idle -- the tell above was still delivered either way. */
    if ((target->pflags & PLR_AFK)
        && (long)time(NULL) - target->desc->last_active > IDLE_DISPLAY_SECS) {
        snprintf(out, sizeof(out), "<k>(%s is AFK and may not see this right away.)<z>\r\n",
                 target->base.name);
        descriptor_send(d, out);
    }
}

/* `tell <name> <message>` (Sneezy port, user 2026-07-12). Per Sneezy's
 * help text: "send a message strictly to the person referenced,
 * regardless of where they are in the mud" -- so this reaches anyone
 * connected and playing anywhere, unlike `say` (room-only) or
 * `whisper` (same-room-only, cmd_whisper.c). Same global-lookup-by-
 * name-prefix pattern as `transfer` (cmd_transfer.c). Not replicated:
 * the original's "can you actually see them" (blind/dark) check --
 * Tobin has no blindness/darkness system yet. Delivery (history log,
 * PLR_NOTELL/ignore/PLR_AFK checks, last_teller/last_told bookkeeping)
 * is shared with `reply` via tell_deliver() above. */
bool cmd_tell(descriptor_t *d, const char *args) {
    if (!d->character)
        return true;
    if (d->character->pflags & PLR_MUTED) {
        descriptor_send(d, "You have been muted and cannot tell anyone.\r\n");
        return true;
    }

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

    tell_deliver(d, target, msg_text);
    return true;
}
