#include "room.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *const DIR_NAMES[ROOM_NUM_EXITS] = {
    "north", "east", "south", "west", "up", "down",
    "northeast", "northwest", "southeast", "southwest",
};

/* Original: constants.cc rev_dirs (all ten entries). */
const int REV_DIR[ROOM_NUM_EXITS] = { 2, 3, 0, 1, 5, 4, 9, 8, 7, 6 };

/* sectorTypeT names, misc/enum.h order (0-60). */
static const char *const SECTOR_NAMES[MAX_SECTOR_TYPES] = {
    "subarctic", "arctic waste", "arctic city", "arctic road", "tundra",
    "arctic mountains", "arctic forest", "arctic marsh",
    "arctic river surface", "iceflow", "cold beach", "solid ice",
    "arctic building", "arctic cave", "arctic atmosphere",
    "arctic climbing", "arctic forest road", "plains", "temperate city",
    "temperate road", "grasslands", "temperate hills",
    "temperate mountains", "temperate forest", "temperate swamp",
    "temperate ocean", "temperate river surface", "temperate underwater",
    "temperate beach", "temperate building", "temperate cave",
    "temperate atmosphere", "temperate climbing", "temperate forest road",
    "desert", "savannah", "veldt", "tropical city", "tropical road",
    "jungle", "rainforest", "tropical hills", "tropical mountains",
    "volcano lava", "tropical swamp", "tropical ocean",
    "tropical river surface", "tropical underwater", "tropical beach",
    "tropical building", "tropical cave", "tropical atmosphere",
    "tropical climbing", "rainforest road", "astral ethreal",
    "solid rock", "fire", "inside mob", "fire atmosphere", "make fly",
    "dead woods",
};

const char *sector_name(int sector) {
    if (sector < 0 || sector >= MAX_SECTOR_TYPES)
        return "unknown";
    return SECTOR_NAMES[sector];
}

/* ROOM_* flag bit names, misc/room.h order (bits 0-21). */
static const char *const ROOM_FLAG_NAMES[22] = {
    "always-lit", "death", "no-mob", "indoors", "peaceful", "no-steal",
    "no-escape", "no-magic", "no-portal", "private", "silence",
    "no-order", "no-flee", "have-to-walk", "arena", "no-heal",
    "hospital", "save-room", "no-autoformat", "being-editted",
    "on-fire", "flooded",
};

const char *room_flag_names(int flags, char *buf, size_t size) {
    size_t n = 0;
    buf[0] = '\0';
    for (int bit = 0; bit < 22; bit++) {
        if (!(flags & (1 << bit)))
            continue;
        n += (size_t)snprintf(buf + n, size > n ? size - n : 0, "%s%s",
                              n > 0 ? " " : "", ROOM_FLAG_NAMES[bit]);
        if (n >= size)
            break;
    }
    if (buf[0] == '\0')
        snprintf(buf, size, "none");
    return buf;
}

room_t *room_create(int vnum, const char *name, const char *description, int sector) {
    room_t *r = calloc(1, sizeof(*r));
    if (!r)
        return NULL;

    r->base.kind = THING_ROOM;
    r->base.id = vnum;
    snprintf(r->base.name, sizeof(r->base.name), "%s", name ? name : "");
    r->vnum = vnum;
    snprintf(r->description, sizeof(r->description), "%s", description ? description : "");
    r->sector = sector;

    for (int i = 0; i < ROOM_NUM_EXITS; i++)
        r->exits[i] = -1;

    return r;
}

void room_destroy(room_t *r) {
    free(r);
}
