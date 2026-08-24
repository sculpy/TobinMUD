/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_MOB_REPO_H
#define TOBIN_MOB_REPO_H

#include <stdbool.h>

#include "being.h"

/* DB access for mob prototypes: the upstream-seeded `mob` table
 * (db/tobin/mob.sql) is read directly -- no new Tobin table needed, same
 * "prototypes already exist" precedent as obj_repo.h.
 *
 * As of `edit mob` (medit, 2026-07-25, the last builder-tools-OLC gap --
 * see TODO.md), this struct widened from the original 12-field subset
 * `being_create_mob()` alone needed to cover every column the real
 * upstream's own `send_mob_menu()` (misc/create_mobs.cc) exposes AND
 * Tobin's schema actually has -- see mob_repo.c's mob_proto_save() doc
 * comment for the two real upstream fields disclosed as still out of
 * scope (Immunities: no column at all; the real menu's own slot 15,
 * labeled "unused" with no case in ITS OWN dispatcher either). Several
 * columns here are real, saveable data that Tobin's live game logic
 * doesn't consume yet (affects, long_descr, letter, tohit/ac/damage_level/
 * damage_precision/attacks -- see being_create_mob()'s doc comment for
 * why combat stats are level-derived instead of copied from these
 * upstream-scale columns) -- editable now so a builder's work isn't lost
 * once that consumption lands, same "author now, wire up later" spirit
 * `mob.align`/`mob.race` already went through before their own AI/loot
 * consumption shipped. */

typedef struct {
    char name[64];              /* matches thing_t.name */
    char short_descr[128];      /* matches thing_t.short_descr */
    char long_descr[256];       /* room-listing text -- real column, not yet
                                  * read by being_create_mob() (a pre-existing
                                  * gap, not introduced by medit) */
    char description[BEING_APPEARANCE_LEN]; /* the "look <mob>" closer text */
    int actions;                /* ACT_* bitmask (misc/defs.h) -- only bits 1
                                  * (ACT_SENTINEL) and 2 (ACT_SCAVENGER) are
                                  * consumed today (mob_ai.c); the rest is
                                  * real seeded data, not yet acted on */
    int affects;                 /* AFF_* bitmask -- not consumed anywhere yet */
    int faction;
    int fact_perc;
    double attacks;
    int level;
    int tohit;
    double ac;
    double hpbonus;              /* the original's real HP-scaling parameter,
                                   * not a flat "+bonus" -- see being.c */
    double damage_level;
    int damage_precision;
    int gold;                    /* the flat gold-drop-on-kill amount */
    int race;                    /* raw index into MOB_RACE_NAMES[]/
                                   * mob_race_name() (being.c) */
    int body_type;               /* body_type_t (body.h) -- Tobin-added
                                   * column (Body types, 2026-07-26), not
                                   * an upstream field: the real seeded
                                   * `mob` table never carried body-shape
                                   * data at all. Defaults to BODY_HUMANOID
                                   * (1) for every mob unless
                                   * tobin_migrations.sql's name-matching
                                   * pass classified it otherwise. Not yet
                                   * exposed in `edit mob` (medit) -- round-
                                   * trips correctly via SQL/this struct,
                                   * just no interactive UI for it yet. */
    int weight;
    int height;
    int str, bra, con, dex, agi, intel, wis, foc, per, cha, kar, spe; /* the
        real upstream's own 12-stat scale (NOT copied onto a live mob's
        attrs_t -- see being_create_mob()'s doc comment for why). medit
        only exposes the 6 with a real Tobin attrs_t analog (str/con/wis/
        intel/dex/cha, user 2026-07-25: "should be 6 not 12") -- the other
        6 (bra/agi/foc/per/kar/spe) round-trip through load/save unedited,
        same "real data, not consumed yet" spirit as the rest of this
        struct's disclosed gaps. */
    int def_position;            /* position_t, e.g. standing/sitting */
    int sex;
    int spec_proc;                /* see mob_repo_get_spec_proc()'s own doc
                                    * comment -- same real seeded value */
    int skin;
    int vision;
    int can_be_seen;
    int max_exist;                /* upstream world-wide instance cap, 0 =
                                    * uncapped -- see cmd_load.c's `load` */
    char local_sound[256];
    char adjacent_sound[256];
    int align;                    /* Tobin-added `mob.align` column (NOT an
                                    * upstream field) -- -1 evil, 0 unaligned,
                                    * 1 good; see mob_ai.c's mob_try_aggress() */
    int class_mask;               /* Upstream `mob.class` bitmask (1 mage, 2
                                    * cleric, 4 warrior, 8 thief, 16 shaman,
                                    * 32 deikhan, 64 monk, 128 ranger, 256
                                    * other) -- being_create_mob() maps the
                                    * recognizable single-class bits to a
                                    * Tobin player_class_t */
} mob_proto_t;

/* Loads the prototype row for `vnum` from the `mob` table into *out.
 * Returns false if no such vnum exists. */
bool mob_proto_load(int vnum, mob_proto_t *out);

/* Writes every field of `p` back to its existing `mob` row (p->name is used
 * only to find which row via... no -- the vnum is threaded through
 * separately, see mob_proto_save()'s own definition). EDIT-ONLY, same
 * scope boundary `edit object`/`edit room` draw: there is no way to
 * allocate a brand-new mob vnum here. Returns false on DB error or if the
 * vnum doesn't exist. */
bool mob_proto_save(int vnum, const mob_proto_t *p);

/* Inserts a brand-new, minimal `mob` row at `vnum` -- descriptor_medit_
 * begin() calls this automatically when `edit mob <vnum>` targets a vnum
 * with no existing row (2026-07-25, user: "if one doesn't exist a blank
 * one should be created", then "objects and rooms should behave the
 * same" -- see obj_proto_create_blank()/room_create() for the equivalent
 * on the other two editors). Returns false if `vnum` already exists (a
 * PRIMARY KEY collision) or on any other DB error. */
bool mob_proto_create_blank(int vnum);

/* Finds the lowest vnum whose `name` column contains `name` (case-
 * insensitive substring, e.g. "demon" matches "a vrock demon"), or -1 if
 * none match. Backs `mload <name>` (cmd_mload.c) as an alternative to a
 * bare vnum. */
int mob_find_vnum_by_name(const char *name);

/* The `spec_proc` column for mob `vnum`, or -1 if the mob doesn't exist.
 * Not part of mob_proto_t/mob_proto_load() -- Tobin has no spec-proc
 * EXECUTION engine (triggers.c replaced that concept), but the seeded
 * value itself is still real, useful data: shop_repo_is_hospital()
 * (shop_repo.h) reads it to tell a "doctor" shopkeeper (spec_proc 48 in
 * the original engine) from an ordinary one, purely as a lookup key --
 * no original spec-proc behavior is executed. */
int mob_repo_get_spec_proc(int vnum);

/* Deletes every mob prototype row (and its mob_extra/mob_imm/
 * mobresponses rows) with vnum in [low, high] -- `zone reclaim`
 * (cmd_zone.c). Returns the count of `mob` rows deleted (0 on DB error
 * or an empty range). DB-only, same caveat as room_repo_delete_range()
 * (room_repo.h). */
int mob_repo_delete_range(int low, int high);

/* Opt-in per-vnum prototype cache for mob_proto_load(), OFF by default --
 * with it inactive, mob_proto_load() hits the DB every single call,
 * exactly as before this existed. Activate only around a bounded,
 * single-threaded burst of REPEAT lookups where nothing else can be
 * concurrently editing prototypes (see game_loop.c's copyover_recover(),
 * which restores every loose room mob from the copyover dump -- found
 * live, 2026-08-04: a growing world population meant a growing number of
 * literal duplicate mob_proto_load() DB round trips for the same handful
 * of vnums on every single copyover, the actual driver behind "copyover
 * hangs a bit when restoring"). Deliberately NOT a general always-on
 * cache: medit's mob_proto_save() and the many `sql()`-driven raw DB
 * edits this project's own smoke tests and migrations rely on (e.g.
 * tobin_migrations.sql's body_type backfill) both depend on the very
 * next mob_proto_load() reflecting a change with no server restart --
 * an always-on cache would silently break that. mob_proto_cache_end()
 * always fully clears the cache, so no entry can outlive the one call
 * site that opted in. */
void mob_proto_cache_begin(void);
void mob_proto_cache_end(void);

#endif
