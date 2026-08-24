/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "log.h"
#include "player_repo.h"

/* `poofin`/`poofout` (user, 2026-07-11: "immorts should be able to set
 * their own enter or leave messages. Like Jesus drags his cross in from
 * the east. of course gender specific in the messaging" -- named
 * "poofin"/"poofout" originally, briefly renamed to "bamfin"/"bamfout"
 * the same session, then renamed BACK here once "bamfin"/"bamfout" was
 * freed up for `goto`'s teleport messages instead, user: "bamfin|out
 * should modify goto messaging and the current bamfin|out should be
 * called something else following the in|out syntax" -- see cmd_bamf.c).
 * Immortal-only, mirrors `title`'s set/clear/persist shape (cmd_title.c).
 * The stored message is a fragment completing "<Name> ___" -- do_move
 * (cmd_move.c) substitutes `$d` with the direction word and `$p` with the
 * mover's gender_possess() pronoun before showing it, e.g. "drags $p
 * cross in from the $d" becomes "Jesus drags his cross in from the
 * east." for a male Jesus moving east. Empty/`none`/`clear` reverts to
 * the default "exits to the <dir>"/"has arrived" wording. */
static bool set_poof(descriptor_t *d, const char *args, bool is_in) {
    being_t *ch = d->character;
    if (!ch)
        return true;

    while (*args && isspace((unsigned char)*args))
        args++;

    char *dest = is_in ? ch->poofin : ch->poofout;
    size_t destsz = is_in ? sizeof(ch->poofin) : sizeof(ch->poofout);
    const char *label = is_in ? "Poofin" : "Poofout";

    if (!*args || strcasecmp(args, "none") == 0 || strcasecmp(args, "clear") == 0) {
        dest[0] = '\0';
        if (is_in)
            player_set_poofin(ch->base.name, ch->account_id, NULL);
        else
            player_set_poofout(ch->base.name, ch->account_id, NULL);
        game_log(LOG_SILENT, "%s clears their %s", ch->base.name, label);
        char msg[64];
        snprintf(msg, sizeof(msg), "%s cleared.\r\n", label);
        descriptor_send(d, msg);
        return true;
    }

    snprintf(dest, destsz, "%s", args);
    if (is_in)
        player_set_poofin(ch->base.name, ch->account_id, dest);
    else
        player_set_poofout(ch->base.name, ch->account_id, dest);
    game_log(LOG_SILENT, "%s sets their %s to '%s'", ch->base.name, label, dest);

    char msg[BEING_BAMF_LEN + 32];
    snprintf(msg, sizeof(msg), "%s set to: %s\r\n", label, dest);
    descriptor_send(d, msg);
    return true;
}

bool cmd_poofin(descriptor_t *d, const char *args) { return set_poof(d, args, true); }
bool cmd_poofout(descriptor_t *d, const char *args) { return set_poof(d, args, false); }
