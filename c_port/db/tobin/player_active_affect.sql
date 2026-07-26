-- Persistence for affect.h's active_affect_t (buffs/debuffs/status) on a
-- PC. One row per (player_id, affect_type) -- MAX_ACTIVE_AFFECTS is small
-- (4), so a player never has more than a handful of rows here at once.
--
-- Named player_ACTIVE_affect, not player_affect -- that shorter name is
-- already taken by an unrelated, unused table (`type`/`level`/`duration`/
-- `renew`/`modifier`/`modifier2`/`location`/`bitvector` columns, 0 rows)
-- that came in as part of the upstream SneezyMUD seed schema (confirmed
-- 2026-07-26: not referenced anywhere in Tobin's own code, same "inert
-- upstream leftover" status as the drug_use/player_drug precedent) --
-- CREATE TABLE IF NOT EXISTS against that name would have silently no-op'd
-- against the wrong schema instead of creating this one, exactly what
-- happened on the first attempt before this rename.
--
-- CREATE TABLE IF NOT EXISTS, not an unconditional DROP+CREATE, matching
-- player_drug.sql/player_attrs.sql's own fixed convention.
CREATE TABLE IF NOT EXISTS `player_active_affect` (
  `player_id` bigint(20) unsigned NOT NULL,
  `affect_type` int(11) NOT NULL,
  `rounds_left` int(11) NOT NULL DEFAULT 0,
  `modifier` int(11) NOT NULL DEFAULT 0,
  PRIMARY KEY (`player_id`, `affect_type`),
  CONSTRAINT `fk_player_active_affect_player_id` FOREIGN KEY (`player_id`) REFERENCES `player` (`id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;
