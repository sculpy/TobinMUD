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

-- Custom immortal move messages (2026-07-11, user: "immorts should be able
-- to set their own enter or leave messages. Like Jesus drags his cross in
-- from the east." -- named "poofin"/"poofout" originally, renamed to
-- "bamfin"/"bamfout" per user request the same session). NULL = use the
-- default "exits to the <dir>"/"has arrived" wording. See `bamfin`/
-- `bamfout` commands (cmd_bamf.c).
ALTER TABLE `player`
  ADD COLUMN IF NOT EXISTS `bamfin` varchar(96) DEFAULT NULL;
ALTER TABLE `player`
  ADD COLUMN IF NOT EXISTS `bamfout` varchar(96) DEFAULT NULL;

-- "bamfin"/"bamfout" freed up for `goto`'s teleport messages (2026-07-11,
-- user: "bamfin|out should modify goto messaging and the current
-- bamfin|out should be called something else following the in|out
-- syntax") -- the WALKING move-message feature above moves to new
-- `poofin`/`poofout` columns (its ORIGINAL name, before the same-session
-- rename quoted above), carrying over anything already set so no existing
-- immortal's custom move message is lost. The `bamfin`/`bamfout` columns
-- themselves are then cleared so `goto` starts with the default
-- puff-of-smoke wording rather than inheriting stale move-message text
-- under its new meaning. See `poofin`/`poofout` (cmd_poof.c) and the
-- (fresh) `bamfin`/`bamfout` (cmd_bamf.c) commands.
ALTER TABLE `player`
  ADD COLUMN IF NOT EXISTS `poofin` varchar(96) DEFAULT NULL;
ALTER TABLE `player`
  ADD COLUMN IF NOT EXISTS `poofout` varchar(96) DEFAULT NULL;
UPDATE `player` SET `poofin` = `bamfin`, `poofout` = `bamfout`
  WHERE `bamfin` IS NOT NULL OR `bamfout` IS NOT NULL;
UPDATE `player` SET `bamfin` = NULL, `bamfout` = NULL;

-- `edbug <id> [note]` (TODO.md-planned): resolve a filed bug in place
-- instead of only being able to `delbug` (delete) it outright, so a
-- player can be told their report was actually fixed. NULL resolved_at =
-- still outstanding.
ALTER TABLE `bug`
  ADD COLUMN IF NOT EXISTS `resolved_at` timestamp NULL DEFAULT NULL;
ALTER TABLE `bug`
  ADD COLUMN IF NOT EXISTS `resolution` text DEFAULT NULL;

-- Classes + races (user 2026-07-11: "implement classes, 6 player classes:
-- mage, cleric, warrior, thief, druid, monk" / "implement races, 6 player
-- races: human, elf, ogre, dwarf, hobbit, gnome"). Chosen at character
-- creation (being.h's player_class_t/player_race_t enum order: 0-5).
ALTER TABLE `player`
  ADD COLUMN IF NOT EXISTS `class` tinyint(4) NOT NULL DEFAULT 0;
ALTER TABLE `player`
  ADD COLUMN IF NOT EXISTS `race` tinyint(4) NOT NULL DEFAULT 0;

-- Alignment now chosen at creation too (same message as classes/races,
-- user: "ask player to choose initial alignment"), instead of always
-- defaulting to neutral (0) until an immortal later sets it with `set`.
-- No new column needed -- player_progress.alignment already exists
-- (Session 43 continued); player_create() just stops hardcoding 0.

-- Mob alignment (user: "so good will attack evil and evil will attack
-- good randomly ... people who are neutral should be taunted by evil and
-- supported by good") -- -1 evil, 0 unaligned (existing ACT_AGGRESSIVE
-- behavior untouched), 1 good. Separate from player_progress.alignment's
-- -1000..1000 range -- mobs need only a coarse three-way tag, set by a
-- builder via `edit trigger`-adjacent tooling or a direct DB edit (no
-- in-game editor for it yet, same as several other mob prototype fields).
ALTER TABLE `mob`
  ADD COLUMN IF NOT EXISTS `align` tinyint(4) NOT NULL DEFAULT 0;

-- `practice` command + guildmaster-gated discipline percentages (user
-- 2026-07-12: "add the practice command so players have to visit a
-- guildmaster to gain skills based upon percentage of discipline
-- learned. cant get to advanced disc until basic disc is at least 95%
-- complete"). A player's Basic (SKILL_TIER_CLASS) and Advanced
-- (SKILL_TIER_ADVANCED) discipline are each a single 0-100 aggregate
-- percentage raised by `practice`ing at a guildmaster of the player's
-- own class -- not a per-skill percentage (see cmd_practice.c's header
-- comment for why this coarser v1 scope was chosen). 0% = none of that
-- tier's skills/spells are usable yet, regardless of level.
ALTER TABLE `player_progress`
  ADD COLUMN IF NOT EXISTS `basic_disc_pct` int(11) NOT NULL DEFAULT 0;
ALTER TABLE `player_progress`
  ADD COLUMN IF NOT EXISTS `advanced_disc_pct` int(11) NOT NULL DEFAULT 0;
