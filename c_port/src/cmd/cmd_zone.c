/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "zone.h"
#include "zone_repo.h"

#define MAX_ZONE_LIST 512
#define MAX_OWNERS_PER_LINE 8

/* `zone reset <zone>` (a quick shortcut -- full editing, including builder
 * assignment, lives in `edzone`, cmd_edzone.c) / `zone list` (see who's
 * assigned where, user request Session 43: "dont forget a zone list so we
 * can see whats been assigned and to whom"). Subcommand-abbreviated like
 * `toggle`/`set`. */
bool cmd_zone(descriptor_t *d, const char *args) {
    if (!d->character)
        return true;

    char sub[16] = "";
    int consumed = 0;
    sscanf(args, "%15s %n", sub, &consumed);
    const char *rest = args + consumed;
    size_t sublen = strlen(sub);

    if (sublen > 0 && strncasecmp("reset", sub, sublen) == 0) {
        int zone_nr;
        if (sscanf(rest, "%d", &zone_nr) != 1) {
            descriptor_send(d, "Usage: zone reset <zone number>\r\n");
            return true;
        }
        int mobs = 0, objs = 0;
        zone_reset_now(zone_nr, &mobs, &objs);
        char msg[128];
        snprintf(msg, sizeof(msg), "Zone %d reset: %d mobs, %d objects loaded.\r\n",
                 zone_nr, mobs, objs);
        descriptor_send(d, msg);
        return true;
    }

    if (sublen > 0 && strncasecmp("list", sub, sublen) == 0) {
        static zone_t zones[MAX_ZONE_LIST];
        int n = zone_repo_load_all(zones, MAX_ZONE_LIST);

        char out[16000];
        int pos = snprintf(out, sizeof(out),
            "\r\n<c>=== Zones ===<z>\r\n%-6s %-30s %-4s %-4s  %s\r\n",
            "Zone", "Name", "On", "Life", "Builders");
        for (int i = 0; i < n && pos < (int)sizeof(out) - 200; i++) {
            char owners[MAX_OWNERS_PER_LINE][64];
            int oc = zone_repo_load_owner_names(zones[i].zone_nr, owners, MAX_OWNERS_PER_LINE);
            char ownerbuf[520] = "-";
            if (oc > 0) {
                int op = 0;
                for (int j = 0; j < oc; j++)
                    op += snprintf(ownerbuf + op, sizeof(ownerbuf) - (size_t)op,
                                    "%s%s", j ? ", " : "", owners[j]);
            }
            pos += snprintf(out + pos, sizeof(out) - (size_t)pos,
                "%-6d %-30.30s %-4s %-4d  %s\r\n",
                zones[i].zone_nr, zones[i].name,
                zones[i].enabled ? "yes" : "no", zones[i].lifespan, ownerbuf);
        }
        descriptor_page_start(d, out, 20);
        return true;
    }

    descriptor_send(d, "Usage: zone reset <zone number>\r\n"
                        "       zone list\r\n"
                        "       edzone <zone number>  (assign builders, edit properties)\r\n");
    return true;
}
