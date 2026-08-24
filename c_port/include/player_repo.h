/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_PLAYER_REPO_H
#define TOBIN_PLAYER_REPO_H

#include <stdbool.h>

#include "being.h"

/* C replacement for player persistence, backed by the `player`,
 * `player_attrs`, and `player_progress` tables (db/tobin/player.sql,
 * player_attrs.sql, player_progress.sql). An account can own multiple
 * characters (player.account_id); every lookup by name here is scoped to
 * the owning account_id to enforce that. */

#define PLAYER_NAME_LEN 64 /* matches thing_t.name's size, see thing.h */

/* Default landing rooms (user spec): mortals start/return to Center
 * Square; immortals whose load_room is still the mortal default land in
 * room 1 (Imperia, the immortal entryway) instead -- an explicit
 * `loadroom` choice (any room but 100) overrides both. */
#define DEFAULT_LOAD_ROOM_MORTAL 100
#define DEFAULT_LOAD_ROOM_IMMORTAL 1
#define MAX_CHARS_PER_ACCOUNT 10

/* Loads a player by character name, but ONLY if it's owned by account_id.
 * Also loads its attrs (player_attrs). Returns NULL if no such player
 * exists under that account. Caller owns the result (being_destroy). */
being_t *player_load(const char *name, long account_id);

/* Creates a new player row linked to account_id with a default load_room,
 * plus a player_attrs row seeded from *attrs (or ATTR_BASE defaults if
 * attrs is NULL). `char_class`/`race`/`alignment` are user 2026-07-11
 * additions (chosen at creation, alongside stats/gender/appearance):
 * `attrs` should already have class_stat_bonus()/race_stat_bonus() folded
 * in by the caller (descriptor.c) -- this function just persists whatever
 * it's given, it doesn't apply the bonuses itself. Returns a
 * freshly-allocated being_t. Fails if the name is already taken
 * (player.name is globally unique) or the account has already reached
 * MAX_CHARS_PER_ACCOUNT. */
being_t *player_create(const char *name, long account_id, const attrs_t *attrs,
                       int handed_right, gender_t gender, const char *appearance,
                       player_class_t char_class, player_race_t race, int alignment,
                       player_territory_t territory);

/* Deletes a player (and, via ON DELETE CASCADE, its player_attrs row) --
 * but only if it's owned by account_id. Returns false if not found/not
 * owned, or the delete failed. */
bool player_delete(const char *name, long account_id);

/* Lists up to `max` character names owned by account_id into `names`
 * (each PLAYER_NAME_LEN bytes) and each one's level into `levels` (may be
 * NULL if the caller doesn't want them; a player somehow missing its
 * player_progress row reports level 1), sets *count to how many were
 * found. Returns false only on a DB error. */
bool player_list_by_account(long account_id, char names[][PLAYER_NAME_LEN], int levels[], int max, int *count);

/* The room vnum this player should load into (from `player.load_room`).
 * Returns -1 if the player doesn't exist under that account. */
int player_load_room(const char *name, long account_id);

/* Sets `player.load_room` -- backs the immortal `loadroom` command
 * (cmd_loadroom.c). Account-scoped like the other player mutations. */
bool player_set_load_room(const char *name, long account_id, int vnum);

/* Sets `player.title` -- backs the player `title` command (cmd_title.c).
 * A NULL/empty title clears the column (SQL NULL). Account-scoped. */
bool player_set_title(const char *name, long account_id, const char *title);

/* Sets `player.poofin`/`player.poofout` (custom WALKING move messages) --
 * back the `poofin`/`poofout` commands (cmd_poof.c). A NULL/empty message
 * clears the column (falls back to the default move-message wording). */
bool player_set_poofin(const char *name, long account_id, const char *msg);
bool player_set_poofout(const char *name, long account_id, const char *msg);

/* Sets `player.bamfin`/`player.bamfout` (custom `goto` teleport messages)
 * -- back the `bamfin`/`bamfout` commands (cmd_bamf.c). A NULL/empty
 * message clears the column (falls back to the default puff-of-smoke
 * wording). */
bool player_set_bamfin(const char *name, long account_id, const char *msg);
bool player_set_bamfout(const char *name, long account_id, const char *msg);

/* True if ANY account owns a character with this name (case-insensitive
 * via the column collation) -- backs the duplicate-name rejection at
 * character creation (descriptor.c). */
bool player_name_exists(const char *name);

/* player_id for `name`, or -1 if no such character exists. Used wherever a
 * target is resolved by name for an offline-capable admin action (e.g.
 * `zoneassign`, zone_repo.h) rather than a live in-room being_t. */
long player_id_for_name(const char *name);

/* account_id owning `name`, or -1 if no such character exists. Used by
 * `wipe` (cmd_wipe.c), the one admin action that must reach a character
 * regardless of which account owns it -- every other by-name lookup above
 * is deliberately scoped to a caller-supplied account_id and won't cross
 * accounts. */
long player_account_id_for_name(const char *name);

/* Persists the prompt customization bitmask (cmd_prompt.c). */
bool player_set_prompt_flags(long player_id, int flags);

/* The highest news.id this player has already caught up on (0 if never
 * checked) -- backs the unseen-news notice shown at login (descriptor.c)
 * and gets bumped to news_repo_max_id() whenever they actually read `news`
 * (cmd_news.c). */
long player_get_news_last_seen(long player_id);
bool player_set_news_last_seen(long player_id, long news_id);

/* Same shape as the pair above, for the immortal-only `wiznews` channel
 * (news_repo's `wiz=true` side) -- backs its own "there is new wiznews"
 * login notice (descriptor.c, gated on being_is_immortal()) and gets
 * bumped whenever they actually read `wiznews` (cmd_wiznews.c). */
long player_get_wiznews_last_seen(long player_id);
bool player_set_wiznews_last_seen(long player_id, long news_id);

/* Persists the player-flags bitmask (player.pflags, PLR_*). */
bool player_set_pflags(long player_id, int flags);
/* Persists monk sash quest-chain progress bits + purple-sash leper kill
 * counter (player.monk_quest_flags/monk_purple_kills). */
bool player_set_monk_quest(long player_id, unsigned int flags, unsigned char purple_kills);

/* Loads persisted attributes for player_id into *out. Returns false (and
 * leaves *out untouched) if no player_attrs row exists. */
bool player_attrs_load(long player_id, attrs_t *out);

/* Upserts (insert-or-replace) the player_attrs row for player_id. */
bool player_attrs_save(long player_id, const attrs_t *attrs);

/* Loads persisted level/experience/hp/max_hp for player_id into *out.
 * Returns false (and leaves *out untouched) if no player_progress row
 * exists -- being_create_pc()'s freshly-seeded defaults stand in that case. */
bool player_progress_load(long player_id, progress_t *out);

/* Upserts (insert-or-replace) the player_progress row for player_id. */
bool player_progress_save(long player_id, const progress_t *progress);

/* Persists everything about a live character in one call: attrs,
 * progress (level/xp/hp/etc), and inventory (obj_repo.h). Mirrors the
 * original's real `TBeing::doSave()` (user request, 2026-07-07: "add a
 * save command to manually save your character") -- a genuine port, not
 * a Tobin invention, consolidating the scattered player_attrs_save/
 * player_progress_save/player_inventory_save call sites into one place.
 * Backs the player-invokable `save` command (cmd_save.c) and the
 * quit/death auto-save in descriptor_leave_to_menu() (user, 2026-07-12:
 * "the game should automatically save a char upon death or quit").
 * Limb HP is deliberately NOT included -- it isn't persisted at all
 * (recalculated from strength on every fresh load, see being.c). Returns
 * false if any individual save failed (still attempts all three). */
bool player_save(long player_id, const being_t *b);

/* Sets the persisted level of any player by exact name -- deliberately NOT
 * scoped to an account, unlike everything else here: this backs the
 * immortal-only `promote` command, which acts across accounts. Returns
 * false if no such player exists (or on DB error). */
bool player_set_level_by_name(const char *name, int level);

/* Loads a player by exact name, ANY account (unlike player_load) -- backs
 * the immortal-only `edplayer` command, which (like `promote`) acts across
 * accounts. Also loads attrs/progress like player_load does. *out_load_room
 * (may be NULL) receives player.load_room, which isn't part of being_t.
 * Returns NULL if no such player exists. Caller owns the result
 * (being_destroy). */
being_t *player_load_admin(const char *name, int *out_load_room);

/* Admin-wide (not account-scoped) setters for the fields player_load_admin
 * exposes but that had no post-creation setter yet -- back `edplayer`,
 * same not-account-scoped precedent as player_set_level_by_name. */
bool player_set_gender_by_name(const char *name, gender_t gender);
bool player_set_handed_by_name(const char *name, int handed_right);
bool player_set_appearance_by_name(const char *name, const char *appearance);
bool player_set_class_by_name(const char *name, player_class_t cls);
bool player_set_race_by_name(const char *name, player_race_t race);

#endif
