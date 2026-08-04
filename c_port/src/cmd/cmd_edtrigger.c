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

#include "log.h"
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
 * One other read-only quick sub-form shares this entry point:
 *   edit trigger list <vnum>    -- shows every trigger on ANY target type
 *                                   at this vnum (room AND mob AND obj --
 *                                   no longer needs the target type, since
 *                                   a builder often doesn't remember which
 *                                   table a vnum belongs to at a glance)
 *
 * There is deliberately no `edit trigger delete <id>` quick form (removed
 * 2026-07-26, user: "forget the use of id, use only vnums") -- deletion
 * only happens from inside the menu (CONN_TRIGEDIT_ITEM's "D" option),
 * which a builder reaches by vnum and list position, never a raw db id.
 */
bool cmd_edtrigger(descriptor_t *d, const char *args) {
    if (!d->character || !d->character->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    char usage[] =
        "Usage: edit trigger <room|mob|obj> <vnum>\r\n"
        "       edit trigger list <vnum>\r\n"
        "       edit trigger reclaim <low vnum>-<high vnum>  (59+)\r\n";

    char a[32], b[32];
    int got = sscanf(args, "%31s %31s", a, b);

    /* `edit trigger reclaim <low>-<high>` -- trigger-only counterpart to
     * `edit room reclaim` (cmd_edroom.c, see its doc comment); no live
     * in-memory purge needed here (trigger_repo_load_for() always hits
     * the DB fresh when an event fires, nothing caches a trigger row in
     * memory), just deletes matching rows via
     * trigger_repo_delete_for_range(). */
    if (got >= 2 && strcasecmp(a, "reclaim") == 0) {
        if (d->character->progress.level < EDTRIGGER_RECLAIM_MIN_LEVEL) {
            descriptor_send(d, "Command not found, maybe submit an idea if you believe TobinMUD should have it.\r\n");
            return true;
        }
        int low, high;
        if (sscanf(b, "%d-%d", &low, &high) != 2) {
            descriptor_send(d, "Usage: edit trigger reclaim <low vnum>-<high vnum>\r\n");
            return true;
        }
        if (low > high) {
            int tmp = low; low = high; high = tmp;
        }
        int triggers_deleted = trigger_repo_delete_for_range(low, high);
        char msg[96];
        snprintf(msg, sizeof(msg), "Reclaimed range %d-%d: %d trigger(s) deleted.\r\n", low, high, triggers_deleted);
        descriptor_send(d, msg);
        game_log(LOG_EDIT, "%s reclaimed trigger range %d-%d (%d trigger(s)). [%s]",
                 d->character->base.name, low, high, triggers_deleted, descriptor_display_host(d));
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
                char suffix[TRIGGER_MATCH_LEN + 24] = "";
                if (trigs[i].match_text[0])
                    snprintf(suffix, sizeof(suffix), " match=\"%s\"", trigs[i].match_text);
                else if (strcasecmp(trigs[i].trigger_type, "random") == 0)
                    snprintf(suffix, sizeof(suffix), " chance=%d%%", trigs[i].chance_pct);
                len += (size_t)snprintf(out + len, sizeof(out) - len,
                    "%s %d %s%s\r\n",
                    trigs[i].target_type, trigs[i].target_vnum, trigs[i].trigger_type, suffix);
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
