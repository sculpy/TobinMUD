/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "zone.h"

/* `zonefile create <zone>` (user 2026-07-11: "zonefile create should
 * create a zone file with the current status of the zone and its
 * contents... you should also be able to delete a line from the zone
 * file, rerun zonefile create and it fills in the blanks"). Snapshots the
 * zone's current live mobs/objects (see zone_file_create(), zone.c/zone.h
 * for the full design and its documented limitations) into new
 * zone_reset rows. Gated the same way as `zone`/`edzone`: BUILD_MIN_LEVEL
 * to be callable at all, then zone_can_edit() for the actual per-zone
 * builder-assignment boundary. */
bool cmd_zonefile(descriptor_t *d, const char *args) {
    if (!d->character)
        return true;

    char sub[16] = "";
    int consumed = 0;
    sscanf(args, "%15s %n", sub, &consumed);
    const char *rest = args + consumed;
    size_t sublen = strlen(sub);

    if (sublen == 0 || strncasecmp("create", sub, sublen) != 0) {
        descriptor_send(d, "Usage: zonefile create <zone number>\r\n");
        return true;
    }

    int zone_nr;
    if (sscanf(rest, "%d", &zone_nr) != 1) {
        descriptor_send(d, "Usage: zonefile create <zone number>\r\n");
        return true;
    }

    if (!zone_can_edit(d->character, zone_nr)) {
        descriptor_send(d, "You aren't assigned to that zone.\r\n");
        return true;
    }

    int mobs = 0, objs = 0;
    zone_file_create(zone_nr, &mobs, &objs);

    char msg[192];
    snprintf(msg, sizeof(msg),
             "Zonefile created for zone %d: %d new mob load%s, %d new object load%s added.\r\n",
             zone_nr, mobs, mobs == 1 ? "" : "s", objs, objs == 1 ? "" : "s");
    descriptor_send(d, msg);
    return true;
}
