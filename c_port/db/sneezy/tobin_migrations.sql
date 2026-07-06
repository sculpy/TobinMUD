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

-- Gender (Session 23): 0 = neuter (default), 1 = male, 2 = female. Chosen at
-- character creation; drives pronouns (he/she/it, him/her/it, his/her/its).
ALTER TABLE `player`
  ADD COLUMN IF NOT EXISTS `gender` tinyint(4) NOT NULL DEFAULT 0;

-- Appearance (Session 23): a free-text self-description set at creation and
-- shown by `look <player>`/`score`. NULL = none set.
ALTER TABLE `player`
  ADD COLUMN IF NOT EXISTS `appearance` varchar(255) DEFAULT NULL;

-- Color preference (Session 23): 1 = ANSI color on (default), 0 = off. Asked
-- at account creation and remembered; the `color` command persists changes.
ALTER TABLE `account`
  ADD COLUMN IF NOT EXISTS `color_pref` tinyint(4) NOT NULL DEFAULT 1;

-- Player flags bitmask (Session 23): bit 0 = PLR_NEWBIE (on the newbie help
-- channel). Defaults to 1 so new players start on the channel; toggleable.
ALTER TABLE `player`
  ADD COLUMN IF NOT EXISTS `pflags` int(11) NOT NULL DEFAULT 1;
