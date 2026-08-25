-- Dragon ride system (user, 2026-08-25: "implement a dragon ride system
-- to take players from one area to another distant area for a fee").
-- New Tobin feature, no SneezyMUD precedent -- see cmd_fly.c's doc
-- comment and STATUS.md's decisions table.
--
-- Two new "dragon roost" rooms, vnums 7900/7901, in zone 63 ("Permanent
-- General Purpose (Single rooms/tiny zones)", vnum range 7800-7999,
-- otherwise unused above 7870 -- confirmed empty before picking these).
-- Each roost has a single ordinary `up` exit down to a real, existing,
-- already-seeded hub room so it's reachable on foot; the two roosts are
-- deliberately NOT connected to each other by a walkable exit -- `fly`
-- (cmd_fly.c, dragon_route_repo.c) is the only way between them.
--   7900 "A Dragon Roost Above Market Square" <- up from room 238
--        (Market Square, Tobin City Roads); down returns to 238.
--   7901 "A Dragon Roost Above the Araxus Walls" <- up from room 1216
--        (Outside the Town Walls, near Araxus's main gatekeep); down
--        returns to 1216.
-- A flavor-only dragon-keeper mob (not mechanically load-bearing --
-- `fly` doesn't check for it) sits in each roost. Fee: 1500 gold each
-- way, anchored above cmd_shop.c's SPEC_TICKET_GUY flat fee (1000) as a
-- longer, more strenuous haul.
--
-- Idempotent: every INSERT below is ON DUPLICATE KEY UPDATE, safe to
-- re-run (apply-tobin-schema.sh always re-applies every file).

CREATE TABLE IF NOT EXISTS `dragon_route` (
  `from_room` int(11) NOT NULL,
  `to_room` int(11) NOT NULL,
  `dest_name` varchar(64) NOT NULL,
  `fee` int(11) NOT NULL,
  PRIMARY KEY (`from_room`,`to_room`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT INTO `room` (`vnum`,`x`,`y`,`z`,`name`,`description`,`zone`,`room_flag`,`sector`,`teletime`,`teletarg`,`telelook`,`river_speed`,`river_dir`,`capacity`,`height`,`spec`,`mine_trapped`) VALUES
(7900, 6099991, -5, 1, 'A Dragon Roost Above Market Square',
 '  A weathered wooden platform juts out from a rocky spire high above\r\nMarket Square, lashed together with thick rope and iron bracing. The\r\nwind up here is sharp and constant, carrying the distant clamor of the\r\nmarket below. A huge, scorched perch dominates the center of the\r\nplatform, its claw marks gouged deep into the timber. A stair cut into\r\nthe rock leads back down.\r\n',
 63, 0, 29, 0, 0, 0, 0, 0, 0, -1, 0, 0)
ON DUPLICATE KEY UPDATE `name`=VALUES(`name`), `description`=VALUES(`description`);

INSERT INTO `room` (`vnum`,`x`,`y`,`z`,`name`,`description`,`zone`,`room_flag`,`sector`,`teletime`,`teletarg`,`telelook`,`river_speed`,`river_dir`,`capacity`,`height`,`spec`,`mine_trapped`) VALUES
(7901, 6099891, -28, 1, 'A Dragon Roost Above the Araxus Walls',
 '  A crude stone landing has been hewn into the cliff face overlooking\r\nAraxus''s outer walls. Chains and heavy iron rings are bolted into the\r\nrock, and the stone underfoot is scorched black in a wide ring. Far\r\nbelow, the town walls and the road beyond look small and distant. A\r\nnarrow switchback path leads back down to solid ground.\r\n',
 63, 0, 29, 0, 0, 0, 0, 0, 0, -1, 0, 0)
ON DUPLICATE KEY UPDATE `name`=VALUES(`name`), `description`=VALUES(`description`);

INSERT INTO `roomexit` (`vnum`,`direction`,`name`,`description`,`type`,`condition_flag`,`lock_difficulty`,`weight`,`key_num`,`destination`) VALUES
(238, 4, '', '', 0, 0, -1, -1, -1, 7900)
ON DUPLICATE KEY UPDATE `destination`=VALUES(`destination`);
INSERT INTO `roomexit` (`vnum`,`direction`,`name`,`description`,`type`,`condition_flag`,`lock_difficulty`,`weight`,`key_num`,`destination`) VALUES
(7900, 5, '', '', 0, 0, -1, -1, -1, 238)
ON DUPLICATE KEY UPDATE `destination`=VALUES(`destination`);

INSERT INTO `roomexit` (`vnum`,`direction`,`name`,`description`,`type`,`condition_flag`,`lock_difficulty`,`weight`,`key_num`,`destination`) VALUES
(1216, 4, '', '', 0, 0, -1, -1, -1, 7901)
ON DUPLICATE KEY UPDATE `destination`=VALUES(`destination`);
INSERT INTO `roomexit` (`vnum`,`direction`,`name`,`description`,`type`,`condition_flag`,`lock_difficulty`,`weight`,`key_num`,`destination`) VALUES
(7901, 5, '', '', 0, 0, -1, -1, -1, 1216)
ON DUPLICATE KEY UPDATE `destination`=VALUES(`destination`);

INSERT INTO `mob` (`vnum`,`name`,`short_desc`,`long_desc`,`description`,`actions`,`affects`,`faction`,`fact_perc`,`letter`,`attacks`,`class`,`level`,`tohit`,`ac`,`hpbonus`,`damage_level`,`damage_precision`,`gold`,`race`,`weight`,`height`,`str`,`bra`,`con`,`dex`,`agi`,`intel`,`wis`,`foc`,`per`,`cha`,`kar`,`spe`,`pos`,`def_position`,`sex`,`spec_proc`,`skin`,`vision`,`can_be_seen`,`max_exist`,`local_sound`,`adjacent_sound`,`align`,`body_type`) VALUES
(7900, 'dragon keeper Keirath', 'Keirath the dragon-keeper', 'Keirath the dragon-keeper tends the roost here, watching the sky.',
 '  Keirath is a wiry, weather-beaten man in thick leather gauntlets that\r\nreach past his elbows, scarred from old talon-marks. He watches the sky\r\nconstantly, as if expecting company at any moment.\r\n',
 2, 0, 0, 50, 'A', 1.0, 4, 20, 0, 15.0, 15.0, 15.0, 15, 0, 1, 160, 70, 0,0,0,0,0,0,0,0,0,0,0,0, 9, 9, 1, 0, 68, 0, 0, 1, '', '', 0, 1)
ON DUPLICATE KEY UPDATE `name`=VALUES(`name`), `short_desc`=VALUES(`short_desc`), `long_desc`=VALUES(`long_desc`), `description`=VALUES(`description`);

INSERT INTO `mob` (`vnum`,`name`,`short_desc`,`long_desc`,`description`,`actions`,`affects`,`faction`,`fact_perc`,`letter`,`attacks`,`class`,`level`,`tohit`,`ac`,`hpbonus`,`damage_level`,`damage_precision`,`gold`,`race`,`weight`,`height`,`str`,`bra`,`con`,`dex`,`agi`,`intel`,`wis`,`foc`,`per`,`cha`,`kar`,`spe`,`pos`,`def_position`,`sex`,`spec_proc`,`skin`,`vision`,`can_be_seen`,`max_exist`,`local_sound`,`adjacent_sound`,`align`,`body_type`) VALUES
(7901, 'dragon keeper Sorha', 'Sorha the dragon-keeper', 'Sorha the dragon-keeper stands watch over the roost, scanning the horizon.',
 '  Sorha is a tall, sun-browned woman with a long braid and the same\r\nscarred leather gauntlets worn by every dragon-keeper. A battered horn\r\nhangs at her belt, ready to call a dragon down out of the sky.\r\n',
 2, 0, 0, 50, 'A', 1.0, 4, 20, 0, 15.0, 15.0, 15.0, 15, 0, 1, 150, 68, 0,0,0,0,0,0,0,0,0,0,0,0, 9, 9, 0, 0, 68, 0, 0, 1, '', '', 0, 1)
ON DUPLICATE KEY UPDATE `name`=VALUES(`name`), `short_desc`=VALUES(`short_desc`), `long_desc`=VALUES(`long_desc`), `description`=VALUES(`description`);

INSERT INTO `dragon_route` (`from_room`,`to_room`,`dest_name`,`fee`) VALUES
(7900, 7901, 'Araxus', 1500)
ON DUPLICATE KEY UPDATE `dest_name`=VALUES(`dest_name`), `fee`=VALUES(`fee`);
INSERT INTO `dragon_route` (`from_room`,`to_room`,`dest_name`,`fee`) VALUES
(7901, 7900, 'Tobin City', 1500)
ON DUPLICATE KEY UPDATE `dest_name`=VALUES(`dest_name`), `fee`=VALUES(`fee`);


-- Follow-up (user, 2026-08-25: "there must be a series of rooms to go
-- through for flavor, a flight should take between 10-15 seconds to
-- complete"). Five shared "in the sky" waypoint rooms, vnums
-- 7902-7906, same zone-63 block as the roosts -- every `fly` route now
-- passes through this same sequence, in order 7902 -> 7903 -> 7904 ->
-- 7906 -> 7905 (fly.c's FLY_WAYPOINTS; 7906 was added after 7905 to
-- widen a too-tight 4-leg landing window to a reliable 12-15s one, so
-- the vnum order doesn't match travel order), rather than teleporting
-- instantly. No walk-in exits: only fly_start()/fly_tick_run() ever
-- place a character here.
--
-- Idempotent: every INSERT below is ON DUPLICATE KEY UPDATE, safe to
-- re-run (apply-tobin-schema.sh always re-applies every file).

INSERT INTO `room` (`vnum`,`x`,`y`,`z`,`name`,`description`,`zone`,`room_flag`,`sector`,`teletime`,`teletarg`,`telelook`,`river_speed`,`river_dir`,`capacity`,`height`,`spec`,`mine_trapped`) VALUES
(7902, 6099993, -5, 2, 'High Above the Clouds',
 '  You are far above the world now, riding a dragon through open sky. A\r\nsea of white cloud stretches out beneath you, broken here and there by\r\nglimpses of the land far below. The air is thin and bitterly cold, and\r\nthe wind tears at your clothes.\r\n',
 63, 0, 29, 0, 0, 0, 0, 0, 0, -1, 0, 0)
ON DUPLICATE KEY UPDATE `name`=VALUES(`name`), `description`=VALUES(`description`);

INSERT INTO `room` (`vnum`,`x`,`y`,`z`,`name`,`description`,`zone`,`room_flag`,`sector`,`teletime`,`teletarg`,`telelook`,`river_speed`,`river_dir`,`capacity`,`height`,`spec`,`mine_trapped`) VALUES
(7903, 6099995, -5, 3, 'Over Rolling Hills',
 '  The dragon banks and levels out, carrying you over a broad stretch of\r\ncountryside. Rolling hills and dark forest pass beneath you in a\r\nblur, and a distant thread of river catches the light. The dragon''s\r\nwingbeats settle into a long, steady rhythm.\r\n',
 63, 0, 29, 0, 0, 0, 0, 0, 0, -1, 0, 0)
ON DUPLICATE KEY UPDATE `name`=VALUES(`name`), `description`=VALUES(`description`);

INSERT INTO `room` (`vnum`,`x`,`y`,`z`,`name`,`description`,`zone`,`room_flag`,`sector`,`teletime`,`teletarg`,`telelook`,`river_speed`,`river_dir`,`capacity`,`height`,`spec`,`mine_trapped`) VALUES
(7904, 6099997, -5, 4, 'Riding a Mountain Wind',
 '  Jagged peaks rise up on either side as the dragon threads its way\r\nthrough a high mountain pass, riding the currents that pour off the\r\nrock faces. Snow streams off distant summits in long white banners,\r\nand your ears pop with the changing air.\r\n',
 63, 0, 29, 0, 0, 0, 0, 0, 0, -1, 0, 0)
ON DUPLICATE KEY UPDATE `name`=VALUES(`name`), `description`=VALUES(`description`);

INSERT INTO `room` (`vnum`,`x`,`y`,`z`,`name`,`description`,`zone`,`room_flag`,`sector`,`teletime`,`teletarg`,`telelook`,`river_speed`,`river_dir`,`capacity`,`height`,`spec`,`mine_trapped`) VALUES
(7905, 6099999, -5, 5, 'Descending Toward the Horizon',
 '  The dragon tips into a long, gentle dive, wings half-folded, and a\r\nfamiliar stretch of land begins to resolve out of the haze ahead. Smoke\r\nrises from distant chimneys, and rooftops sharpen into focus as the\r\nground rushes closer.\r\n',
 63, 0, 29, 0, 0, 0, 0, 0, 0, -1, 0, 0)
ON DUPLICATE KEY UPDATE `name`=VALUES(`name`), `description`=VALUES(`description`);

INSERT INTO `room` (`vnum`,`x`,`y`,`z`,`name`,`description`,`zone`,`room_flag`,`sector`,`teletime`,`teletarg`,`telelook`,`river_speed`,`river_dir`,`capacity`,`height`,`spec`,`mine_trapped`) VALUES
(7906, 6099998, -5, 6, 'Crossing an Open Valley',
 '  The mountains fall away behind you, and the dragon glides out over a\r\nwide, open valley, wings barely moving. Patchwork fields and winding\r\nfences stitch the ground together far below, and the air grows warmer\r\nand thicker the lower you drift.\r\n',
 63, 0, 29, 0, 0, 0, 0, 0, 0, -1, 0, 0)
ON DUPLICATE KEY UPDATE `name`=VALUES(`name`), `description`=VALUES(`description`);
