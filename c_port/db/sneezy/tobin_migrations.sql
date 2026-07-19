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

-- `rent` (user 2026-07-12: "make rent work from sneezy"). Unix timestamp
-- of the last `rent`, 0 if not currently rented out -- player_load()
-- heals a flat rate for the real time elapsed since this was set, then
-- clears it ("regenerating HP while rented out" per Sneezy's own rent
-- help text).
ALTER TABLE `player_progress`
  ADD COLUMN IF NOT EXISTS `rented_at` int(11) NOT NULL DEFAULT 0;

-- Practice-system redesign (user 2026-07-13, design locked; guildmaster
-- decisions resolved 2026-07-17 -- see TODO.md). Adds a THIRD discipline,
-- Combat (SKILL_TIER_COMBAT), alongside Basic and Advanced, plus a pool of
-- spendable practice points. On level-up a player earns
--   random(6,8) + round(wisdom_bonus * wisdom_practice_modifier)
-- points (wisdom_bonus = floor((wisdom - ATTR_BASE) / 10)); each point
-- spent at a guildmaster raises one discipline by a random 1-2%. Advanced
-- unlocks only once Basic AND Combat both reach 100%. The three
-- guildmaster tiers are told apart by mob.level: 51 = Basic, 80 = Combat,
-- 100 = Advanced (no new world content -- the 80-tier already exists,
-- one per class, each already placed; see cmd_practice.c).
ALTER TABLE `player_progress`
  ADD COLUMN IF NOT EXISTS `combat_disc_pct` int(11) NOT NULL DEFAULT 0;
ALTER TABLE `player_progress`
  ADD COLUMN IF NOT EXISTS `practice_points` int(11) NOT NULL DEFAULT 0;

-- Scales the wisdom contribution to per-level practice-point awards.
-- Default '1'. Viewed/changed live with `balance wisdom [<value>]`.
INSERT INTO `game_config` (`name`, `value`) VALUES ('wisdom_practice_modifier', '1')
  ON DUPLICATE KEY UPDATE `name` = `name`;

-- Per-skill/spell proficiency (user 2026-07-17: "the actual gain in
-- proficiency should be gained as in sneezy" -- learn-by-doing, separate
-- from the coarse *_disc_pct access gate above). One row per player per
-- skill they've actually attempted; a skill never attempted has no row
-- (treated as 0%, i.e. "not yet learned by doing"). `last_gain_at` is a
-- unix timestamp, used as a simple anti-grind cooldown (skill.c). See
-- skill_repo.h/skill.c for the read/write API and the gain formula.
CREATE TABLE IF NOT EXISTS `player_skill` (
  `player_id` bigint(20) unsigned NOT NULL,
  `skill_name` varchar(64) NOT NULL,
  `pct` int(11) NOT NULL DEFAULT 0,
  `last_gain_at` int(11) NOT NULL DEFAULT 0,
  PRIMARY KEY (`player_id`, `skill_name`)
);

-- Player-defined command aliases (`alias` command, cmd_alias.c). Stored on
-- the ACCOUNT, not the character, and shared across every character on it
-- -- but scoped by tier: an immortal alias only expands for an immortal
-- caller, a mortal alias only for a mortal one, so the same short name
-- (e.g. "k") can mean something different depending which side of the
-- account is playing. `tier` is the literal string 'mortal'/'immortal'
-- (matches being_is_immortal()'s boolean, kept as text for readability in
-- a raw SELECT). PRIMARY KEY on (account_id, tier, name) lets a mortal and
-- immortal alias share the same name independently.
CREATE TABLE IF NOT EXISTS `account_alias` (
  `account_id` bigint(20) unsigned NOT NULL,
  `tier` varchar(9) NOT NULL,
  `name` varchar(32) NOT NULL,
  `expansion` varchar(255) NOT NULL,
  PRIMARY KEY (`account_id`, `tier`, `name`)
);

-- Money system (user 2026-07-17: "implement money and shops"). GOLD-COIN-
-- ONLY currency (no talens/commodities/shards, per the Shops TODO entry's
-- own scoped-down spec) -- a plain wallet stat on progress_t, same shape
-- as practice_points, not a pickupable/lootable object. Mobs hand it to
-- their killer directly on defeat (combat.c); shops (below) are the sink.
ALTER TABLE `player_progress`
  ADD COLUMN IF NOT EXISTS `gold` int(11) NOT NULL DEFAULT 0;

-- Components + commodities (user 2026-07-18: "implement components and
-- commodities again from sneezy"). No new code needed -- ITEM_COMPONENT/
-- RAW_MATERIAL/RAW_ORGANIC (obj.type 30/42/50) already collapse into a
-- working generic category (OBJ_CAT_OTHER, obj.c) and 380 real objects of
-- these types already exist in the upstream `obj` seed, with real
-- shoptype rows declaring which shops buy each category. The gap: every
-- one of their upstream zone_reset ground/mob placements sits in a zone
-- that's disabled or missing entirely in this DB, so none were actually
-- reachable, AND the shops flavored as component/commodity dealers
-- (Camron's Components, the "well-stocked commodities shop of
-- Brightmoon", etc.) had empty `shopproducing` catalogs -- nothing to
-- buy. This stocks those already-real, already-enabled shops with a
-- curated sample of the upstream game's own component/raw-material/
-- organic object vnums (no fabricated content), so `list`/`buy` there
-- actually offers something, and any shop that already declares it buys
-- that category (shoptype) completes the loop on the sell side. The full
-- upstream merge-stack/decay/alchemy component system (obj_component.cc)
-- remains out of scope -- these are plain, individually-vnum'd objects,
-- same as everything else Tobin sells.
INSERT INTO `shopproducing` (`shop_nr`, `producing`) VALUES
  -- Camron's Components (shop 12, room 554, Market Place) -- spell
  -- reagents, ITEM_COMPONENT (type 30).
  (12, 202), (12, 205), (12, 208), (12, 209), (12, 213), (12, 218), (12, 225), (12, 232),
  -- The commodities shop of Brightmoon (shop 57, room 1393) -- refined
  -- metal bars/rods/ingots, ITEM_RAW_MATERIAL (type 42).
  (57, 50), (57, 51), (57, 52), (57, 53),
  -- Logrus commodities (shop 58, room 3709) -- more of the same catalog.
  (58, 54), (58, 55), (58, 56), (58, 60),
  -- Eldon's shop, Xanesla (shop 238, room 6420) -- more of the same catalog.
  (238, 57), (238, 58), (238, 61), (238, 64),
  -- Amber commodities (shop 56, room 8734) -- more of the same catalog.
  (56, 59), (56, 62), (56, 63), (56, 65),
  -- Tuvar's organic commodities (shop 105, room 7805) -- his own room
  -- description says "hides, skins, herbal ingredients" verbatim;
  -- ITEM_RAW_ORGANIC (type 50) animal hides.
  (105, 2400), (105, 2402), (105, 2403), (105, 2405), (105, 2406), (105, 2408), (105, 2420)
ON DUPLICATE KEY UPDATE `shop_nr` = `shop_nr`;

-- Component charges / symbol strength (user 2026-07-18: "how long does
-- each component last? should be getting 10 casts out of each component
-- and the symbols should decay as in sneezy") -- a follow-up to the
-- "Components + commodities" migration above, which explicitly left the
-- upstream charges/decay system out of scope. Seeds every already-real
-- component/symbol row (obj.name LIKE keyword match, same convention
-- cmd_cast.c/cmd_pray.c's find_keyword_item() already uses) to 10/10 --
-- see obj.h's val[] doc comment for what val0/val1 mean here and
-- cmd_cast.c/cmd_pray.c/cmd_continue.c for how they're spent.
--
-- UNCONDITIONAL, not guarded on val0=0 AND val1=0: many holy symbol rows
-- turned out to already carry huge leftover val0/val1 from the upstream
-- import (up to 1.8 MILLION, val2 uniformly -1, val3 uniformly 0) --
-- some other original field entirely (upstream symbols' real
-- strength/max_strength ARE this large under the original's level-
-- squared `sym_stress` cost, misc/discipline.cc, but Tobin has no
-- per-symbol "level" rating to make that formula meaningful, and
-- inheriting it as-is would flatly contradict "10 casts" above). No
-- meaningful Tobin state exists yet for this category to protect --
-- component/symbol val0/val1 were unused/decorative before this session
-- (obj.h's val[] doc) -- so this is a one-time reset, not an ongoing
-- guard; a LATER manual edit (via `redit`/a builder) is expected to be
-- respected the normal way from here on, same as everywhere else.
UPDATE `obj` SET val0=10, val1=10 WHERE `name` LIKE '%component%';
UPDATE `obj` SET val0=10, val1=10 WHERE `name` LIKE '%symbol%';

-- Unseen-news bookmark ("News follow-ups", user 2026-07-17 batch): highest
-- news.id this player has already caught up on. Bumped to news_repo_max_id()
-- when they run `news`; compared against it at login to show a one-line
-- "there's new news" notice (no count/id ever shown -- house rule, see
-- news.sql). int, not bigint: matches news.id's own column type.
ALTER TABLE `player`
  ADD COLUMN IF NOT EXISTS `news_last_seen_id` int(11) NOT NULL DEFAULT 0;

-- Ignore lists (Sneezy → Tobin feature audit, "Ignore lists" -- player-
-- maintained block list for unwanted communication). Scoped down from the
-- original's ignoreList (which also supports blocking by descriptor or
-- whole account, and reaches say/shout/grouptell/emote/socials -- none of
-- which apply here, or don't exist yet) to what Tobin actually has: a
-- flat per-character name list, checked by `tell`/`whisper` only (see
-- ignore_repo.h). Blocked by NAME, not player_id -- matches how every
-- other cross-player lookup in this codebase already works (tell/whisper/
-- goto all resolve by name), and survives the target deleting and
-- recreating a same-named character the same way a real ignore would be
-- expected to.
CREATE TABLE IF NOT EXISTS `player_ignore` (
  `player_id` bigint(20) unsigned NOT NULL,
  `ignored_name` varchar(32) NOT NULL,
  PRIMARY KEY (`player_id`, `ignored_name`)
);

-- Vital statistics (Sneezy → Tobin feature audit, "Vital statistics
-- (hunger/thirst/age)"). Rescaled to a plain 0-100 (-1 = immortal-immune)
-- instead of the original's cryptic 0-24 condTypeT units -- see being.h's
-- progress_t field comment. 100 = the being_create_pc() default for a
-- genuinely NEW character; existing rows predating this migration also
-- default to 100 ("not already starving") rather than 0, matching the
-- "benefit of the doubt" precedent of every other retrofitted stat column
-- in this file (gold, practice_points, ...).
ALTER TABLE `player_progress`
  ADD COLUMN IF NOT EXISTS `hunger` int(11) NOT NULL DEFAULT 100;
ALTER TABLE `player_progress`
  ADD COLUMN IF NOT EXISTS `thirst` int(11) NOT NULL DEFAULT 100;

-- Age: track + display only, no stat effects (user 2026-07-19,
-- AskUserQuestion -- see progress_t's doc comment for why the original's
-- full age-based stat-curve system was cut). Unix timestamp of character
-- creation, set once and never touched again. Existing rows predating this
-- migration have no real creation moment to recover, so they're backfilled
-- to "now" (birth_time=0 is the ALTER's own column default, used here as
-- the one-time "not yet backfilled" marker) rather than showing an
-- absurd/impossible epoch-zero age.
ALTER TABLE `player_progress`
  ADD COLUMN IF NOT EXISTS `birth_time` int(11) NOT NULL DEFAULT 0;
UPDATE `player_progress` SET `birth_time` = UNIX_TIMESTAMP() WHERE `birth_time` = 0;
