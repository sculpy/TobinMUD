-- Persisted attributes for a player (db/sneezy/player_attrs.sql).
-- Tobin-specific (not part of the upstream seed).
--
-- CREATE TABLE IF NOT EXISTS -- NOT an unconditional DROP+CREATE. This file
-- used to open with `DROP TABLE IF EXISTS` (a raw mysqldump export), which
-- silently wiped every player's attributes on every re-run of
-- apply-tobin-schema.sh (the documented "apply new migrations to an
-- existing DB" step) -- discovered 2026-07-07 alongside the identical bug
-- in player_progress.sql. Fixed to the same safe pattern
-- player_inventory.sql/help_topic.sql/news.sql already use.
CREATE TABLE IF NOT EXISTS `player_attrs` (
  `player_id` bigint(20) unsigned NOT NULL,
  `strength` int(11) NOT NULL DEFAULT 120,
  `dexterity` int(11) NOT NULL DEFAULT 120,
  `constitution` int(11) NOT NULL DEFAULT 120,
  `intelligence` int(11) NOT NULL DEFAULT 120,
  `wisdom` int(11) NOT NULL DEFAULT 120,
  `charisma` int(11) NOT NULL DEFAULT 120,
  PRIMARY KEY (`player_id`),
  CONSTRAINT `fk_player_attrs_player_id` FOREIGN KEY (`player_id`) REFERENCES `player` (`id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;
