/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "being.h"
#include "mob_repo.h"
#include "trophy.h"
#include "trophy_repo.h"

/* Case-insensitive "does haystack contain needle" (strcasestr is GNU-only,
 * same hand-rolled helper cmd_scan.c/cmd_who.c/etc. already duplicate). */
static bool ci_contains(const char *haystack, const char *needle) {
    size_t nl = strlen(needle);
    if (nl == 0)
        return true;
    for (; *haystack; haystack++)
        if (strncasecmp(haystack, needle, nl) == 0)
            return true;
    return false;
}

/* `trophy [<mob name>]`: lists the caller's own trophy-decayed XP
 * modifier against every mob they've killed, optionally filtered to
 * names containing the given substring. Ported from SneezyMUD's
 * TBeing::doTrophy() (cmd_trophy.cc) -- see trophy.h's own doc comment
 * for the disclosed scope-down (no zone-grouped kill-percentage
 * browsing, just the flat per-mob modifier listing). */
bool cmd_trophy(descriptor_t *d, const char *args) {
    if (!d->character)
        return true;
    if (d->character->base.kind != THING_PC) {
        descriptor_send(d, "Mobs can't use this command!\r\n");
        return true;
    }

    char filter[64] = "";
    if (args && *args) {
        strncpy(filter, args, sizeof(filter) - 1);
        filter[sizeof(filter) - 1] = '\0';
    }

    trophy_entry_t entries[512];
    int count = trophy_repo_list_for_player(d->character->player_id, entries, 512);

    char out[8192];
    size_t n = 0;
    n += (size_t)snprintf(out + n, sizeof(out) - n, "\r\n<c>-- Trophy --<z>\r\n");

    int shown = 0;
    for (int i = 0; i < count && n < sizeof(out); i++) {
        mob_proto_t proto;
        if (!mob_proto_load(entries[i].mob_vnum, &proto))
            continue;
        if (filter[0] && !ci_contains(proto.short_descr, filter) && !ci_contains(proto.name, filter))
            continue;

        double mod = trophy_exp_mod(entries[i].mob_vnum, entries[i].count);
        n += (size_t)snprintf(out + n, sizeof(out) - n,
                               "  You will gain %s experience when fighting %s.\r\n",
                               trophy_exp_mod_descr(mod), proto.short_descr);
        shown++;
    }

    if (shown == 0)
        n += (size_t)snprintf(out + n, sizeof(out) - n,
                               filter[0] ? "  (no matching kills on record)\r\n" : "  (no kills on record yet)\r\n");
    else
        n += (size_t)snprintf(out + n, sizeof(out) - n, "\r\nTotal mobs: %d\r\n", shown);
    (void)n;

    descriptor_page_start(d, out, 0);
    return true;
}
