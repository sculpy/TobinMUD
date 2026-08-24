/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <string.h>

#include "being.h"
#include "ignore_repo.h"

/* `ignore [<name>]` / `unignore <name>` -- a per-character block list for
 * unwanted `tell`/`whisper` (Sneezy → Tobin feature audit, "Ignore
 * lists"). Scoped down from the original's ignoreList (also blockable by
 * descriptor or whole account, and reaching say/shout/grouptell/emote/
 * socials) to name-only blocking of the two direct-message channels
 * Tobin actually has -- see tobin_migrations.sql's own comment on
 * player_ignore. A blocked sender's tell/whisper still reports success
 * to THEM (matches the original: "Tell failures are silent; the sender
 * sees success even when ignored") -- see cmd_tell.c/cmd_whisper.c. */
bool cmd_ignore(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch)
        return true;

    char tok[64] = "";
    sscanf(args, "%63s", tok);

    if (!tok[0]) {
        int n = ignore_repo_count(ch->player_id);
        if (n == 0) {
            descriptor_send(d, "You aren't ignoring anyone.\r\n");
            return true;
        }
        char names[IGNORE_MAX_PER_PLAYER][IGNORE_NAME_LEN];
        n = ignore_repo_list(ch->player_id, names, IGNORE_MAX_PER_PLAYER);
        char out[1024];
        int len = snprintf(out, sizeof(out), "You are ignoring:\r\n");
        for (int i = 0; i < n; i++)
            len += snprintf(out + len, sizeof(out) - (size_t)len, "  %s\r\n", names[i]);
        descriptor_send(d, out);
        return true;
    }

    if (strcasecmp(tok, ch->base.name) == 0) {
        descriptor_send(d, "You can't ignore yourself.\r\n");
        return true;
    }

    if (ignore_repo_is_ignored(ch->player_id, tok)) {
        char out[96];
        snprintf(out, sizeof(out), "You are already ignoring %s.\r\n", tok);
        descriptor_send(d, out);
        return true;
    }

    if (!ignore_repo_add(ch->player_id, tok)) {
        char out[96];
        snprintf(out, sizeof(out), "You can only ignore up to %d people at once.\r\n", IGNORE_MAX_PER_PLAYER);
        descriptor_send(d, out);
        return true;
    }

    char out[96];
    snprintf(out, sizeof(out), "You are now ignoring %s.\r\n", tok);
    descriptor_send(d, out);
    return true;
}

/* `unignore <name>` -- removes one name from `ch`'s ignore list (see the
 * file's top comment for what ignore blocks). */
bool cmd_unignore(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch)
        return true;

    char tok[64] = "";
    if (sscanf(args, "%63s", tok) != 1) {
        descriptor_send(d, "Usage: unignore <name>\r\n");
        return true;
    }

    if (!ignore_repo_remove(ch->player_id, tok)) {
        char out[96];
        snprintf(out, sizeof(out), "You aren't ignoring %s.\r\n", tok);
        descriptor_send(d, out);
        return true;
    }

    char out[96];
    snprintf(out, sizeof(out), "You are no longer ignoring %s.\r\n", tok);
    descriptor_send(d, out);
    return true;
}
