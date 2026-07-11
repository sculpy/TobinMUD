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

/* `bamfin`/`bamfout` (user, 2026-07-11: "immorts should be able to set
 * their own enter or leave messages. Like Jesus drags his cross in from
 * the east. of course gender specific in the messaging" -- named
 * "poofin"/"poofout" originally, renamed per user request the same
 * session). Immortal-only, mirrors `title`'s set/clear/persist shape
 * (cmd_title.c). The stored message is a fragment completing "<Name> ___"
 * -- do_move (cmd_move.c) substitutes `$d` with the direction word and
 * `$p` with the mover's gender_possess() pronoun before showing it, e.g.
 * "drags $p cross in from the $d" becomes "Jesus drags his cross in from
 * the east." for a male Jesus moving east. Empty/`none`/`clear` reverts
 * to the default "exits to the <dir>"/"has arrived" wording. */
static bool set_bamf(descriptor_t *d, const char *args, bool is_in) {
    being_t *ch = d->character;
    if (!ch)
        return true;

    while (*args && isspace((unsigned char)*args))
        args++;

    char *dest = is_in ? ch->bamfin : ch->bamfout;
    size_t destsz = is_in ? sizeof(ch->bamfin) : sizeof(ch->bamfout);
    const char *label = is_in ? "Bamfin" : "Bamfout";

    if (!*args || strcasecmp(args, "none") == 0 || strcasecmp(args, "clear") == 0) {
        dest[0] = '\0';
        if (is_in)
            player_set_bamfin(ch->base.name, ch->account_id, NULL);
        else
            player_set_bamfout(ch->base.name, ch->account_id, NULL);
        char msg[64];
        snprintf(msg, sizeof(msg), "%s cleared.\r\n", label);
        descriptor_send(d, msg);
        return true;
    }

    snprintf(dest, destsz, "%s", args);
    if (is_in)
        player_set_bamfin(ch->base.name, ch->account_id, dest);
    else
        player_set_bamfout(ch->base.name, ch->account_id, dest);

    char msg[BEING_BAMF_LEN + 32];
    snprintf(msg, sizeof(msg), "%s set to: %s\r\n", label, dest);
    descriptor_send(d, msg);
    return true;
}

bool cmd_bamfin(descriptor_t *d, const char *args) { return set_bamf(d, args, true); }
bool cmd_bamfout(descriptor_t *d, const char *args) { return set_bamf(d, args, false); }
