#ifndef TOBIN_SUIT_REPO_H
#define TOBIN_SUIT_REPO_H

/* DB access for the `suit`/`suit_item` tables (db/tobin/suit.sql) -- a
 * named, optionally class-restricted bundle of obj vnums (user 2026-07-26:
 * newbie equipment suits). Pure lookup layer; see suit.h for the actual
 * grant/auto-equip logic that consumes these. */

#define SUIT_MAX_ITEMS 8 /* generous cap -- real suits are ~4 items */

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

/* Loads suit_id's item vnums into `vnums` (capacity `max`). Returns how
 * many were actually loaded. */
int suit_repo_load_items(int suit_id, int *vnums, int max);

#endif
