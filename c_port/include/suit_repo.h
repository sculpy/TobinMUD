#ifndef TOBIN_SUIT_REPO_H
#define TOBIN_SUIT_REPO_H

#include <stdbool.h>
#include <stddef.h>

/* DB access for the `suit`/`suit_item` tables (db/tobin/suit.sql) -- a
 * named, optionally class-restricted bundle of obj vnums (user 2026-07-26:
 * newbie equipment suits). Pure lookup layer; see suit.h for the actual
 * grant/auto-equip logic that consumes these. */

#define SUIT_MAX_ITEMS 16 /* bumped from 8 (Newbie equipment system
                             expansion, TODO.md priority item, 2026-08-02):
                             a per-race starting-gear suit needs 11 armor
                             pieces + 1 racial weapon + 1 shared shield =
                             13 rows, the old cap of 8 wasn't enough */

/* Finds a suit by name (case-insensitive substring match, same spirit as
 * find_obj()'s keyword matching elsewhere) -- returns its numeric id, or
 * -1 if nothing matches. `out_class` (if non-NULL) is set to the suit's
 * class restriction (a player_class_t value, being.h), or -1 if the suit
 * isn't restricted to any one class. `out_name` (if non-NULL, buffer size
 * `out_name_sz`) is set to the suit's real stored name, so a caller that
 * matched on an abbreviation can echo back the full name. */
int suit_repo_find_by_name(const char *name, int *out_class, char *out_name, int out_name_sz);

/* Finds the suit whose `class` column equals `player_class` (a
 * player_class_t value), or -1 if no suit is defined for that class. */
int suit_repo_find_for_class(int player_class);

/* Finds the suit whose `race` column equals `player_race` (a
 * player_race_t value), or -1 if no suit is defined for that race.
 * Newbie equipment system expansion (TODO.md priority item,
 * 2026-08-02) -- a second, independent suit grant alongside the
 * existing per-CLASS one, so a fresh character gets both their
 * class's weapon/shield AND their race's own starting armor set. */
int suit_repo_find_for_race(int player_race);

/* Loads suit_id's item vnums AND per-item quantities in parallel arrays
 * (capacity `max` each) -- Menu-driven loadsuit editor (TODO.md priority
 * item, 2026-08-02): a suit_item row can now specify MORE than one of
 * the same vnum (two wrist bands, two boots, etc), where before the
 * (suit_id, obj_vnum) primary key meant an item was either in the suit
 * once or not at all. Returns how many DISTINCT rows were loaded (not
 * the sum of quantities) -- suit_grant() expands each by its own qty. */
int suit_repo_load_items_qty(int suit_id, int *vnums, int *qtys, int max);

/* Menu-driven loadsuit editor (`edit suit`, cmd_edsuit.c) CRUD layer
 * below -- mirrors the "list/get/create/set-field" shape other ed*
 * editors' repo layers already use (e.g. obj_repo.h's obj_proto_*). */

/* One row of suit_repo_list_all()'s summary listing. */
typedef struct {
    int id;
    char name[32];
    int class_restrict; /* -1 = any class */
    char description[128];
    int item_count; /* distinct suit_item rows, not summed quantity */
} suit_summary_t;

/* Lists every suit (id order), capacity `max`. Returns how many were
 * loaded. For `edit suit` with no name argument. */
int suit_repo_list_all(suit_summary_t *out, int max);

/* Loads one suit's own scalar fields by id -- true if found. */
bool suit_repo_get(int suit_id, char *name, size_t name_sz,
                    int *out_class, char *description, size_t desc_sz);

/* Creates a brand-new, empty suit (no items yet, class unrestricted,
 * a placeholder description) -- returns its new id, or -1 on a name
 * collision (suit.name is UNIQUE) or other DB error. Mirrors
 * obj_proto_create_blank()'s "auto-create if missing" precedent. */
int suit_repo_create(const char *name);

/* Updates a suit's class restriction (-1 = clear/any class) or
 * description. Both return true on success. */
bool suit_repo_set_class(int suit_id, int class_restrict);
bool suit_repo_set_description(int suit_id, const char *description);

/* Adds one suit_item row (or, if (suit_id, obj_vnum) already exists,
 * overwrites its quantity -- same "upsert" convention as trigger_repo's
 * own add). Returns true on success, false if `obj_vnum` isn't a real
 * obj prototype (the FK would reject it) or any other DB error. */
bool suit_repo_add_item(int suit_id, int obj_vnum, int quantity);

/* Updates an existing suit_item row's quantity. Returns true if the
 * UPDATE executed without a DB error (same "success, not row-match"
 * convention as suit_repo_set_class() above -- doesn't distinguish "no
 * such row" from "matched but unchanged"). */
bool suit_repo_set_item_qty(int suit_id, int obj_vnum, int quantity);

/* Removes one suit_item row. Returns true if the DELETE executed
 * without a DB error (same caveat as suit_repo_set_item_qty() above). */
bool suit_repo_delete_item(int suit_id, int obj_vnum);

#endif
