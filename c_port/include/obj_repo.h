/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_OBJ_REPO_H
#define TOBIN_OBJ_REPO_H

#include <stdbool.h>

#include "being.h"
#include "obj.h"

/* DB access for objects: prototype rows (the upstream-seeded `obj` table,
 * db/sneezy/obj.sql -- unchanged, real world content, no Tobin migration
 * needed) plus the new Tobin-only `player_inventory` table that persists
 * which prototype instances a player is carrying/wearing/holding across a
 * reconnect (db/sneezy/player_inventory.sql). Room-floor objects (via bare
 * `oload`) are NOT persisted -- there's no zone-reset system yet to
 * repopulate them at boot, so they're lost on restart; see STATUS.md. */

#define OBJ_NAME_LEN 64        /* matches thing_t.name */
#define OBJ_SHORT_DESCR_LEN 128 /* matches thing_t.short_descr */

typedef struct {
    char name[OBJ_NAME_LEN];
    char short_descr[OBJ_SHORT_DESCR_LEN];
    char long_descr[OBJ_LONG_DESCR_LEN];
    int type;          /* raw upstream itemTypeT value -- category_for_item_type() */
    int wear_flag;
    int val[4];
    double weight;
    int price;
    bool can_be_seen;
    int max_struct;
    int cur_struct;
    int volume;
    int material;
    int max_exist; /* upstream world-wide instance cap, 0 = uncapped -- see
                     * cmd_load.c's `load` command for the one place Tobin
                     * reads this (a warning, not an enforced cap). */
    int decay_time; /* upstream `decay` column, verbatim -- see obj_t's own
                      * decay_time doc comment (obj.h) for the convention. */
} obj_proto_t;

/* Loads the prototype row for `vnum` from the `obj` table into *out. Returns
 * false if no such vnum exists. */
bool obj_proto_load(int vnum, obj_proto_t *out);

/* Finds the lowest vnum whose `name` column contains `name` (case-
 * insensitive substring, e.g. "sword" matches "a rusty sword"), or -1 if
 * none match. Backs `oload <name>` (cmd_oload.c) as an alternative to a
 * bare vnum. */
int obj_find_vnum_by_name(const char *name);

/* Sums `vnum`'s hit/damroll bonuses out of the upstream-seeded `objaffect`
 * table (vnum, type, mod1, mod2 -- untouched real content, same precedent
 * as `obj` itself). `type` is the original applyTypeT enum
 * (misc/enum.h): 15=APPLY_HITROLL, 16=APPLY_DAMROLL, 17=APPLY_HITNDAM
 * (applies to both at once) -- every other type (stat bonuses, AC,
 * immunities, ...) is irrelevant to combat_strike() and ignored here.
 * `mod1` is the bonus amount; `mod2` is unused for these three types.
 * Both out-params are ALWAYS set (0 if the vnum has no matching rows or
 * doesn't exist) -- never left uninitialized. */
void obj_load_combat_mods(int vnum, int *hitroll, int *damroll);

/* Sums `vnum`'s stat/HP/Vitality/AC affects out of the same real, upstream-
 * seeded `objaffect` table -- the subset of applyTypeT this port's
 * simplified 6-stat model has a home for (see obj_t's own doc comment in
 * obj.h for the full type-number breakdown and the AC sign-flip). Called
 * ONCE per instance, at creation (obj_create_from_proto(), obj.c), which
 * caches the results on the obj_t itself (aff_str/aff_dex/.../aff_ac) --
 * not re-queried on every wear/remove or (AC specifically, a combat hot
 * path) every hit-roll. All eight out-params are ALWAYS set (0 if the
 * vnum has no matching rows or doesn't exist), never left uninitialized. */
void obj_load_stat_affects(int vnum, int *str, int *dex, int *con, int *intel,
                           int *wis, int *cha, int *hit, int *move, int *ac);

/* player_inventory.slot encoding (db/sneezy/player_inventory.sql): -1 is
 * carried loose, 0..LIMB_COUNT-1 is a worn limb_t index, and these two
 * sentinels are the held[] pair. Kept out of obj.h's WEAR_SLOT_* sentinels
 * (those describe wear_slot_for_flag()'s return value, a different small
 * negative-int space) to avoid confusing the two call sites. */
#define INV_SLOT_CARRIED      (-1)
#define INV_SLOT_HELD_PRIMARY (-2)
#define INV_SLOT_HELD_OFFHAND (-3)

/* Loads every persisted carried/worn/held instance for player_id onto `b`
 * (recreating each via obj_create_from_proto() and attaching it to b's
 * carried list, or into equipment[]/held[] per the saved slot) -- called
 * from player_repo.c's player_load()/player_load_admin(), same call site as
 * player_attrs_load/player_progress_load. A vnum whose prototype has since
 * been removed from `obj` is silently skipped (logged), not fatal. */
void player_inventory_load(long player_id, being_t *b);

/* Replaces player_id's persisted inventory with b's current carried list +
 * equipment[]/held[] contents. Called at the same points
 * player_attrs_save/player_progress_save are. */
bool player_inventory_save(long player_id, const being_t *b);

#endif
