/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "being.h"
#include "log.h"
#include "room.h"
#include "world_map_repo.h"

/* Direction coordinate deltas, matching room.c's own DIR_NAMES order:
 * north/east/south/west/up/down/northeast/northwest/southeast/southwest.
 * north is +y, east is +x, up is +z -- an arbitrary but internally
 * consistent convention; nothing else reads these values except a
 * future map-drawing client, so any consistent choice works. */
static const int DX[ROOM_NUM_EXITS] = { 0, 1, 0, -1, 0, 0, 1, -1, 1, -1 };
static const int DY[ROOM_NUM_EXITS] = { 1, 0, -1, 0, 0, 0, 1, 1, -1, -1 };
static const int DZ[ROOM_NUM_EXITS] = { 0, 0, 0, 0, 1, -1, 0, 0, 0, 0 };

/* Binary-search `rooms` (sorted by vnum, world_map_repo_load_all()'s own
 * contract) for `vnum`, returning its index or -1. */
static int find_room_index(const world_map_room_t *rooms, int count, int vnum) {
    int lo = 0, hi = count - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (rooms[mid].vnum == vnum)
            return mid;
        if (rooms[mid].vnum < vnum)
            lo = mid + 1;
        else
            hi = mid - 1;
    }
    return -1;
}

/* `maprecalc` (Implementor, 60+): derives x/y/z for EVERY room from the
 * roomexit graph and writes them into the `room` table's existing-but-
 * unused x/y/z columns (TODO.md's mapping-support item, paired with
 * `mapexport` above 59+) -- turns the graph-walked map the client
 * already builds into a real positioned one automatically, rerunnable
 * any time the world's layout actually changes (new zone, dug exit).
 *
 * A breadth-first flood-fill per connected component: the first room of
 * each component gets (componentIndex*100000, 0, 0) -- offset on x so
 * separate components don't visually overlap in a merged map; every
 * neighbor reached through a real exit gets that room's own coordinate
 * plus the exit direction's delta. FIRST visit wins -- a room reached
 * again later via a different path (a real cycle, a one-way/teleport
 * link, or a genuinely non-planar layout) keeps whatever coordinate it
 * got first rather than being overwritten, so the result is always
 * well-defined, just not necessarily geometrically "correct" for those
 * cases -- see TODO.md's own open-questions note on this; not solved
 * further here. */
bool cmd_maprecalc(descriptor_t *d, const char *args) {
    (void)args;
    int count = 0;
    world_map_room_t *rooms = world_map_repo_load_all(&count);
    if (!rooms) {
        descriptor_send(d, "Failed to load the world from the database.\r\n");
        return true;
    }
    bool *visited = (bool *)calloc((size_t)count, sizeof(bool));
    int *queue = (int *)malloc(sizeof(int) * (size_t)count);
    if (!visited || !queue) {
        free(rooms);
        free(visited);
        free(queue);
        descriptor_send(d, "Out of memory.\r\n");
        return true;
    }
    int components = 0;
    for (int start = 0; start < count; start++) {
        if (visited[start])
            continue;
        int qhead = 0, qtail = 0;
        rooms[start].x = components * 100000;
        rooms[start].y = 0;
        rooms[start].z = 0;
        visited[start] = true;
        queue[qtail++] = start;
        while (qhead < qtail) {
            int cur = queue[qhead++];
            for (int dir = 0; dir < ROOM_NUM_EXITS; dir++) {
                int dest_vnum = rooms[cur].exits[dir];
                if (dest_vnum < 0)
                    continue;
                int idx = find_room_index(rooms, count, dest_vnum);
                if (idx < 0 || visited[idx])
                    continue;
                rooms[idx].x = rooms[cur].x + DX[dir];
                rooms[idx].y = rooms[cur].y + DY[dir];
                rooms[idx].z = rooms[cur].z + DZ[dir];
                visited[idx] = true;
                queue[qtail++] = idx;
            }
        }
        components++;
    }
    free(visited);
    free(queue);

    bool ok = world_map_repo_save_coords(rooms, count);
    free(rooms);

    char msg[160];
    if (ok)
        snprintf(msg, sizeof(msg),
                 "Recalculated coordinates for %d rooms across %d connected component(s).\r\n",
                 count, components);
    else
        snprintf(msg, sizeof(msg), "Recalculated in memory but failed to save to the database.\r\n");
    descriptor_send(d, msg);
    game_log(LOG_EDIT, "%s recalculated world map coordinates (%d rooms, %d components). [%s]",
             d->character->base.name, count, components, descriptor_display_host(d));
    return true;
}
