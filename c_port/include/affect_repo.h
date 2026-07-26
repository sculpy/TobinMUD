#ifndef TOBIN_AFFECT_REPO_H
#define TOBIN_AFFECT_REPO_H

#include <stdbool.h>

#include "affect.h"

/* Persistence for affect.h's active_affect_t (buffs/debuffs/status) on a
 * PC -- user 2026-07-26: the original SneezyMUD's own charFile round-trips
 * active affects through every login/logout (docs/systems/critical/
 * 08-persistence-storage.md), so a reconnect or a deliberate quit!/relog
 * shouldn't just wipe a buff or debuff the way Tobin's original "session-
 * scoped, meaningless across a reconnect" comment assumed. Note this is
 * ORTHOGONAL to the quick linkdead-reconnect path (world_find_linkdead_pc(),
 * within LINKDEAD_PURGE_SECONDS): that path reuses the same live being_t
 * directly and was never actually losing affects -- this persistence layer
 * only matters once the being_t is genuinely destroyed and reloaded fresh
 * (a real quit!, or the 5-minute linkdead auto-purge, or a plain restart).
 *
 * AFFECT_CHARMED/AFFECT_POLYMORPH are deliberately never saved here even
 * if somehow present -- both live on a temporary/summoned MOB body, not a
 * real player's own being_t (see affect.h's enum comments), so they should
 * never reach player_save() at all; skipped defensively rather than
 * assumed impossible.
 *
 * Table is `player_active_affect`, NOT the shorter `player_affect` --
 * that name is already taken by an unrelated, unused (0-row) table that
 * came in as part of the upstream SneezyMUD seed schema; CREATE TABLE IF
 * NOT EXISTS against it would have silently no-op'd against the wrong
 * columns instead of creating this one (caught live 2026-07-26).
 *
 * Load is bookkeeping-only: it does NOT re-apply a stat-affect's
 * `modifier` to attrs_t. player_attrs_save() (player_repo.c) always saves
 * the LIVE, already-modified b->attrs blob as a single snapshot, so any
 * active stat modifier is already baked into the reloaded attrs -- re-
 * applying it here on top would double it. affect_repo_load_all() just
 * restores the affects[] slot metadata (type/rounds_left/modifier) so
 * `affects`'s display, being_has_affect(), and normal expiry-driven
 * reversal (reverse_stat_modifier(), affect.c) all keep working. */

/* Loads every affect row this player has into `affects` (fixed-size
 * MAX_ACTIVE_AFFECTS array) -- rows with none simply stay however the
 * caller already initialized them (a fresh being_t is calloc'd to all
 * AFFECT_NONE). Extra rows beyond MAX_ACTIVE_AFFECTS (shouldn't happen --
 * nothing writes more than that) are silently ignored rather than
 * overflowing. */
void affect_repo_load_all(long player_id, active_affect_t affects[MAX_ACTIVE_AFFECTS]);

/* Replaces this player's whole saved affect set with the current live
 * one -- a full delete-then-insert (not an upsert per slot), since unlike
 * drug.h's history-only fields, affects come and go and old rows must not
 * linger once expired/removed. AFFECT_NONE slots are simply skipped. */
bool affect_repo_save_all(long player_id, const active_affect_t affects[MAX_ACTIVE_AFFECTS]);

#endif
