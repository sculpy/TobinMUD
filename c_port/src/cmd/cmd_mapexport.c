/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "being.h"
#include "log.h"
#include "room.h"
#include "world_map_repo.h"

#define MAP_EXPORT_DIR "map_exports"

/* `mapexport [filename]` (Administrator, 59+): dumps the ENTIRE world --
 * every room in the `room` table and every exit in `roomexit`, straight
 * from the database, not just what's been visited/loaded into the
 * in-memory world cache this boot (world_map_repo_load_all()) -- to a
 * plain text file in the TobinMUD Client's own map.dat format
 * (client/src/core/map_model.c): one line per room,
 * "VNUM<TAB>NAME<TAB>e0,e1,...,e9" (destination vnum per direction, in
 * the fixed north/east/south/west/up/down/northeast/northwest/southeast/
 * southwest order that room.c's DIR_NAMES already uses, -1 = no exit).
 *
 * Unlike the GMCP Room.Info push a player's own client learns from as
 * they walk (gmcp.c), secret exits ARE included here on purpose -- this
 * is meant to be a complete admin reference map, not a player-eye view.
 * TODO.md's mapping-support item. */
bool cmd_mapexport(descriptor_t *d, const char *args) {
    char filename[64] = "world_map.dat";
    char given[64];
    if (sscanf(args, "%63s", given) == 1) {
        if (strchr(given, '/') || strchr(given, '\\') || given[0] == '.') {
            descriptor_send(d, "Filename can't contain a path separator or start with a dot.\r\n");
            return true;
        }
        snprintf(filename, sizeof(filename), "%s", given);
    }
    mkdir(MAP_EXPORT_DIR, 0755); /* EEXIST is fine -- mkdir()'s return isn't checked for that reason */
    char path[128];
    snprintf(path, sizeof(path), "%s/%s", MAP_EXPORT_DIR, filename);

    int count = 0;
    world_map_room_t *rooms = world_map_repo_load_all(&count);
    if (!rooms) {
        descriptor_send(d, "Failed to load the world from the database.\r\n");
        return true;
    }
    FILE *f = fopen(path, "w");
    if (!f) {
        free(rooms);
        descriptor_send(d, "Failed to open the export file for writing.\r\n");
        return true;
    }
    for (int i = 0; i < count; i++) {
        /* Room names are short DB-sourced text that should never
         * legitimately contain a tab or newline; stripped defensively
         * anyway so a malformed name can never split or corrupt a
         * written line (same defense the client's own map_model.c
         * save path uses). */
        char safe_name[128];
        size_t o = 0;
        for (const char *p = rooms[i].name; *p && o + 1 < sizeof(safe_name); p++)
            safe_name[o++] = (*p == '\t' || *p == '\r' || *p == '\n') ? ' ' : *p;
        safe_name[o] = '\0';
        fprintf(f, "%d\t%s\t", rooms[i].vnum, safe_name);
        for (int dir = 0; dir < ROOM_NUM_EXITS; dir++)
            fprintf(f, "%s%d", dir ? "," : "", rooms[i].exits[dir]);
        fputc('\n', f);
    }
    fclose(f);
    free(rooms);

    char msg[192];
    snprintf(msg, sizeof(msg), "Exported %d rooms to %s.\r\n", count, path);
    descriptor_send(d, msg);
    game_log(LOG_EDIT, "%s exported the whole world map (%d rooms) to %s. [%s]",
             d->character->base.name, count, path, descriptor_display_host(d));
    return true;
}
