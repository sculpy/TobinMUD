/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
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
/* Uppercased, from the enum names (sectorTypeT) -- displayed all-caps. */
static const char *const SECTOR_NAMES[MAX_SECTOR_TYPES] = {
    "SUBARCTIC", "ARCTIC WASTE", "ARCTIC CITY", "ARCTIC ROAD", "TUNDRA",
    "ARCTIC MOUNTAINS", "ARCTIC FOREST", "ARCTIC MARSH",
    "ARCTIC RIVER SURFACE", "ICEFLOW", "COLD BEACH", "SOLID ICE",
    "ARCTIC BUILDING", "ARCTIC CAVE", "ARCTIC ATMOSPHERE",
    "ARCTIC CLIMBING", "ARCTIC FOREST ROAD", "PLAINS", "TEMPERATE CITY",
    "TEMPERATE ROAD", "GRASSLANDS", "TEMPERATE HILLS",
    "TEMPERATE MOUNTAINS", "TEMPERATE FOREST", "TEMPERATE SWAMP",
    "TEMPERATE OCEAN", "TEMPERATE RIVER SURFACE", "TEMPERATE UNDERWATER",
    "TEMPERATE BEACH", "TEMPERATE BUILDING", "TEMPERATE CAVE",
    "TEMPERATE ATMOSPHERE", "TEMPERATE CLIMBING", "TEMPERATE FOREST ROAD",
    "DESERT", "SAVANNAH", "VELDT", "TROPICAL CITY", "TROPICAL ROAD",
    "JUNGLE", "RAINFOREST", "TROPICAL HILLS", "TROPICAL MOUNTAINS",
    "VOLCANO LAVA", "TROPICAL SWAMP", "TROPICAL OCEAN",
    "TROPICAL RIVER SURFACE", "TROPICAL UNDERWATER", "TROPICAL BEACH",
    "TROPICAL BUILDING", "TROPICAL CAVE", "TROPICAL ATMOSPHERE",
    "TROPICAL CLIMBING", "RAINFOREST ROAD", "ASTRAL ETHREAL",
    "SOLID ROCK", "FIRE", "INSIDE MOB", "FIRE ATMOSPHERE", "MAKE FLY",
    "DEAD WOODS",
};

const char *sector_name(int sector) {
    if (sector < 0 || sector >= MAX_SECTOR_TYPES)
        return "unknown";
    return SECTOR_NAMES[sector];
}

/* ROOM_* flag bit names (original room_bits[], misc/room.h, bits 0-21),
 * displayed all-caps straight from the upstream table. */
static const char *const ROOM_FLAG_NAMES[22] = {
    "ALWAYS-LIT", "DEATH", "NO-MOB", "INDOORS", "PEACEFUL", "NO-STEAL",
    "NO-ESCAPE", "NO-MAGIC", "NO-PORTAL", "PRIVATE", "SILENCE",
    "NO-ORDER", "NO-FLEE", "HAVE-TO-WALK", "ARENA", "NO-HEAL",
    "HOSPITAL", "SAVE ROOMS", "NO-AUTOFORMAT", "BEING EDITED",
    "ON-FIRE", "FLOODED",
};

const char *room_flag_names(int flags, char *buf, size_t size) {
    size_t n = 0;
    buf[0] = '\0';
    for (int bit = 0; bit < 22; bit++) {
        if (!(flags & (1 << bit)))
            continue;
        /* Each flag in its own bracket, e.g. "[ ALWAYS-LIT ] [ INDOORS ]"
         * (callers wrap the whole run in a color). */
        n += (size_t)snprintf(buf + n, size > n ? size - n : 0, "%s[ %s ]",
                              n > 0 ? " " : "", ROOM_FLAG_NAMES[bit]);
        if (n >= size)
            break;
    }
    if (buf[0] == '\0')
        snprintf(buf, size, "none");
    return buf;
}

int room_flag_count(void) {
    return 22;
}

const char *room_flag_name(int bit) {
    if (bit < 0 || bit >= 22)
        return "?";
    return ROOM_FLAG_NAMES[bit];
}

/* doorTypeT (misc/room.h), indices 0-10. */
static const char *const DOOR_TYPE_NAMES[MAX_DOOR_TYPES] = {
    "None", "Door", "Trapdoor", "Gate", "Grate", "Portcullis",
    "Drawbridge", "Rubble", "Panel", "Screen", "Hatch",
};

int door_type_count(void) {
    return MAX_DOOR_TYPES;
}

const char *door_type_name(int t) {
    if (t < 0 || t >= MAX_DOOR_TYPES)
        return "?";
    return DOOR_TYPE_NAMES[t];
}

/* exit_bits (misc/room.h, MAX_DOOR_CONDITIONS), condition bitmask names. */
static const char *const EXIT_COND_NAMES[MAX_EXIT_CONDITIONS] = {
    "Closed", "Locked", "Secret", "Destroyed", "No-enter", "Trapped",
    "Caved-In", "Magically-Warded", "Sloped-up", "Sloped-down", "Jammed",
};

int exit_cond_count(void) {
    return MAX_EXIT_CONDITIONS;
}

const char *exit_cond_name(int bit) {
    if (bit < 0 || bit >= MAX_EXIT_CONDITIONS)
        return "?";
    return EXIT_COND_NAMES[bit];
}

const char *exit_cond_names(int flags, char *buf, size_t size) {
    size_t n = 0;
    buf[0] = '\0';
    for (int bit = 0; bit < MAX_EXIT_CONDITIONS; bit++) {
        if (!(flags & (1 << bit)))
            continue;
        n += (size_t)snprintf(buf + n, size > n ? size - n : 0, "%s%s",
                              n > 0 ? " " : "", EXIT_COND_NAMES[bit]);
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

    for (int i = 0; i < ROOM_NUM_EXITS; i++) {
        r->exits[i] = -1;
        r->exit_door[i] = 0; /* DOOR_NONE */
        r->exit_cond[i] = 0;
    }

    return r;
}

void room_destroy(room_t *r) {
    free(r);
}
