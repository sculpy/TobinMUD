/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#include "being.h"
#include "log.h"
#include "mob_repo.h"
#include "room.h"
#include "thing.h"
#include "world.h"

/* `edit mob <vnum>` -- the last builder-tools-OLC gap (TODO.md), menu-
 * driven mob-prototype editor over the existing upstream-seeded `mob`
 * table. The whole editor lives in descriptor.c's CONN_MEDIT_* state
 * machine (see descriptor_medit_begin), mirroring edroom/edzone/edobject.
 * EDIT-ONLY: there is no vnum here to create a brand-new prototype with,
 * same scope boundary edroom/edobject draw. Gate: BUILD_MIN_LEVEL,
 * enforced by `edit`'s own dispatcher (cmd_edit.c) -- no zone_can_edit()
 * check here, same reasoning as edobject: mob prototypes have no zone
 * association in the schema (the `mob` table has no `zone` column). */
/* `edit mob reclaim <low>-<high>` -- mob-only counterpart to
 * `edit room reclaim` above (see that function's doc comment); purges
 * loose loaded mob instances in range first (an already-spawned mob_t
 * has no live pointer back to its prototype row either, but leaving one
 * roaming a room whose prototype just vanished is needless clutter this
 * command exists specifically to avoid), then deletes
 * mob/mob_extra/mob_imm/mobresponses rows via mob_repo_delete_range(). */
static bool edmobile_reclaim(descriptor_t *d, const char *rangearg) {
    if (d->character->progress.level < EDMOBILE_RECLAIM_MIN_LEVEL) {
        descriptor_send(d, "Command not found, maybe submit an idea if you believe TobinMUD should have it.\r\n");
        return true;
    }
    int low, high;
    if (sscanf(rangearg, "%d-%d", &low, &high) != 2) {
        descriptor_send(d, "Usage: edit mob reclaim <low vnum>-<high vnum>\r\n");
        return true;
    }
    if (low > high) {
        int tmp = low; low = high; high = tmp;
    }

    int mobs_purged = 0;
    for (int v = low; v <= high; v++) {
        room_t *r = world_get_room(v);
        if (!r)
            continue;
        thing_t *t = r->base.stuff_head;
        while (t) {
            thing_t *next = t->stuff_next;
            if (t->kind == THING_MOB) {
                being_destroy((being_t *)t);
                mobs_purged++;
            }
            t = next;
        }
    }

    int mobs_deleted = mob_repo_delete_range(low, high);
    char msg[128];
    snprintf(msg, sizeof(msg), "Reclaimed range %d-%d: %d mob(s) deleted; %d loose mob(s) purged from live memory.\r\n",
             low, high, mobs_deleted, mobs_purged);
    descriptor_send(d, msg);
    game_log(LOG_EDIT, "%s reclaimed mob range %d-%d (%d mob(s)). [%s]",
             d->character->base.name, low, high, mobs_deleted, descriptor_display_host(d));
    return true;
}

bool cmd_edmobile(descriptor_t *d, const char *args) {
    if (!d->character) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }
    if (strncasecmp(args, "reclaim", 7) == 0 && (args[7] == ' ' || args[7] == '\0')) {
        const char *rest = args + 7;
        while (*rest == ' ')
            rest++;
        return edmobile_reclaim(d, rest);
    }
    if (!*args || !isdigit((unsigned char)args[0])) {
        descriptor_send(d, "Usage: edit mob <vnum>\r\n");
        return true;
    }
    int vnum = atoi(args);

    if (!descriptor_medit_begin(d, vnum)) {
        char msg[64];
        snprintf(msg, sizeof(msg), "There is no mob %d to edit.\r\n", vnum);
        descriptor_send(d, msg);
    }
    return true;
}
