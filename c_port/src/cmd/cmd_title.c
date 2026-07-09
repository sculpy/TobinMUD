/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "player_repo.h"

/* `title [text]`: sets the free-form descriptor shown after your name in
 * who (e.g. "Testguy floats here bleeding"). Mirrors the original's
 * free-text title. `title` with no argument, or `title none`/`title clear`,
 * removes it. Length-capped to the player.title column (BEING_TITLE_LEN).
 * Persists immediately so it survives a reconnect. */
bool cmd_title(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch)
        return true;

    /* Skip leading whitespace so `title    the Brave` stores "the Brave". */
    while (*args && isspace((unsigned char)*args))
        args++;

    if (!*args || strcasecmp(args, "none") == 0 || strcasecmp(args, "clear") == 0) {
        ch->title[0] = '\0';
        player_set_title(ch->base.name, ch->account_id, NULL);
        descriptor_send(d, "Title cleared.\r\n");
        return true;
    }

    snprintf(ch->title, sizeof(ch->title), "%s", args);
    player_set_title(ch->base.name, ch->account_id, ch->title);

    char msg[BEING_TITLE_LEN + 32];
    snprintf(msg, sizeof(msg), "Title set to: %s\r\n", ch->title);
    descriptor_send(d, msg);
    return true;
}
