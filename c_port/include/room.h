/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_ROOM_H
#define TOBIN_ROOM_H

#include <stdbool.h>
#include <stddef.h>

#include "thing.h"

/* C replacement for the TRoom slice of misc/thing.h + the `room` DB table
 * (db/tobin/room.sql). Phase 1 keeps only the fields needed for `look`. */

/* Quadrupled 2026-07-28 (user: paste-in-editor headroom for long
 * descriptions) -- see descriptor.h's DESC_LINE_MAX comment for the
 * REAL bottleneck this was paired with (a much smaller per-line input
 * cap was silently dropping pasted text long before this storage limit
 * ever mattered). Shared with HELP_BODY_MAX (help_repo.h) -- the two
 * must stay equal, see descriptor.h's edit_buf comment. */
#define ROOM_DESCRIPTION_MAX 16384
/* The original dirTypeT's full set, IN ITS ORDER: north(0), east(1),
 * south(2), west(3), up(4), down(5), northeast(6), northwest(7),
 * southeast(8), southwest(9) -- confirmed against constants.cc's rev_dirs
 * table. All 10 carried as of Session 21 (the seed DB's diagonal exit
 * rows load again instead of being dropped). */
#define ROOM_NUM_EXITS 10

/* "north", "east", ... indexed by direction; and each direction's reverse
 * (north->south etc), a straight port of the original's rev_dirs. */
extern const char *const DIR_NAMES[ROOM_NUM_EXITS];
extern const int REV_DIR[ROOM_NUM_EXITS];

/* Sector-type names, a straight port of the original's sectorTypeT
 * (misc/enum.h, 61 entries, SECT_SUBARCTIC=0 .. SECT_DEAD_WOODS=60).
 * Out-of-range values render as "unknown". */
#define MAX_SECTOR_TYPES 61
const char *sector_name(int sector);

/* Lowercase base color-tag letter (see colorstring.h) for a sector, grouped
 * by what the sector name implies (lava/fire -> red, city/road/building/
 * mountain/cave/solid rock -> white, ocean/river/beach -> blue, arctic/
 * atmosphere -> cyan, desert -> yellow, swamp/forest/jungle/grassland/
 * plains/hills -> green, astral -> purple, anything else -> white).
 * Deliberately never returns 'k'/'K' (gray/black, user spec) -- unreadable
 * on a black terminal background. `look` uses the uppercase (bright) tag
 * for the room name and this lowercase (dim) one for the description --
 * see cmd_look.c. */
char sector_color(int sector);

/* Vitality cost (1-6) of moving INTO a sector (Sneezy → Tobin feature
 * audit, "Vitality stat + Terrain movement cost"). The original's
 * TerrainInfo[MAX_SECTOR_TYPES] (misc/constants.cc) gives every one of
 * the 61 sectors its own hand-tuned movement/thickness/hunger/thirst/
 * drunk/temp/humidity row; Tobin has no per-sector content to justify
 * that granularity yet, so this reuses sector_color()'s own
 * substring-grouping precedent to bucket sectors into six cost tiers by
 * name instead (roads/cities cheapest, lava/solid rock/fire priciest).
 * No swim/fly/mount modifiers -- those stay blocked on the still-open
 * "Water, drowning, flight" and "Mount / riding" audit items; water
 * sectors are just an expensive-but-walkable tier for now. cmd_move.c
 * charges the average of the source and destination sector's cost,
 * mirroring the original's own average-of-two-sectors rawMove() rule. */
int sector_move_cost(int sector);

/* Per-sector hunger/thirst drain weight (1-6), the (c) per-sector-effects
 * slice of the room-flag TODO. Straight in spirit from the original's
 * TerrainInfo hunger/thirst columns (misc/constants.cc) fed to
 * TBeing::foodNDrink() (obj/obj_food.cc): a higher number means that
 * sector burns that vital faster (desert thirst=6, savannah=5; arid/hot
 * terrain parches, cold/exertion terrain starves). Same substring
 * bucketing precedent as sector_move_cost() above rather than a full
 * 61-row port -- deserts/savannah/veldt drive thirst, mountains/climbing/
 * forest drive hunger; ordinary terrain sits at the baseline 2, matching
 * the flat 2/2 every temperate/arctic row carries upstream. Consumed by
 * vitals.c's drain tick. Baseline 2 == no change from the old flat rate. */
int sector_thirst_rate(int sector);
int sector_hunger_rate(int sector);

/* Ambient heat of a sector (Tobin-original heat subsystem, user
 * 2026-08-17) -- a DELIBERATE INVENTION, not a port: SneezyMUD defines a
 * TerrainInfo heat column (misc/constants.cc) but nothing in that engine
 * ever reads it, so there is no upstream behaviour to mirror. Values
 * reuse the original data's own scale for familiarity (lava ~140, desert
 * 120, tropics ~100, temperate ~60, arctic <=0), bucketed by the same
 * sector-name substring precedent as sector_move_cost() above. Consumed
 * by vitals.c (heatstroke/hypothermia HP chip past the DAMAGE thresholds
 * below, outdoors only, saved by a race heat/cold resist roll) and
 * cmd_move.c (a cosmetic sweat/shiver cue past the STRESS thresholds). */
int sector_heat(int sector);

/* Heat-subsystem thresholds (see sector_heat()). STRESS = cosmetic
 * discomfort cue on entry; DAMAGE = a 1-HP chip per drain tick. Shared by
 * vitals.c and cmd_move.c so the two layers agree on where "extreme"
 * begins. Temperate baseline (~60) sits comfortably between them. */
#define HEAT_STRESS_HOT   95
#define HEAT_STRESS_COLD  15
#define HEAT_DAMAGE_HOT   120
#define HEAT_DAMAGE_COLD  0

/* True for a genuinely UNDERWATER sector (e.g. "TEMPERATE UNDERWATER"),
 * false for surface water (OCEAN/RIVER SURFACE/ICEFLOW -- swimmable,
 * not a drowning risk) or anything dry. Sneezy → Tobin feature audit,
 * "Water, drowning, flight": gates vitals_tick_run()'s (vitals.c)
 * drowning check the same way the original's checkDrowning() only
 * fires for a true underwater sector, not merely a wet one. */
bool sector_is_underwater(int sector);

/* True for open-air/no-floor sectors (catfall/catleap, Sneezy → Tobin
 * feature audit) -- see room.c's own doc comment for the exact bucket. */
bool sector_is_fall(int sector);

/* True for any water sector, surface or underwater -- see room.c's own
 * doc comment for why it's broader than sector_is_underwater() above. */
bool sector_is_water(int sector);

/* True if seeds can be sown here (Planting, Sneezy → Tobin feature
 * audit): not indoors, not on/under water, not on bare rock/lava, not
 * open-air atmosphere -- the same four categories the original's
 * doSeedPlant() refuses (isFallSector/isWaterSector/isIndoorSector/
 * isUnderwaterSector), matched here by ROOM_FLAG_INDOORS plus sector-name
 * keyword bucketing (same substring-match precedent as sector_color()). */
bool room_can_plant(const struct room *r);

/* Renders the set ROOM_* flag bits (original misc/room.h, 22 bits) into
 * buf as space-separated names ("always-lit indoors ..."), or "none".
 * Returns buf for convenience. */
const char *room_flag_names(int flags, char *buf, size_t size);

/* Per-bit access to the ROOM_* flag table, for the flag-toggle submenu in
 * the room builder. `bit` in [0, room_flag_count()); out of range -> "?". */
int room_flag_count(void);
const char *room_flag_name(int bit);

/* Bit 3 of ROOM_FLAG_NAMES (room.c) -- matches the upstream ROOM_INDOORS
 * bit position verbatim. Named here since room_ground_type() needs to test
 * it directly, not just display it. */
#define ROOM_FLAG_INDOORS (1 << 3)

/* Bit 0 of ROOM_FLAG_NAMES (room.c) -- matches the upstream ROOM_ALWAYS_LIT
 * bit position verbatim. Named here since the "Weather & light levels"
 * audit item's darkness gate (cmd_look.c/cmd_exits.c) needs to test it
 * directly, not just display it. */
#define ROOM_FLAG_ALWAYS_LIT (1 << 0)

/* Bit 2 of ROOM_FLAG_NAMES (room.c) -- matches the upstream ROOM_NO_MOB bit
 * position verbatim. Named here since mob_ai.c's wander logic needs to
 * test it directly. */
#define ROOM_FLAG_NO_MOB (1 << 2)

/* Bit 16 of ROOM_FLAG_NAMES (room.c) -- matches the upstream ROOM_HOSPITAL
 * bit position verbatim. Named here since cmd_goto.c's `goto hospital`
 * landmark search needs to test it directly. */
#define ROOM_FLAG_HOSPITAL (1 << 16)

/* Bits 1/6/9/13 of ROOM_FLAG_NAMES (room.c) -- DEATH/NO-ESCAPE/PRIVATE/
 * HAVE-TO-WALK, matching the upstream bit positions verbatim. Named here
 * for the spell/skill functional-completeness audit's `teleport` (Mage,
 * 19, cmd_cast.c): real upstream's genericTeleport() excludes a random
 * destination room flagged DEATH/PRIVATE/HAVE-TO-WALK, and refuses to
 * even attempt the spell if the CASTER's own room is flagged NO-ESCAPE. */
#define ROOM_FLAG_DEATH (1 << 1)
#define ROOM_FLAG_NO_ESCAPE (1 << 6)
#define ROOM_FLAG_PRIVATE (1 << 9)
#define ROOM_FLAG_HAVE_TO_WALK (1 << 13)

/* Bit 14 of ROOM_FLAG_NAMES (room.c) -- ARENA, matching the upstream bit
 * position verbatim. Named here for `word of recall` (Cleric, 21,
 * cmd_pray.c): real upstream refuses to recall out of an arena or
 * NO-ESCAPE room. */
#define ROOM_FLAG_ARENA (1 << 14)

/* Bit 12 of ROOM_FLAG_NAMES (room.c) -- NO-FLEE, matching the upstream
 * ROOM_NO_FLEE bit verbatim. Distinct from NO-ESCAPE (bit 6): NO-ESCAPE
 * blocks only MAGICAL exits (teleport/word-of-recall, already gated in
 * cmd_cast.c/cmd_pray.c), while NO-FLEE blocks the physical `flee`
 * command (upstream offense.cc: "a strange power prevents you from
 * escaping"). Only ~77 live rooms carry it (vs ~2325 for NO-ESCAPE), so
 * gating `flee` on it is a small, deliberate arena/trap-room effect, not
 * a broad balance change. Tested directly in cmd_flee.c. */
#define ROOM_FLAG_NO_FLEE (1 << 12)

/* Sector-based ground-surface word (Sneezy's TRoom::describeGroundType(),
 * misc/create_rooms.cc) -- "street", "road", "water", "mud", "sand",
 * "floor" (indoors), or "ground" (default). Backs the `$$g`/`$g` token in
 * object descriptions (see obj_apply_ground_token(), obj.h). Simplified
 * from the original: no weather-prefix component ("snow-covered ground",
 * "rain-slick street", ...) -- Tobin has no weather system yet. */
const char *room_ground_type(const struct room *r);

/* Exit door types (original doorTypeT, misc/room.h) -- None/Door/Trapdoor/
 * ... Chosen when editing a room exit. */
#define MAX_DOOR_TYPES 11
int door_type_count(void);
const char *door_type_name(int t);

/* Exit condition bits (original exit_bits, MAX_DOOR_CONDITIONS) --
 * Closed/Locked/Secret/... A per-exit bitmask, edited by toggle. */
#define MAX_EXIT_CONDITIONS 11
int exit_cond_count(void);
const char *exit_cond_name(int bit);
/* Renders the set condition bits into buf, space-separated, or "none". */
const char *exit_cond_names(int flags, char *buf, size_t size);

/* Named bits for the three conditions door mechanics actually act on
 * (open/close/movement-blocking/hiding) -- see cmd_move.c, cmd_open.c,
 * cmd_look.c, cmd_exits.c. The rest (Trapped, Caved-In, ...) are still
 * builder-editable via redit's toggle submenu but have no behavior yet. */
#define EXIT_COND_CLOSED (1 << 0)
#define EXIT_COND_LOCKED (1 << 1)
#define EXIT_COND_SECRET (1 << 2)
/* Trap mechanics (user 2026-07-11: "...then weapon depth, trap
 * mechanics" -- sequenced after weapon depth). A Thief's "set trap
 * (door)"/"disarm trap" skills (skill.c) toggle this bit on a closed
 * door via `settrap`/`disarmtrap` (cmd_trap.c); walking through a
 * trapped door (cmd_move.c) springs it -- one-shot damage, then the
 * bit clears -- unless the mover knows "detect trap" and spots it
 * first. Was already a named-but-inert bit (EXIT_COND_NAMES, room.c)
 * before this. */
#define EXIT_COND_TRAPPED (1 << 5)

typedef struct room {
    thing_t base;               /* first member -- see thing.h */
    int vnum;
    char description[ROOM_DESCRIPTION_MAX];
    int sector;
    int room_flag;              /* original's room_flag bitmask -- carried +
                                 * shown to immortals; no behavior yet */
    int capacity;               /* room `capacity` column (moblim); builder-set */
    int height;                 /* room `height` column; builder-set */
    int exits[ROOM_NUM_EXITS];  /* destination vnum per direction, -1 = no exit */
    int exit_door[ROOM_NUM_EXITS]; /* doorTypeT per exit (0 = DOOR_NONE) */
    int exit_cond[ROOM_NUM_EXITS]; /* exit condition bitmask per exit */
    int exit_key[ROOM_NUM_EXITS]; /* roomexit.key_num -- vnum of the KEY-category
                                 * object that locks/unlocks this exit (0/-1 =
                                 * no keyhole). Real upstream seed data, was
                                 * loaded and silently discarded before `lock`/
                                 * `unlock` existed to read it -- see cmd_lock.c. */
} room_t;

room_t *room_create(int vnum, const char *name, const char *description, int sector);
void room_destroy(room_t *r);

#endif
