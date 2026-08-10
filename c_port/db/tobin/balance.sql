-- Gamewide class/race balance modifiers (db/tobin/balance.sql).
-- Tobin-specific (not part of the upstream seed). CREATE TABLE IF NOT
-- EXISTS + ALTER ... ADD COLUMN IF NOT EXISTS + INSERT IGNORE, all
-- idempotent -- safe to re-run, and never clobbers values an immortal
-- has since tuned via `balance` (INSERT IGNORE no-ops on existing rows).
--
-- User 2026-07-12: "a balance command (60) ... menu driven to adjust
-- balance numbers/modifiers that will apply gamewide." Extended 2026-08-10
-- into the full PC-race perk system (docs/RACE_PERKS.md): mana/move/food/
-- drink mults, resistances, infravision, and a per-race talent, all
-- live-tunable through the same editor. Combat columns (hp/dmg/tohit/ac)
-- apply to classes too; the race-perk columns are neutral on class rows.
-- See balance.h/balance.c (cache), balance_repo.c (I/O), being.c/vitals.c
-- (apply sites), and descriptor.c CONN_BALANCE_* (the in-game editor).
CREATE TABLE IF NOT EXISTS `class_balance` (
  `class` tinyint(4) NOT NULL,
  `hp_mult` float NOT NULL DEFAULT 1.0,
  `dmg_mult` float NOT NULL DEFAULT 1.0,
  `tohit_mod` int(11) NOT NULL DEFAULT 0,
  `ac_mod` int(11) NOT NULL DEFAULT 0,
  `mana_mult` float NOT NULL DEFAULT 1.0,
  `move_mult` float NOT NULL DEFAULT 1.0,
  `food_mult` float NOT NULL DEFAULT 1.0,
  `drink_mult` float NOT NULL DEFAULT 1.0,
  `resist_poison` int(11) NOT NULL DEFAULT 0,
  `resist_charm` int(11) NOT NULL DEFAULT 0,
  `resist_sleep` int(11) NOT NULL DEFAULT 0,
  `resist_paralysis` int(11) NOT NULL DEFAULT 0,
  `resist_energy` int(11) NOT NULL DEFAULT 0,
  `resist_heat` int(11) NOT NULL DEFAULT 0,
  `resist_cold` int(11) NOT NULL DEFAULT 0,
  `infravision` tinyint(4) NOT NULL DEFAULT 0,
  `talent` tinyint(4) NOT NULL DEFAULT 0,
  PRIMARY KEY (`class`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

CREATE TABLE IF NOT EXISTS `race_balance` (
  `race` tinyint(4) NOT NULL,
  `hp_mult` float NOT NULL DEFAULT 1.0,
  `dmg_mult` float NOT NULL DEFAULT 1.0,
  `tohit_mod` int(11) NOT NULL DEFAULT 0,
  `ac_mod` int(11) NOT NULL DEFAULT 0,
  `mana_mult` float NOT NULL DEFAULT 1.0,
  `move_mult` float NOT NULL DEFAULT 1.0,
  `food_mult` float NOT NULL DEFAULT 1.0,
  `drink_mult` float NOT NULL DEFAULT 1.0,
  `resist_poison` int(11) NOT NULL DEFAULT 0,
  `resist_charm` int(11) NOT NULL DEFAULT 0,
  `resist_sleep` int(11) NOT NULL DEFAULT 0,
  `resist_paralysis` int(11) NOT NULL DEFAULT 0,
  `resist_energy` int(11) NOT NULL DEFAULT 0,
  `resist_heat` int(11) NOT NULL DEFAULT 0,
  `resist_cold` int(11) NOT NULL DEFAULT 0,
  `infravision` tinyint(4) NOT NULL DEFAULT 0,
  `talent` tinyint(4) NOT NULL DEFAULT 0,
  PRIMARY KEY (`race`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

-- Migration for DBs created before the perk columns existed (2026-08-10).
ALTER TABLE `class_balance`
  ADD COLUMN IF NOT EXISTS `mana_mult` float NOT NULL DEFAULT 1.0,
  ADD COLUMN IF NOT EXISTS `move_mult` float NOT NULL DEFAULT 1.0,
  ADD COLUMN IF NOT EXISTS `food_mult` float NOT NULL DEFAULT 1.0,
  ADD COLUMN IF NOT EXISTS `drink_mult` float NOT NULL DEFAULT 1.0,
  ADD COLUMN IF NOT EXISTS `resist_poison` int(11) NOT NULL DEFAULT 0,
  ADD COLUMN IF NOT EXISTS `resist_charm` int(11) NOT NULL DEFAULT 0,
  ADD COLUMN IF NOT EXISTS `resist_sleep` int(11) NOT NULL DEFAULT 0,
  ADD COLUMN IF NOT EXISTS `resist_paralysis` int(11) NOT NULL DEFAULT 0,
  ADD COLUMN IF NOT EXISTS `resist_energy` int(11) NOT NULL DEFAULT 0,
  ADD COLUMN IF NOT EXISTS `resist_heat` int(11) NOT NULL DEFAULT 0,
  ADD COLUMN IF NOT EXISTS `resist_cold` int(11) NOT NULL DEFAULT 0,
  ADD COLUMN IF NOT EXISTS `infravision` tinyint(4) NOT NULL DEFAULT 0,
  ADD COLUMN IF NOT EXISTS `talent` tinyint(4) NOT NULL DEFAULT 0;
ALTER TABLE `race_balance`
  ADD COLUMN IF NOT EXISTS `mana_mult` float NOT NULL DEFAULT 1.0,
  ADD COLUMN IF NOT EXISTS `move_mult` float NOT NULL DEFAULT 1.0,
  ADD COLUMN IF NOT EXISTS `food_mult` float NOT NULL DEFAULT 1.0,
  ADD COLUMN IF NOT EXISTS `drink_mult` float NOT NULL DEFAULT 1.0,
  ADD COLUMN IF NOT EXISTS `resist_poison` int(11) NOT NULL DEFAULT 0,
  ADD COLUMN IF NOT EXISTS `resist_charm` int(11) NOT NULL DEFAULT 0,
  ADD COLUMN IF NOT EXISTS `resist_sleep` int(11) NOT NULL DEFAULT 0,
  ADD COLUMN IF NOT EXISTS `resist_paralysis` int(11) NOT NULL DEFAULT 0,
  ADD COLUMN IF NOT EXISTS `resist_energy` int(11) NOT NULL DEFAULT 0,
  ADD COLUMN IF NOT EXISTS `resist_heat` int(11) NOT NULL DEFAULT 0,
  ADD COLUMN IF NOT EXISTS `resist_cold` int(11) NOT NULL DEFAULT 0,
  ADD COLUMN IF NOT EXISTS `infravision` tinyint(4) NOT NULL DEFAULT 0,
  ADD COLUMN IF NOT EXISTS `talent` tinyint(4) NOT NULL DEFAULT 0;

-- Classes: neutral rows (their combat balance is tuned in-game, not seeded).
INSERT IGNORE INTO `class_balance` (`class`) VALUES (0),(1),(2),(3),(4),(5);

-- Races: seeded PC-race perks, derived+scaled from the SneezyMUD RACE_*
-- tables (docs/RACE_PERKS.md). INSERT IGNORE => these land only on a fresh
-- install; an existing DB keeps whatever an immortal has tuned. Columns:
-- race, hp, dmg, tohit, ac, mana, move, food, drink, rPoison, rCharm,
-- rSleep, rParalysis, rEnergy, rHeat, rCold, infravision, talent.
INSERT IGNORE INTO `race_balance`
  (`race`,`hp_mult`,`dmg_mult`,`tohit_mod`,`ac_mod`,`mana_mult`,`move_mult`,`food_mult`,`drink_mult`,`resist_poison`,`resist_charm`,`resist_sleep`,`resist_paralysis`,`resist_energy`,`resist_heat`,`resist_cold`,`infravision`,`talent`)
VALUES
  (0, 1.00, 1.0, 0, 0, 1.00, 1.00, 1.00, 1.00,  0,  0,  0,  0,  0,  0,  0, 0, 1),  -- Human: adaptable
  (1, 1.00, 1.0, 0, 0, 1.05, 1.13, 1.50, 0.50,  0, 33, 33, 20,  0,  0,  0, 0, 0),  -- Elf
  (2, 1.10, 1.0, 0, 0, 1.00, 1.20, 1.25, 0.75, 17,  0,  0,  0,  0, 13, 13, 0, 2),  -- Ogre: brawler
  (3, 1.03, 1.0, 0, 0, 0.95, 0.90, 0.75, 1.25, 20, 38, 38,  0,  5,  0,  0, 1, 0),  -- Dwarf
  (4, 1.00, 1.0, 0, 0, 1.00, 1.20, 0.50, 1.30, 20,  0,  0,  0,  0,  0,  0, 1, 3),  -- Hobbit: woodland stealth
  (5, 1.00, 1.0, 0, 0, 1.10, 0.83, 0.80, 1.20,  0,  0,  0,  0, 15,  0,  0, 1, 4);  -- Gnome: detect magic
