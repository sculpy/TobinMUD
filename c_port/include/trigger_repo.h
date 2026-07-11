/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_TRIGGER_REPO_H
#define TOBIN_TRIGGER_REPO_H

#include <stdbool.h>

/* Scripted mob/object/room behavior, backed by the `trigger` table
 * (db/sneezy/trigger.sql) -- see that file's header comment for the full
 * design rationale (the in-game-authorable alternative to SneezyMUD's
 * hardcoded spec procs). One row = one trigger attached to a prototype. */

#define TRIGGER_SCRIPT_MAX 1024
#define TRIGGER_MATCH_LEN 64

typedef struct {
    long id;
    char target_type[8];   /* "room" | "mob" | "obj" */
    int target_vnum;
    char trigger_type[16]; /* "enter"/"random" (room); "greet"/"speech"/
                               "death"/"random" (mob); "get"/"wear" (obj) */
    char match_text[TRIGGER_MATCH_LEN]; /* "speech" keyword; empty otherwise */
    int chance_pct;         /* "random" roll percent; 100 (always) otherwise */
    char script[TRIGGER_SCRIPT_MAX]; /* newline-separated actions, trigger.c */
} trigger_t;

/* Creates a new trigger row. `match_text` may be NULL/empty (stored as SQL
 * NULL) for trigger types that don't use it. Returns false on DB error. */
bool trigger_repo_add(const char *created_by, const char *target_type, int target_vnum,
                      const char *trigger_type, const char *match_text, int chance_pct,
                      const char *script);

/* Loads up to `max` triggers matching `target_type`/`target_vnum`/
 * `trigger_type` exactly, into `out`. Returns how many were found (0 if
 * none or on DB error). The event-firing hook points (cmd_move.c,
 * combat.c, cmd_object.c, cmd_say.c, trigger.c's random tick) call this
 * at the moment their event happens -- a live query, not a cache, since
 * triggers fire rarely enough that this isn't a hot path. */
int trigger_repo_load_for(const char *target_type, int target_vnum,
                          const char *trigger_type, trigger_t *out, int max);

/* Loads up to `max` triggers of ANY type attached to `target_type`/
 * `target_vnum`, for a builder reviewing what's already there
 * (`edit trigger list <target_type> <vnum>`). Returns how many were found. */
int trigger_repo_list_for(const char *target_type, int target_vnum,
                          trigger_t *out, int max);

/* Deletes the trigger with the given id. Returns false if no such trigger
 * exists or on DB error. Backs `edit trigger delete <id>`. */
bool trigger_repo_delete(long id);

/* Loads up to `max` DISTINCT target_vnum values that have at least one
 * trigger_type='random' row for `target_type`. Unlike trigger_repo_load_for()
 * this IS meant as a hot-path gate: trigger_random_tick() (trigger.c) calls
 * this once per tick, not once per mob/room, so the vast majority of mobs
 * and rooms (which have no "random" trigger at all) can be skipped with an
 * in-memory lookup instead of a DB round trip apiece -- see the perf bug
 * this fixed (2026-07-11: `aitick`/the real ~60s pulse got seconds slower
 * as the world's loaded-room/mob registry grew, since every single one was
 * getting its own trigger_repo_load_for() call every tick regardless of
 * whether it had a random trigger). */
int trigger_repo_random_vnums(const char *target_type, int *out, int max);

#endif
