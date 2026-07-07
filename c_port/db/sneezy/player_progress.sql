-- Persisted level/experience/HP for a player (db/sneezy/player_progress.sql).
-- Tobin-specific (not part of the upstream seed).
--
-- CREATE TABLE IF NOT EXISTS -- NOT an unconditional DROP+CREATE. This file
-- used to open with `DROP TABLE IF EXISTS` (a raw mysqldump export), which
-- silently wiped every player's level/xp/hp on every re-run of
-- apply-tobin-schema.sh (the documented "apply new migrations to an
-- existing DB" step) -- discovered 2026-07-07 when a real character's level
-- reverted after two same-day redeploys. Fixed to the same safe pattern
-- player_attrs.sql/player_inventory.sql/help_topic.sql/news.sql already use.
CREATE TABLE IF NOT EXISTS `player_progress` (
  `player_id` bigint(20) unsigned NOT NULL,
  `level` int(11) NOT NULL DEFAULT 1,
  `experience` bigint(20) NOT NULL DEFAULT 0,
  `hp` int(11) NOT NULL DEFAULT 20,
  `max_hp` int(11) NOT NULL DEFAULT 20,
  `true_level` int(11) NOT NULL DEFAULT 0,
  PRIMARY KEY (`player_id`),
  CONSTRAINT `fk_player_progress_player_id` FOREIGN KEY (`player_id`) REFERENCES `player` (`id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;
