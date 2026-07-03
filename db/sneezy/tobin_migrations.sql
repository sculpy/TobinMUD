-- Idempotent schema migrations for existing Tobin databases (Session 21+).
-- Fresh seeds get these columns from the base CREATE files; this file
-- brings already-seeded databases up to date. Safe to re-run any time
-- (IF NOT EXISTS throughout). Load alongside help_topic.sql on deploys:
--     mariadb sneezy < db/sneezy/tobin_migrations.sql

-- Mortal/immortal toggle: the suspended TRUE level (0 = not suspended).
ALTER TABLE `player_progress`
  ADD COLUMN IF NOT EXISTS `true_level` int(11) NOT NULL DEFAULT 0;

-- Handedness: 1 = right (default), 0 = left.
ALTER TABLE `player`
  ADD COLUMN IF NOT EXISTS `handed` tinyint(4) NOT NULL DEFAULT 1;

-- Prompt customization bitmask (bit 0 = show HP).
ALTER TABLE `player`
  ADD COLUMN IF NOT EXISTS `prompt_flags` int(11) NOT NULL DEFAULT 0;
