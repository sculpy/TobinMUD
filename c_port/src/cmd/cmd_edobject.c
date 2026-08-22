/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#include "log.h"
#include "obj_repo.h"

/* `edit object <vnum>` (TODO.md's "NEXT UP" item -- Sneezy -> Tobin
 * feature audit's builder-tools-OLC gap) -- menu-driven object-prototype
 * editor over the existing upstream-seeded `obj` table. The whole editor
 * lives in descriptor.c's CONN_OEDIT_* state machine (see
 * descriptor_oedit_begin), mirroring edroom/edzone/edplayer. EDIT-ONLY:
 * there is no vnum here to create a brand-new prototype with, same scope
 * boundary `edroom` draws (a separate `dig` command handles new-room
 * creation; there is no equivalent "make a new object vnum" command
 * either, so this can only open an already-existing row). Gate:
 * BUILD_MIN_LEVEL, enforced by `edit`'s own dispatcher (cmd_edit.c) --
 * no zone_can_edit() check here, unlike edroom/edzone: object prototypes
 * genuinely have no zone association in the schema (the `obj` table has
 * no `zone` column), so there's no ownership boundary to enforce. */
/* `edit object reclaim <low>-<high>` -- obj-only counterpart to
 * `edit room reclaim` above (see that function's doc comment); deletes
 * obj/objaffect/objextra/obj_magic rows in range via
 * obj_repo_delete_range(). No live in-memory purge here -- an obj_t
 * instance already carries its own loaded copy of the prototype's stats
 * (obj_proto_load() reads by value, not by live pointer), so deleting
 * the DB row doesn't corrupt anything already spawned; only a FUTURE
 * `load obj <vnum>` in this range would fail. */
static bool edobject_reclaim(descriptor_t *d, const char *rangearg) {
    if (d->character->progress.level < EDOBJECT_RECLAIM_MIN_LEVEL) {
        descriptor_send(d, "Command not found, maybe submit an idea if you believe TobinMUD should have it.\r\n");
        return true;
    }
    int low, high;
    if (sscanf(rangearg, "%d-%d", &low, &high) != 2) {
        descriptor_send(d, "Usage: edit object reclaim <low vnum>-<high vnum>\r\n");
        return true;
    }
    if (low > high) {
        int tmp = low; low = high; high = tmp;
    }
    int objs_deleted = obj_repo_delete_range(low, high);
    char msg[96];
    snprintf(msg, sizeof(msg), "Reclaimed range %d-%d: %d obj(s) deleted.\r\n", low, high, objs_deleted);
    descriptor_send(d, msg);
    game_log(LOG_EDIT, "%s reclaimed obj range %d-%d (%d obj(s)). [%s]",
             d->character->base.name, low, high, objs_deleted, descriptor_display_host(d));
    return true;
}

bool cmd_edobject(descriptor_t *d, const char *args) {
    if (!d->character) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }
    if (strncasecmp(args, "reclaim", 7) == 0 && (args[7] == ' ' || args[7] == '\0')) {
        const char *rest = args + 7;
        while (*rest == ' ')
            rest++;
        return edobject_reclaim(d, rest);
    }
    if (!*args || !isdigit((unsigned char)args[0])) {
        descriptor_send(d, "Usage: edit object <vnum>\r\n");
        return true;
    }
    int vnum = atoi(args);

    if (!descriptor_oedit_begin(d, vnum)) {
        char msg[64];
        snprintf(msg, sizeof(msg), "There is no object %d to edit.\r\n", vnum);
        descriptor_send(d, msg);
    }
    return true;
}
