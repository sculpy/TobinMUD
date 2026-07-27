/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

/* `edit mob <vnum>` -- the last builder-tools-OLC gap (TODO.md), menu-
 * driven mob-prototype editor over the existing upstream-seeded `mob`
 * table. The whole editor lives in descriptor.c's CONN_MEDIT_* state
 * machine (see descriptor_medit_begin), mirroring edroom/edzone/edobject.
 * EDIT-ONLY: there is no vnum here to create a brand-new prototype with,
 * same scope boundary edroom/edobject draw. Gate: BUILD_MIN_LEVEL,
 * enforced by `edit`'s own dispatcher (cmd_edit.c) -- no zone_can_edit()
 * check here, same reasoning as edobject: mob prototypes have no zone
 * association in the schema (the `mob` table has no `zone` column). */
bool cmd_edmobile(descriptor_t *d, const char *args) {
    if (!d->character) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
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
