/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "being.h"
#include "suit_repo.h"

/* `edit suit [name]` -- menu-driven loadsuit editor (TODO.md priority
 * item, 2026-08-02: "Menu-driven loadsuit editor, configurable
 * per-wear-location quantities"). Builds on the existing `suit`/
 * `suit_item` tables + cmd_loadsuit.c (2026-07-26), which could only be
 * populated by hand-written SQL before this -- now a senior immortal can
 * create a suit, add/remove items, set its class restriction and
 * description, and (the actual per-wear-location-quantity feature) give
 * any item a quantity greater than 1, all from inside the game.
 *
 * With no argument, lists every suit (id/name/class/item count) --
 * read-only, a quick overview before picking one to open. With a name,
 * opens the menu editor (descriptor.c's CONN_EDSUIT_* state machine) on
 * the first suit whose name contains it (same substring match
 * suit_repo_find_by_name()/`loadsuit` already use); if nothing matches,
 * auto-creates a brand-new empty suit under that exact name and opens
 * the editor on it -- same "auto-create if missing" precedent oedit/
 * medit already established for object/mob prototypes. */
bool cmd_edsuit(descriptor_t *d, const char *args) {
    while (*args == ' ')
        args++;

    if (!*args) {
        suit_summary_t suits[32];
        int n = suit_repo_list_all(suits, 32);
        if (n == 0) {
            descriptor_send(d, "No suits defined yet. Usage: edit suit <name> (creates it if new).\r\n");
            return true;
        }
        char out[2048];
        size_t len = (size_t)snprintf(out, sizeof(out), "\r\n<c>=== Suits ===<z>\r\n");
        for (int i = 0; i < n && len < sizeof(out); i++) {
            len += (size_t)snprintf(out + len, sizeof(out) - len,
                "  <c>%3d)<z> <p>%-20s<z> class=%-9s items=%d -- %s\r\n",
                suits[i].id, suits[i].name,
                suits[i].class_restrict < 0 ? "any" : class_name((player_class_t)suits[i].class_restrict),
                suits[i].item_count, suits[i].description);
        }
        if (len < sizeof(out))
            snprintf(out + len, sizeof(out) - len, "\r\nUsage: edit suit <name>\r\n");
        descriptor_page_start(d, out, 0);
        return true;
    }

    char name[64];
    snprintf(name, sizeof(name), "%.63s", args);

    int suit_id = suit_repo_find_by_name(name, NULL, NULL, 0);
    if (suit_id < 0) {
        suit_id = suit_repo_create(name);
        if (suit_id < 0) {
            descriptor_send(d, "Couldn't create that suit -- the name may already be taken.\r\n");
            return true;
        }
        char msg[96];
        snprintf(msg, sizeof(msg), "No suit matched \"%s\" -- created a new empty one.\r\n", name);
        descriptor_send(d, msg);
    }

    descriptor_edsuit_begin(d, suit_id);
    return true;
}
