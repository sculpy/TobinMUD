/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "being.h"
#include "suit_repo.h"

/* `edit suit [<id> | new <name>]` -- menu-driven loadsuit editor
 * (TODO.md priority item, 2026-08-02: "Menu-driven loadsuit editor,
 * configurable per-wear-location quantities"). Builds on the existing
 * `suit`/`suit_item` tables + cmd_loadsuit.c (2026-07-26), which could
 * only be populated by hand-written SQL before this -- now a senior
 * immortal can create a suit, add/remove items, set its class
 * restriction and description, delete it entirely, and (the actual
 * per-wear-location-quantity feature) give any item a quantity greater
 * than 1, all from inside the game.
 *
 * Number-driven (user, 2026-08-02: "loadsuit by name is quirky, lets be
 * number driven") -- an earlier version matched by name substring, the
 * same convention `loadsuit` used, but that made both commands fragile
 * against ambiguous/partial matches. A bare number now always means
 * exactly one suit, no guessing. `edit suit` with no argument still
 * lists every suit (id/name/class/item count/description) as a
 * read-only overview -- that's how a builder finds the id to edit or
 * delete in the first place. `edit suit new <name>` is the explicit
 * creation verb (replaces the old "unmatched name auto-creates"
 * behavior, which doesn't make sense once the primary argument is a
 * number instead of a name). */
bool cmd_edsuit(descriptor_t *d, const char *args) {
    while (*args == ' ')
        args++;

    if (!*args) {
        suit_summary_t suits[32];
        int n = suit_repo_list_all(suits, 32);
        if (n == 0) {
            descriptor_send(d, "No suits defined yet. Usage: edit suit new <name>\r\n");
            return true;
        }
        char out[2048];
        size_t len = (size_t)snprintf(out, sizeof(out), "\r\n<c>=== Suits ===<z>\r\n");
        for (int i = 0; i < n && len < sizeof(out); i++) {
            len += (size_t)snprintf(out + len, sizeof(out) - len,
                "  <c>%3d)<z> <p>%-20s<z> class=%-9s race=%-7s items=%d -- %s\r\n",
                suits[i].id, suits[i].name,
                suits[i].class_restrict < 0 ? "any" : class_name((player_class_t)suits[i].class_restrict),
                suits[i].race_restrict < 0 ? "any" : race_name((player_race_t)suits[i].race_restrict),
                suits[i].item_count, suits[i].description);
        }
        if (len < sizeof(out))
            snprintf(out + len, sizeof(out) - len,
                "\r\nUsage: edit suit <id>\r\n       edit suit new <name>\r\n");
        descriptor_page_start(d, out, 0);
        return true;
    }

    char tok[64], rest[64];
    int got = sscanf(args, "%63s %63s", tok, rest);

    if (strcasecmp(tok, "new") == 0) {
        if (got < 2) {
            descriptor_send(d, "Usage: edit suit new <name>\r\n");
            return true;
        }
        int suit_id = suit_repo_create(rest);
        if (suit_id < 0) {
            descriptor_send(d, "Couldn't create that suit -- the name may already be taken.\r\n");
            return true;
        }
        char msg[96];
        snprintf(msg, sizeof(msg), "Created a new empty suit: \"%s\".\r\n", rest);
        descriptor_send(d, msg);
        descriptor_edsuit_begin(d, suit_id);
        return true;
    }

    char *end;
    long suit_id = strtol(tok, &end, 10);
    if (end == tok || suit_id <= 0 || !suit_repo_get((int)suit_id, NULL, 0, NULL, NULL, NULL, 0)) {
        descriptor_send(d, "No such suit id -- `edit suit` with no argument lists them all.\r\n");
        return true;
    }

    descriptor_edsuit_begin(d, (int)suit_id);
    return true;
}
