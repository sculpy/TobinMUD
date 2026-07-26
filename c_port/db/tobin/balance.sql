-- Gamewide class/race balance modifiers (db/tobin/balance.sql).
-- Tobin-specific (not part of the upstream seed). CREATE TABLE IF NOT
-- EXISTS + INSERT IGNORE, same idempotent-safe pattern as
-- player_attrs.sql/player_progress.sql -- safe to re-run.
--
-- User 2026-07-12: "a balance command (60) where you take args:
-- balance <class|race> that is menu driven to adjust balance
-- numbers/modifiers that will apply gamewide to the class or race you
-- just balanced." Every row starts at the neutral values (1.0/1.0/0/0)
-- -- an untouched class/race behaves exactly as it did before this
-- feature existed. See balance.h/balance.c for how these are cached
-- in memory and applied (being_calc_max_hp, combat_strike,
-- being_total_ac), and cmd_balance.c/descriptor.c's CONN_BALANCE_*
-- state machine for the in-game editor (60+, `balance class|race`).
CREATE TABLE IF NOT EXISTS `class_balance` (
  `class` tinyint(4) NOT NULL,
  `hp_mult` float NOT NULL DEFAULT 1.0,
  `dmg_mult` float NOT NULL DEFAULT 1.0,
  `tohit_mod` int(11) NOT NULL DEFAULT 0,
  `ac_mod` int(11) NOT NULL DEFAULT 0,
  PRIMARY KEY (`class`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

CREATE TABLE IF NOT EXISTS `race_balance` (
  `race` tinyint(4) NOT NULL,
  `hp_mult` float NOT NULL DEFAULT 1.0,
  `dmg_mult` float NOT NULL DEFAULT 1.0,
  `tohit_mod` int(11) NOT NULL DEFAULT 0,
  `ac_mod` int(11) NOT NULL DEFAULT 0,
  PRIMARY KEY (`race`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

-- being.h's player_class_t/player_race_t enum order: 0-5 for each.
INSERT IGNORE INTO `class_balance` (`class`) VALUES (0),(1),(2),(3),(4),(5);
INSERT IGNORE INTO `race_balance` (`race`) VALUES (0),(1),(2),(3),(4),(5);
