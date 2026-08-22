/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "room.h"
#include "weather.h"

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

/* Display name for a sectorTypeT value, e.g. "TEMPERATE FOREST"; "unknown"
 * for anything out of range. Backing table for sector_color(),
 * sector_move_cost(), room_can_plant(), and room_ground_type() below, which
 * all classify terrain by substring-matching this name rather than
 * switching on the raw enum. */
const char *sector_name(int sector) {
    if (sector < 0 || sector >= MAX_SECTOR_TYPES)
        return "unknown";
    return SECTOR_NAMES[sector];
}

/* Substring keyword match against the sector's name, most-specific rule
 * first (e.g. "ARCTIC CITY" is civilization-gray, not cold-cyan). See the
 * declaration comment in room.h for the category groupings. */
char sector_color(int sector) {
    const char *name = sector_name(sector);
    if (strstr(name, "LAVA") || strstr(name, "FIRE"))
        return 'r';
    if (strstr(name, "CITY") || strstr(name, "ROAD") || strstr(name, "BUILDING")
        || strstr(name, "MOUNTAIN") || strstr(name, "CLIMBING") || strstr(name, "CAVE")
        || strstr(name, "SOLID"))
        return 'w'; /* stone/structure -- no black/gray (<k>/<K>) in this map */
    if (strstr(name, "OCEAN") || strstr(name, "RIVER") || strstr(name, "UNDERWATER")
        || strstr(name, "BEACH") || strstr(name, "ICEFLOW"))
        return 'b';
    if (strstr(name, "ARCTIC") || strstr(name, "TUNDRA") || strstr(name, "ATMOSPHERE"))
        return 'c';
    if (strstr(name, "DESERT") || strstr(name, "SAVANNAH") || strstr(name, "VELDT"))
        return 'y';
    if (strstr(name, "SWAMP") || strstr(name, "MARSH"))
        return 'g';
    if (strstr(name, "JUNGLE") || strstr(name, "RAINFOREST") || strstr(name, "FOREST")
        || strstr(name, "GRASSLAND") || strstr(name, "PLAINS") || strstr(name, "HILLS"))
        return 'g';
    if (strstr(name, "ASTRAL"))
        return 'p';
    return 'w';
}

/* Substring keyword match against the sector's name, tiered 1 (cheapest)
 * to 6 (priciest), most-specific rule first. See the declaration comment
 * in room.h for the scope-cut rationale. */
int sector_move_cost(int sector) {
    const char *name = sector_name(sector);
    if (strstr(name, "LAVA") || strstr(name, "SOLID ROCK") || strstr(name, "FIRE"))
        return 6;
    if (strstr(name, "MOUNTAIN") || strstr(name, "CLIMBING") || strstr(name, "CAVE")
        || strstr(name, "WASTE") || strstr(name, "SOLID ICE"))
        return 5;
    if (strstr(name, "SWAMP") || strstr(name, "MARSH") || strstr(name, "OCEAN")
        || strstr(name, "RIVER") || strstr(name, "UNDERWATER"))
        return 4;
    if (strstr(name, "FOREST") || strstr(name, "HILLS") || strstr(name, "JUNGLE")
        || strstr(name, "RAINFOREST") || strstr(name, "DEAD WOODS"))
        return 3;
    if (strstr(name, "TUNDRA") || strstr(name, "SAVANNAH") || strstr(name, "VELDT")
        || strstr(name, "DESERT") || strstr(name, "BEACH") || strstr(name, "ICEFLOW"))
        return 2;
    /* CITY/ROAD/BUILDING/PLAINS/GRASSLAND, plus everything not covered
     * above (ATMOSPHERE, ASTRAL, INSIDE MOB, MAKE FLY -- none of them
     * physically-walked terrain today): cheapest tier. */
    return 1;
}

/* (c) per-sector-effects: how fast a sector parches you. Mirrors the
 * original's TerrainInfo thirst column (misc/constants.cc): desert 6,
 * savannah 5, veldt 4, tropical 3; everything temperate/arctic/wet sits
 * at the flat baseline 2 that most upstream rows carry. Underwater/river
 * surfaces run drier-benefit (1) upstream but Tobin keeps them at the 2
 * baseline -- its drowning path (vitals.c) already dominates there. */
int sector_thirst_rate(int sector) {
    const char *name = sector_name(sector);
    if (strstr(name, "DESERT"))
        return 6;
    if (strstr(name, "SAVANNAH"))
        return 5;
    if (strstr(name, "VELDT"))
        return 4;
    if (strstr(name, "LAVA") || strstr(name, "FIRE") || strstr(name, "JUNGLE")
        || strstr(name, "RAINFOREST") || strstr(name, "TROPICAL"))
        return 3;
    return 2;
}

/* (c) per-sector-effects: how fast a sector burns food. Mirrors the
 * original's TerrainInfo hunger column: mountains/climbing 4, and the
 * broad 3-tier of forest/hills/jungle/cave-climbing exertion terrain;
 * ordinary terrain sits at the flat baseline 2. Desert is also a 4-5
 * upstream (hunger 5) -- hard travel country burns both vitals. */
int sector_hunger_rate(int sector) {
    const char *name = sector_name(sector);
    if (strstr(name, "DESERT") || strstr(name, "MOUNTAIN") || strstr(name, "CLIMBING"))
        return 4;
    if (strstr(name, "FOREST") || strstr(name, "HILLS") || strstr(name, "JUNGLE")
        || strstr(name, "RAINFOREST") || strstr(name, "WASTE") || strstr(name, "DEAD WOODS"))
        return 3;
    return 2;
}

/* Ambient heat of a sector -- see room.h for why this Tobin-original layer
 * exists (upstream defines the data but never reads it). Buckets by name,
 * reusing the original TerrainInfo heat values for scale: lava/fire the
 * hottest (140), desert 120, tropics/jungle ~100, savannah/veldt 90;
 * deep-arctic wastes/high peaks/solid ice the coldest (-30); other arctic/
 * tundra/iceflow near freezing (0); everything temperate at a comfortable
 * baseline 60. The deep-cold test runs before the generic ARCTIC catch so
 * an arctic peak reads colder than an arctic road. Indoor shelter is the
 * caller's job (ROOM_FLAG_INDOORS), not encoded here. */
int sector_heat(int sector) {
    const char *name = sector_name(sector);
    if (strstr(name, "LAVA") || strstr(name, "FIRE"))
        return 140;
    if (strstr(name, "DESERT"))
        return 120;
    if (strstr(name, "JUNGLE") || strstr(name, "RAINFOREST") || strstr(name, "TROPICAL"))
        return 100;
    if (strstr(name, "SAVANNAH") || strstr(name, "VELDT"))
        return 90;
    if (strstr(name, "SUBARCTIC") || strstr(name, "WASTE") || strstr(name, "SOLID ICE")
        || (strstr(name, "ARCTIC") && strstr(name, "MOUNTAIN")))
        return -30;
    if (strstr(name, "ARCTIC") || strstr(name, "TUNDRA") || strstr(name, "ICEFLOW")
        || strstr(name, "COLD BEACH"))
        return 0;
    return 60;
}

/* Effective outdoor ambient heat: the sector's own heat, shifted by the
 * current world weather (rain/storms cool, a clear sky warms slightly).
 * Indoors the weather doesn't reach, so the bare sector value stands.
 * Part of the Tobin-original heat subsystem (see room.h / sector_heat);
 * gear insulation is applied on top, per-being, in being_effective_heat. */
int room_ambient_heat(const room_t *r) {
    if (!r)
        return HEAT_BASELINE;
    int heat = sector_heat(r->sector);
    if (!(r->room_flag & ROOM_FLAG_INDOORS))
        heat += weather_heat_delta(weather_current());
    return heat;
}

/* True for any sector whose name contains "UNDERWATER" (temperate/tropical
 * underwater sectors) -- used to gate things like breathing/swim checks. */
bool sector_is_underwater(int sector) {
    return strstr(sector_name(sector), "UNDERWATER") != NULL;
}

/* True for open-air sectors with nothing solid underfoot -- ATMOSPHERE
 * (arctic/temperate/tropical/fire) and MAKE FLY, the same "open sky"
 * bucket room_can_plant() already excludes together. Used by fall.c's
 * checkFalling()-equivalent to decide whether a room drops you through
 * to whatever is below (Sneezy → Tobin feature audit, "catfall/
 * catleap"). CLIMBING sectors are deliberately NOT included here --
 * the real upstream's own TerrainInfo table marks them passable
 * ground, not open air (you can fall FROM a cliff face by other means,
 * but standing on one isn't itself a fall risk the way open sky is). */
bool sector_is_fall(int sector) {
    const char *name = sector_name(sector);
    return strstr(name, "ATMOSPHERE") != NULL || strcmp(name, "MAKE FLY") == 0;
}

/* True for any water sector, surface or underwater -- softens fall
 * damage the same way the real upstream's isWaterSector() does (a
 * splash lands better than a sidewalk). Deliberately broader than
 * sector_is_underwater() above (which excludes swimmable surface
 * water on purpose for the drowning check) -- falling cares about
 * "is there water to land in at all," not whether it's deep enough to
 * drown in. */
bool sector_is_water(int sector) {
    const char *name = sector_name(sector);
    return strstr(name, "UNDERWATER") || strstr(name, "OCEAN")
        || strstr(name, "RIVER") || strstr(name, "ICEFLOW");
}

/* True if a room's sector/flags allow planting seeds -- excludes indoors,
 * water/underwater, sky/astral, and solid-rock/lava/inside-mob sectors.
 * Used by the `plant` command (see planting.c) to reject bad locations
 * before starting the dig/sow/cover tick sequence. */
bool room_can_plant(const struct room *r) {
    if (!r)
        return false;
    if (r->room_flag & ROOM_FLAG_INDOORS)
        return false;
    const char *name = sector_name(r->sector);
    if (strstr(name, "UNDERWATER") || strstr(name, "OCEAN") || strstr(name, "RIVER")
        || strstr(name, "ICEFLOW"))
        return false;
    if (strstr(name, "ATMOSPHERE") || strstr(name, "MAKE FLY") || strstr(name, "ASTRAL"))
        return false;
    if (strstr(name, "LAVA") || strstr(name, "SOLID ROCK") || strstr(name, "SOLID ICE")
        || strstr(name, "INSIDE MOB"))
        return false;
    return true;
}

/* Sector-name substring bucketing, same style as sector_color() above --
 * most-specific rule first. See room.h's declaration comment for the
 * weather-prefix simplification. */
const char *room_ground_type(const struct room *r) {
    if (!r)
        return "ground";
    const char *name = sector_name(r->sector);
    if (strstr(name, "UNDERWATER"))
        return "ocean floor";
    if (strstr(name, "CITY"))
        return "street";
    if (strstr(name, "ROAD"))
        return "road";
    if (strstr(name, "OCEAN") || strstr(name, "RIVER") || strstr(name, "ICEFLOW"))
        return "water";
    if (strstr(name, "SWAMP") || strstr(name, "MARSH"))
        return "mud";
    if (strstr(name, "BEACH"))
        return "sand";
    if (r->room_flag & ROOM_FLAG_INDOORS)
        return "floor";
    return "ground";
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

/* Formats every set bit of a room-flags bitmask into buf as bracketed,
 * space-separated names (e.g. "[ ALWAYS-LIT ] [ INDOORS ]"), or "none" if
 * no bits are set. Used by room-inspection/editor commands to show flags
 * in the same style as the original room_bits[] display. */
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

/* Number of defined ROOM_FLAG_NAMES bits -- lets callers (e.g. a redit flag
 * toggle menu) iterate the full set without hardcoding 22 themselves. */
int room_flag_count(void) {
    return 22;
}

/* Single flag name by bit index, or "?" if out of range. */
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

/* Number of defined door types, for editor menus that enumerate them. */
int door_type_count(void) {
    return MAX_DOOR_TYPES;
}

/* Display name for a doorTypeT value (e.g. "Portcullis"), or "?" if out of
 * range. */
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

/* Number of defined exit-condition bits, for editor menus that enumerate
 * them. */
int exit_cond_count(void) {
    return MAX_EXIT_CONDITIONS;
}

/* Single exit-condition name by bit index (e.g. "Locked"), or "?" if out of
 * range. */
const char *exit_cond_name(int bit) {
    if (bit < 0 || bit >= MAX_EXIT_CONDITIONS)
        return "?";
    return EXIT_COND_NAMES[bit];
}

/* Formats every set bit of an exit-condition bitmask into buf as
 * space-separated names (e.g. "Closed Locked"), or "none" if no bits are
 * set. Unlike room_flag_names() above, no per-flag brackets -- matches how
 * exit conditions are displayed inline next to a direction. */
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

/* Allocates and initializes a new room_t (name/description copied in,
 * all exits set to "no exit"). Caller is responsible for linking it into
 * the world's room table. */
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

/* Frees a room_t allocated by room_create(). Does not unlink it from any
 * world table or detach contained things -- callers must do that first. */
void room_destroy(room_t *r) {
    free(r);
}
