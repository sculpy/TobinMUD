/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "room_repo.h"
#include "trigger_repo.h"
#include "zone.h"

/* `edit trigger <room|mob|obj> <vnum>` -- 2026-07-25 redesign (user: "edit
 * trigger <room|mob|obj> vnum should go into a menu driven editor where
 * you choose type with an option to delete the trigger inside the menu").
 * Replaces the old one-shot `edit trigger <target_type> <vnum>
 * <trigger_type> [match_text|chance]` command entirely -- every field
 * (which trigger_type to create, its match text/chance, its script, and
 * deleting it) is now reachable from inside the menu (descriptor.c's
 * CONN_TRIGEDIT_* state machine) instead of front-loaded as command-line
 * arguments. This function now only validates the target and opens that
 * menu; see trigger.h for the fixed action vocabulary and
 * trigger_script.h for the DG Scripts-style language.
 *
 * Two other read-only/quick sub-forms share this entry point:
 *   edit trigger list <vnum>    -- shows every trigger on ANY target type
 *                                   at this vnum (room AND mob AND obj --
 *                                   no longer needs the target type, since
 *                                   a builder often doesn't remember which
 *                                   table a vnum belongs to at a glance)
 *   edit trigger delete <id>    -- unchanged: a quick one-shot delete by
 *                                   id, without opening the menu at all
 */
bool cmd_edtrigger(descriptor_t *d, const char *args) {
    if (!d->character || !d->character->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    char usage[] =
        "Usage: edit trigger <room|mob|obj> <vnum>\r\n"
        "       edit trigger list <vnum>\r\n"
        "       edit trigger delete <id>\r\n";

    char a[32], b[32];
    int got = sscanf(args, "%31s %31s", a, b);

    if (got >= 2 && strcasecmp(a, "delete") == 0) {
        long id = atol(b);
        if (trigger_repo_delete(id))
            descriptor_send(d, "Trigger deleted.\r\n");
        else
            descriptor_send(d, "No trigger has that id.\r\n");
        return true;
    }

    if (got >= 2 && strcasecmp(a, "list") == 0) {
        if (!isdigit((unsigned char)b[0])) {
            descriptor_send(d, usage);
            return true;
        }
        int vnum = atoi(b);
        static const char *const TYPES[] = {"room", "mob", "obj"};
        trigger_t trigs[32];
        char out[4096];
        size_t len = 0;
        int total = 0;
        for (int t = 0; t < 3; t++) {
            int n = trigger_repo_list_for(TYPES[t], vnum, trigs, 32);
            total += n;
            for (int i = 0; i < n && len < sizeof(out); i++) {
                len += (size_t)snprintf(out + len, sizeof(out) - len,
                    "#%ld %s %d %s%s%s%s\r\n",
                    trigs[i].id, trigs[i].target_type, trigs[i].target_vnum, trigs[i].trigger_type,
                    trigs[i].match_text[0] ? " match=\"" : "",
                    trigs[i].match_text[0] ? trigs[i].match_text : "",
                    trigs[i].match_text[0] ? "\"" : "");
            }
        }
        if (total == 0) {
            descriptor_send(d, "No triggers on that vnum (checked room, mob, and obj).\r\n");
            return true;
        }
        descriptor_page_start(d, out, 0);
        return true;
    }

    if (got < 2) {
        descriptor_send(d, usage);
        return true;
    }

    if (strcasecmp(a, "room") != 0 && strcasecmp(a, "mob") != 0 && strcasecmp(a, "obj") != 0) {
        descriptor_send(d, usage);
        return true;
    }
    int vnum = atoi(b);
    if (!isdigit((unsigned char)b[0]) || vnum <= 0) {
        descriptor_send(d, usage);
        return true;
    }

    /* Builders (51-54) are confined to their assigned zone for a room
     * target, same rule `edit room` enforces -- mob/obj prototypes have no
     * zone_repo lookup wired yet (TODO.md), so that check is scoped to
     * rooms only for now. */
    if (strcasecmp(a, "room") == 0 && !zone_can_edit(d->character, room_repo_get_zone(vnum))) {
        descriptor_send(d, "You aren't assigned to that zone.\r\n");
        return true;
    }

    descriptor_trigedit_begin(d, a, vnum);
    return true;
}
