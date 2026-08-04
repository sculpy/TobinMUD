/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "being.h"
#include "log.h"
#include "mob_repo.h"
#include "obj.h"
#include "obj_repo.h"
#include "room_repo.h"
#include "thing.h"
#include "trigger_repo.h"
#include "world.h"
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

    /* `zone reclaim <low>-<high>` (user, 2026-08-04: asked for a one-shot
     * way to wipe an entire ad hoc vnum range out of the world -- rooms,
     * objects, mobs, and their satellite rows (roomexit/roomextra,
     * objaffect/objextra/obj_magic, mob_extra/mob_imm/mobresponses),
     * plus any zone_reset/trigger rows referencing that range -- rather
     * than `purge <range>`'s scope (clears loose in-memory mobs/objects
     * only, never touches the DB prototype/room rows themselves, see
     * cmd_purge.c). 59+, same tier as `purge <range>` -- this is
     * STRICTLY more destructive (permanently deletes prototype/room
     * definitions, not just live instances), so it stays gated at least
     * as high. Refuses outright if any connected player is currently
     * standing in a room within the range -- deleting a room out from
     * under a live character would leave their `roomp` pointer dangling.
     * DB-only: any of these rooms/mobs/objects still loaded in server
     * memory at the moment of the call stay loaded (and keep saving/
     * dumping across copyover) until the next full copyover actually
     * drops them from the in-memory world -- same "DB row deletion isn't
     * a live unload" limitation every repo_delete_range() function above
     * documents. Purges each in-range room's loose contents first (same
     * logic `purge <range>` uses) so nothing is left dangling in memory
     * with a vanished prototype row in between. Scope is deliberately
     * NOT exhaustive -- shop/suit_item rows that happen to reference a
     * vnum in range are left alone, a disclosed simplification; nothing
     * in the ad hoc test-sandbox use case this exists for ever touches
     * those tables. */
    if (sublen > 0 && strncasecmp("reclaim", sub, sublen) == 0) {
        const char *dash = strchr(rest, '-');
        int low, high;
        if (!dash || sscanf(rest, "%d-%d", &low, &high) != 2) {
            descriptor_send(d, "Usage: zone reclaim <low vnum>-<high vnum>\r\n");
            return true;
        }
        if (d->character->progress.level < ZONE_RECLAIM_MIN_LEVEL) {
            descriptor_send(d, "Command not found, maybe submit an idea if you believe TobinMUD should have it.\r\n");
            return true;
        }
        if (low > high) {
            int tmp = low; low = high; high = tmp;
        }

        for (int v = low; v <= high; v++) {
            room_t *r = world_get_room(v);
            if (!r)
                continue;
            for (thing_t *t = r->base.stuff_head; t; t = t->stuff_next) {
                /* The caller's own presence doesn't block a reclaim --
                 * standing in the very sandbox room being cleaned up is
                 * the common case (an immortal reclaiming their own
                 * test room), and the DB-only deletion here doesn't
                 * invalidate their live roomp pointer the way it would
                 * for someone left behind after the room unloads at the
                 * next copyover. Only ANOTHER connected player counts. */
                if (t->kind == THING_PC && t != &d->character->base) {
                    descriptor_send(d, "A player is currently standing in a room within that range -- refused.\r\n");
                    return true;
                }
            }
        }

        int things_purged = 0;
        for (int v = low; v <= high; v++) {
            room_t *r = world_get_room(v);
            if (!r)
                continue;
            thing_t *t = r->base.stuff_head;
            while (t) {
                thing_t *next = t->stuff_next;
                if (t->kind == THING_OBJ) {
                    obj_destroy((obj_t *)t);
                    things_purged++;
                } else if (t->kind == THING_MOB) {
                    being_destroy((being_t *)t);
                    things_purged++;
                }
                t = next;
            }
        }

        int rooms_deleted = room_repo_delete_range(low, high);
        int objs_deleted = obj_repo_delete_range(low, high);
        int mobs_deleted = mob_repo_delete_range(low, high);
        int resets_deleted = zone_repo_delete_resets_referencing_range(low, high);
        int triggers_deleted = trigger_repo_delete_for_range(low, high);

        char msg[256];
        snprintf(msg, sizeof(msg),
                 "Reclaimed range %d-%d: %d room(s), %d obj(s), %d mob(s), %d zone-reset row(s), "
                 "%d trigger(s) deleted; %d loose thing(s) purged from live memory.\r\n",
                 low, high, rooms_deleted, objs_deleted, mobs_deleted, resets_deleted,
                 triggers_deleted, things_purged);
        descriptor_send(d, msg);
        game_log(LOG_EDIT, "%s reclaimed vnum range %d-%d (%d room, %d obj, %d mob, %d reset, %d trigger). [%s]",
                 d->character->base.name, low, high, rooms_deleted, objs_deleted, mobs_deleted,
                 resets_deleted, triggers_deleted, descriptor_display_host(d));
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
                        "       zone reclaim <low vnum>-<high vnum>  (59+, permanently deletes a vnum range)\r\n"
                        "       edzone <zone number>  (assign builders, edit properties)\r\n");
    return true;
}
