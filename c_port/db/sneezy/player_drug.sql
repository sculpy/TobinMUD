-- Drug consumption tracking (Sneezy -> Tobin feature audit, "drug
-- tracking"). Tobin-specific (not part of the upstream seed). One row
-- per (player_id, drug_type) actually used -- see include/drug.h for
-- drug_type_t's real values (0=pipeweed, 1=opium, 2=pot, 3=frogslime).
-- Deliberately does NOT persist a session-only "current_consumed"
-- counter the way the real upstream does -- that field is the real
-- upstream's own documented bug (saved to DB, reloaded stale on
-- reconnect, double-counts from then on); only `total_consumed`
-- (lifetime) is tracked, which is all the addiction/withdrawal math
-- actually needs. CREATE TABLE IF NOT EXISTS, not an unconditional
-- DROP+CREATE, matching player_attrs.sql/player_progress.sql's own
-- fixed convention.
CREATE TABLE IF NOT EXISTS `player_drug` (
  `player_id` bigint(20) unsigned NOT NULL,
  `drug_type` int(11) NOT NULL,
  `first_use` int(11) NOT NULL DEFAULT 0,
  `last_use` int(11) NOT NULL DEFAULT 0,
  `total_consumed` int(11) NOT NULL DEFAULT 0,
  PRIMARY KEY (`player_id`, `drug_type`),
  CONSTRAINT `fk_player_drug_player_id` FOREIGN KEY (`player_id`) REFERENCES `player` (`id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;
