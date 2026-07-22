/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

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
bool cmd_edobject(descriptor_t *d, const char *args) {
    if (!d->character) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
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
