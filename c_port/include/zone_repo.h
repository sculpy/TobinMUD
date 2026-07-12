/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_ZONE_REPO_H
#define TOBIN_ZONE_REPO_H

#include <stdbool.h>

/* DB access for zone reset data (Zones Part 2, Session 43) -- the
 * upstream-migrated `zone`/`zone_reset` tables (Zones Part 1, Session 38,
 * ~36k reset rows across 335 zones) are read directly, no new tables
 * needed. See zone.h for what actually executes this data. */

typedef struct {
    int zone_nr;
    char name[64];
    bool enabled;
    int bottom;
    int top;
    int lifespan; /* minutes between periodic resets */
} zone_t;

typedef struct {
    int cmd_no;
    char command; /* 'M'/'O'/'E'/'G'/'P'/'D' handled; others logged+skipped */
    int if_flag;  /* nonzero: skip this row if the previous one didn't fire */
    int arg1, arg2, arg3, arg4;
} zone_reset_cmd_t;

/* Loads every zone row (ordered by zone_nr) into `out`, returns the count
 * actually loaded (capped at `max`). */
int zone_repo_load_all(zone_t *out, int max);

/* Loads a single zone row. False if zone_nr doesn't exist. */
bool zone_repo_load_one(int zone_nr, zone_t *out);

/* Persists a zone's editable properties (name/enabled/lifespan/bottom/top)
 * -- backs `edzone`'s Save (descriptor.c). */
bool zone_repo_save(const zone_t *z);

/* Names (not just player_ids) of every builder assigned to `zone_nr`, for
 * display in `edzone`. Returns the count actually loaded (capped at `max`). */
int zone_repo_load_owner_names(int zone_nr, char names[][64], int max);

/* Loads zone `zone_nr`'s reset commands, ordered by cmd_no (execution
 * order matters -- see zone.c), into `out`. Returns the count actually
 * loaded (capped at `max`). */
int zone_repo_load_resets(int zone_nr, zone_reset_cmd_t *out, int max);

/* Appends one new reset row for `zone_nr` at `cmd_no` (caller's
 * responsibility to pick one higher than any existing row for that zone,
 * so it executes after everything already there -- see zone.c's
 * zone_file_create()). `comment` may be "" but not NULL. Fails (false) if
 * `cmd_no` collides with an existing row (primary key is zone_nr+cmd_no). */
bool zone_repo_insert_reset_cmd(int zone_nr, int cmd_no, char command, int if_flag,
                                 int arg1, int arg2, int arg3, int arg4,
                                 const char *comment);

/* Zone ownership (Session 43, user: "add identity to zones"). Backs
 * `zoneassign` (cmd_zoneassign.c) and the edit gate (zone.h's
 * zone_can_edit()). A zone can have multiple assigned builders; a builder
 * can be assigned to multiple zones. */

/* True iff `player_id` is assigned to `zone_nr`. */
bool zone_repo_is_assigned(int zone_nr, long player_id);

/* Assigns `player_id` to `zone_nr`. Idempotent (no-op if already assigned). */
bool zone_repo_assign(int zone_nr, long player_id);

/* Un-assigns `player_id` from `zone_nr`. True even if they weren't assigned. */
bool zone_repo_unassign(int zone_nr, long player_id);

/* Sets zone `zone_nr`'s vnum range (the `zone` table's bottom/top columns)
 * -- `zone assign` (cmd_zone.c) sets this alongside the ownership grant
 * (user spec, Session 43: "when assigning a zone there should be a vnum
 * range that gets assigned along with that"). A no-op if the zone doesn't
 * exist yet (plain UPDATE, not an upsert -- `zone assign` only makes sense
 * for a zone already migrated in by Zones Part 1). */
bool zone_repo_set_range(int zone_nr, int bottom, int top);

#endif
