/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include "zone.h"

/* `edzone <zone number>` (Session 43, user: "make an edzone command to
 * have a menu driven editor function like edroom etc") -- menu-driven
 * zone editor: name/enabled/lifespan/vnum range, assign/unassign
 * builders, force a reset now. The whole editor lives in descriptor.c's
 * CONN_EDZONE_* state machine (see descriptor_edzone_begin), mirroring
 * edroom/edplayer. Gate: BUILD_MIN_LEVEL (51+) at the table, PLUS
 * zone_can_edit() here -- a 51-54 builder can only edzone a zone they're
 * assigned to (same rule edroom enforces on rooms); 55+ edits any zone. */
bool cmd_edzone(descriptor_t *d, const char *args) {
    if (!d->character) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }
    if (!*args || !isdigit((unsigned char)args[0])) {
        descriptor_send(d, "Usage: edzone <zone number>\r\n");
        return true;
    }
    int zone_nr = atoi(args);

    if (!zone_can_edit(d->character, zone_nr)) {
        descriptor_send(d, "You aren't assigned to that zone.\r\n");
        return true;
    }

    if (!descriptor_edzone_begin(d, zone_nr)) {
        char msg[64];
        snprintf(msg, sizeof(msg), "There is no zone %d to edit.\r\n", zone_nr);
        descriptor_send(d, msg);
    }
    return true;
}
