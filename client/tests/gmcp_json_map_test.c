/* Real (not transcribed) proof harness for the client mapping-support
 * pipeline: gmcp_json.c's object extraction/iteration plus
 * map_model.c's upsert/save/load -- both fully portable (no Win32
 * calls), so this links the actual production .c files directly,
 * unlike trigger_prompt_test.c's transcription (main.c's trigger logic
 * has Win32 deps this pipeline doesn't). Proves: a Room.Info-shaped
 * payload's exits object round-trips through gmcp_json's flat-object
 * iterator into map_model_upsert()'s exits[] array correctly, a later
 * sighting overwrites an earlier one, an unknown direction name is
 * ignored rather than corrupting an array slot, x/y/z round-trip when
 * present (TODO.md real-GDI map view) and a room with no known
 * position stays has_pos=false through a save/load round trip, and
 * the whole table survives that round trip byte-for-byte in content.
 * map_model_t is malloc'd here, not a stack local -- MAP_ROOM_MAX=24000
 * rooms makes it ~4.4MB apiece, and this test needs two of them
 * (m + loaded), close enough to a default 8MB stack limit that adding
 * x/y/z/has_pos to map_room_t was enough to tip two stack locals over
 * it and segfault before main() even got to its first statement. */
#include "../include/gmcp_json.h"
#include "../include/map_model.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

static int checks = 0, failures = 0;
static void check(bool cond, const char *what) {
    checks++;
    if (!cond) {
        failures++;
        printf("FAIL: %s\n", what);
    } else {
        printf("OK: %s\n", what);
    }
}

int main(void) {
    /* --- Part 1: object extraction + iteration, straight off a real
     * Room.Info wire payload shape (gmcp.c's own doc comment). --- */
    const char *json = "Room.Info {\"num\":942900,\"name\":\"A bare sandbox room.\","
                        "\"x\":3,\"y\":-1,\"z\":0,"
                        "\"exits\":{\"north\":942901,\"east\":942902,\"up\":942910}}";
    int gx = 0, gy = 0, gz = 0;
    check(gmcp_json_get_int(json, "x", &gx) && gmcp_json_get_int(json, "y", &gy)
              && gmcp_json_get_int(json, "z", &gz) && gx == 3 && gy == -1 && gz == 0,
          "gmcp_json_get_int extracts x/y/z from a Room.Info-shaped payload");
    char obj[256];
    check(gmcp_json_get_object(json, "exits", obj, sizeof(obj)),
          "gmcp_json_get_object extracts the exits object");
    check(strcmp(obj, "\"north\":942901,\"east\":942902,\"up\":942910") == 0,
          "extracted exits object has the expected raw contents");
    const char *cur = obj;
    char dir[16];
    int val;
    int seen = 0;
    while (gmcp_json_object_iter_next(&cur, dir, sizeof(dir), &val))
        seen++;
    check(seen == 3, "iterator walks all three exit pairs");

    /* Empty exits object (a room with no visible exits at all). */
    const char *json_empty = "Room.Info {\"num\":5,\"name\":\"A pit.\",\"exits\":{}}";
    char obj2[64];
    check(gmcp_json_get_object(json_empty, "exits", obj2, sizeof(obj2)) && obj2[0] == '\0',
          "an empty exits object extracts as an empty string, not a failure");
    const char *cur2 = obj2;
    check(!gmcp_json_object_iter_next(&cur2, dir, sizeof(dir), &val),
          "iterating an empty exits object yields nothing");

    /* --- Part 2: map_dir_index --- */
    check(map_dir_index("north") == 0 && map_dir_index("southwest") == 9,
          "map_dir_index resolves known direction names to the server's fixed order");
    check(map_dir_index("nowhere") == -1, "map_dir_index rejects an unknown direction name");

    /* --- Part 3: full parse-into-map pipeline + later-sighting overwrite. --- */
    map_model_t *m = malloc(sizeof(map_model_t));
    if (!m) { printf("out of memory\n"); return 1; }
    map_model_init(m);
    int exits[MAP_NUM_EXITS];
    for (int i = 0; i < MAP_NUM_EXITS; i++)
        exits[i] = -1;
    const char *c3 = obj;
    while (gmcp_json_object_iter_next(&c3, dir, sizeof(dir), &val)) {
        int di = map_dir_index(dir);
        if (di >= 0)
            exits[di] = val;
    }
    check(map_model_upsert(m, 942900, "A bare sandbox room.", exits, 3, -1, 0, true),
          "map_model_upsert accepts a new room");
    map_room_t *r = map_model_find(m, 942900);
    check(r && r->exits[0] == 942901 && r->exits[1] == 942902 && r->exits[4] == 942910,
          "upserted room carries the parsed exits in the right slots (north/east/up)");
    check(r && r->exits[2] == -1, "an unset direction (south) stays -1");
    check(r && r->has_pos && r->x == 3 && r->y == -1 && r->z == 0,
          "upserted room carries its x/y/z and has_pos");

    /* A door destroyed: same vnum re-sighted with north now gone. */
    int exits2[MAP_NUM_EXITS];
    memcpy(exits2, exits, sizeof(exits2));
    exits2[0] = -1;
    map_model_upsert(m, 942900, "A bare sandbox room.", exits2, 3, -1, 0, true);
    r = map_model_find(m, 942900);
    check(r && r->exits[0] == -1, "a later sighting overwrites a since-closed exit");
    check(m->count == 1, "re-sighting the same vnum does not grow the room count");

    /* No known position yet -- e.g. a room mapexported before maprecalc
       ever ran, or an old map.dat entry from before this field existed. */
    map_model_upsert(m, 942901, "A dusty hallway.", exits2, 0, 0, 0, false);
    check(m->count == 2, "a genuinely new vnum does grow the room count");
    map_room_t *r2 = map_model_find(m, 942901);
    check(r2 && !r2->has_pos, "a room upserted with has_pos=false stays has_pos=false");

    /* --- Part 4: save/load round trip. --- */
    const char *path = "/tmp/tobin_client_map_test.dat";
    check(map_model_save(m, path), "map_model_save writes the file");
    map_model_t *loaded = malloc(sizeof(map_model_t));
    if (!loaded) { printf("out of memory\n"); free(m); return 1; }
    map_model_init(loaded);
    map_model_load(loaded, path);
    check(loaded->count == m->count, "load recovers the same room count");
    map_room_t *lr = map_model_find(loaded, 942901);
    check(lr && strcmp(lr->name, "A dusty hallway.") == 0, "load recovers a room's name");
    check(lr && lr->exits[1] == 942902 && lr->exits[0] == -1,
          "load recovers a room's exits array intact");
    check(lr && !lr->has_pos, "load recovers has_pos=false for a room with no known position");
    map_room_t *lr0 = map_model_find(loaded, 942900);
    check(lr0 && lr0->has_pos && lr0->x == 3 && lr0->y == -1 && lr0->z == 0,
          "load recovers a real x/y/z round-tripped through save");
    remove(path);
    free(m);
    free(loaded);

    printf("\n%d/%d checks passed.\n", checks - failures, checks);
    return failures ? 1 : 0;
}
