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

/* `bamfin`/`bamfout` (user 2026-07-11: "bamfin|out should modify goto
 * messaging and the current bamfin|out should be called something else
 * following the in|out syntax" -- the WALKING move-message feature that
 * used to own this name moved to `poofin`/`poofout`, cmd_poof.c, its
 * ORIGINAL name). Immortal-only, same set/clear/persist shape as
 * `poofin`/`poofout`/`title`. The stored message is a fragment completing
 * "<Name> ___", shown to everyone else in the room `goto` departs from
 * (bamfout) or arrives in (bamfin) -- see cmd_goto.c's announce_bamf().
 * Supports three tokens (follow-up requests the same session: "<N> should
 * work in this as well as $g"; "and $p"): `<N>`/`<n>` (the mover's name,
 * may appear anywhere -- same convention as a player's `title`, cmd_who.c),
 * `$g`/`$$g` (the destination/departure room's ground-surface word,
 * obj_apply_ground_token()), and `$p` (gender_possess() pronoun). Empty/
 * `none`/`clear` reverts to the default "<Name> disappears/appears in a
 * puff of smoke." wording. */
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
