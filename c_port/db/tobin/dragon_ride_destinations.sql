-- Dragon ride destination expansion (user follow-up, 2026-08-25: "offer
-- other major areas to fly [travel] to" -- only Tobin City and Araxus
-- existed). Pure data on top of the existing dragon_route/cmd_travel.c
-- machinery -- no code change needed for routing itself. Four new
-- "dragon roost" rooms, vnums 7907-7910, same zone-63 block as the
-- original two (7900/7901), each with an ordinary `up` exit down to a
-- real, already-seeded hub room in a major, distinct area of the world
-- (confirmed each anchor room has no existing `up` exit before picking
-- it):
--   7907 "A Dragon Roost Above Amber Castle" <- up from room 3011
--        (Inside The South Gate, zone 29 "Minion - Amber Castle").
--   7908 "A Dragon Roost Above Logrus" <- up from room 3726
--        (Dread Square, zone 34 "Spawn - The Town of Logrus").
--   7909 "A Dragon Roost Above the Xanesla Coast" <- up from room 6314
--        (Before a Large Lighthouse, zone 54 "Bump - City of Xanesla").
--   7910 "A Dragon Roost Above the Obsidian Citadel" <- up from room
--        7119 (Inside the Bailey Gates, zone 57 "Mithros - Obsidian
--        Citadel/Town").
-- Each gets its own flavor-only dragon-keeper mob, same as the original
-- two (not mechanically load-bearing).
--
-- Routes are hub-and-spoke through Tobin City (room 7900), matching how
-- a capital city's own dragon roost would realistically be the busiest
-- one -- Tobin City <-> each of the four new roosts, bidirectional, same
-- flat 1500 gold fee as the existing Tobin<->Araxus route. Araxus is NOT
-- directly connected to the four new destinations (only to Tobin City,
-- as before) -- a traveler from Araxus to, say, Amber connects through
-- Tobin City, same as a real hub-and-spoke network. Also two new shared
-- "in the sky" waypoint rooms (7911/7912, fly.c's own 2026-08-25
-- "extend the flight by two more legs" follow-up) -- see fly.c for how
-- they're wired into the flight sequence.
--
-- Idempotent: every INSERT below is ON DUPLICATE KEY UPDATE, safe to
-- re-run (apply-tobin-schema.sh always re-applies every file).

INSERT INTO `room` (`vnum`,`x`,`y`,`z`,`name`,`description`,`zone`,`room_flag`,`sector`,`teletime`,`teletarg`,`telelook`,`river_speed`,`river_dir`,`capacity`,`height`,`spec`,`mine_trapped`) VALUES
(7907, 6099980, -5, 7, 'A Dragon Roost Above Amber Castle',
 '  A weathered stone ledge clings to a spire overlooking Amber Castle,\r\nreachable only by a narrow, wind-scoured stair. Banners from the\r\ncastle below snap in the updraft, and the whole of the castle grounds\r\nspreads out beneath the platform like a painted map.\r\n',
 63, 0, 29, 0, 0, 0, 0, 0, 0, -1, 0, 0)
ON DUPLICATE KEY UPDATE `name`=VALUES(`name`), `description`=VALUES(`description`);

INSERT INTO `room` (`vnum`,`x`,`y`,`z`,`name`,`description`,`zone`,`room_flag`,`sector`,`teletime`,`teletarg`,`telelook`,`river_speed`,`river_dir`,`capacity`,`height`,`spec`,`mine_trapped`) VALUES
(7908, 6099982, -5, 8, 'A Dragon Roost Above Logrus',
 '  A ring of scorched cobblestones marks the dragon roost high above\r\nthe rooftops of Logrus. Smoke from the town''s chimneys drifts past at\r\neye level, and the murmur of the plazas below carries faintly on the\r\nwind.\r\n',
 63, 0, 29, 0, 0, 0, 0, 0, 0, -1, 0, 0)
ON DUPLICATE KEY UPDATE `name`=VALUES(`name`), `description`=VALUES(`description`);

INSERT INTO `room` (`vnum`,`x`,`y`,`z`,`name`,`description`,`zone`,`room_flag`,`sector`,`teletime`,`teletarg`,`telelook`,`river_speed`,`river_dir`,`capacity`,`height`,`spec`,`mine_trapped`) VALUES
(7909, 6099984, -5, 9, 'A Dragon Roost Above the Xanesla Coast',
 '  A weathered platform has been bolted to the very top of the great\r\nlighthouse overlooking Xanesla''s harbor. Gulls wheel below, and the\r\nsalt wind off the open water is sharp and constant up here above the\r\nbeacon.\r\n',
 63, 0, 29, 0, 0, 0, 0, 0, 0, -1, 0, 0)
ON DUPLICATE KEY UPDATE `name`=VALUES(`name`), `description`=VALUES(`description`);

INSERT INTO `room` (`vnum`,`x`,`y`,`z`,`name`,`description`,`zone`,`room_flag`,`sector`,`teletime`,`teletarg`,`telelook`,`river_speed`,`river_dir`,`capacity`,`height`,`spec`,`mine_trapped`) VALUES
(7910, 6099986, -5, 10, 'A Dragon Roost Above the Obsidian Citadel',
 '  Black stone battlements ring a bare landing perched above the\r\nObsidian Citadel. The dark walls of the citadel drink in the daylight\r\neven from up here, and the wind carries a faint metallic tang off the\r\nmines below.\r\n',
 63, 0, 29, 0, 0, 0, 0, 0, 0, -1, 0, 0)
ON DUPLICATE KEY UPDATE `name`=VALUES(`name`), `description`=VALUES(`description`);

INSERT INTO `roomexit` (`vnum`,`direction`,`name`,`description`,`type`,`condition_flag`,`lock_difficulty`,`weight`,`key_num`,`destination`) VALUES
(3011, 4, '', '', 0, 0, -1, -1, -1, 7907)
ON DUPLICATE KEY UPDATE `destination`=VALUES(`destination`);
INSERT INTO `roomexit` (`vnum`,`direction`,`name`,`description`,`type`,`condition_flag`,`lock_difficulty`,`weight`,`key_num`,`destination`) VALUES
(7907, 5, '', '', 0, 0, -1, -1, -1, 3011)
ON DUPLICATE KEY UPDATE `destination`=VALUES(`destination`);

INSERT INTO `roomexit` (`vnum`,`direction`,`name`,`description`,`type`,`condition_flag`,`lock_difficulty`,`weight`,`key_num`,`destination`) VALUES
(3726, 4, '', '', 0, 0, -1, -1, -1, 7908)
ON DUPLICATE KEY UPDATE `destination`=VALUES(`destination`);
INSERT INTO `roomexit` (`vnum`,`direction`,`name`,`description`,`type`,`condition_flag`,`lock_difficulty`,`weight`,`key_num`,`destination`) VALUES
(7908, 5, '', '', 0, 0, -1, -1, -1, 3726)
ON DUPLICATE KEY UPDATE `destination`=VALUES(`destination`);

INSERT INTO `roomexit` (`vnum`,`direction`,`name`,`description`,`type`,`condition_flag`,`lock_difficulty`,`weight`,`key_num`,`destination`) VALUES
(6314, 4, '', '', 0, 0, -1, -1, -1, 7909)
ON DUPLICATE KEY UPDATE `destination`=VALUES(`destination`);
INSERT INTO `roomexit` (`vnum`,`direction`,`name`,`description`,`type`,`condition_flag`,`lock_difficulty`,`weight`,`key_num`,`destination`) VALUES
(7909, 5, '', '', 0, 0, -1, -1, -1, 6314)
ON DUPLICATE KEY UPDATE `destination`=VALUES(`destination`);

INSERT INTO `roomexit` (`vnum`,`direction`,`name`,`description`,`type`,`condition_flag`,`lock_difficulty`,`weight`,`key_num`,`destination`) VALUES
(7119, 4, '', '', 0, 0, -1, -1, -1, 7910)
ON DUPLICATE KEY UPDATE `destination`=VALUES(`destination`);
INSERT INTO `roomexit` (`vnum`,`direction`,`name`,`description`,`type`,`condition_flag`,`lock_difficulty`,`weight`,`key_num`,`destination`) VALUES
(7910, 5, '', '', 0, 0, -1, -1, -1, 7119)
ON DUPLICATE KEY UPDATE `destination`=VALUES(`destination`);

INSERT INTO `mob` (`vnum`,`name`,`short_desc`,`long_desc`,`description`,`actions`,`affects`,`faction`,`fact_perc`,`letter`,`attacks`,`class`,`level`,`tohit`,`ac`,`hpbonus`,`damage_level`,`damage_precision`,`gold`,`race`,`weight`,`height`,`str`,`bra`,`con`,`dex`,`agi`,`intel`,`wis`,`foc`,`per`,`cha`,`kar`,`spe`,`pos`,`def_position`,`sex`,`spec_proc`,`skin`,`vision`,`can_be_seen`,`max_exist`,`local_sound`,`adjacent_sound`,`align`,`body_type`) VALUES
(7907, 'dragon keeper Wenna', 'Wenna the dragon-keeper', 'Wenna the dragon-keeper watches the sky above Amber Castle.',
 '  Wenna is a stout, sharp-eyed woman wrapped in a heavy fur-lined\r\ncloak against the wind up here. A coil of chain hangs at her belt for\r\nsteadying a landing dragon.\r\n',
 2, 0, 0, 50, 'A', 1.0, 4, 20, 0, 15.0, 15.0, 15.0, 15, 0, 1, 155, 66, 0,0,0,0,0,0,0,0,0,0,0,0, 9, 9, 0, 0, 68, 0, 0, 1, '', '', 0, 1)
ON DUPLICATE KEY UPDATE `name`=VALUES(`name`), `short_desc`=VALUES(`short_desc`), `long_desc`=VALUES(`long_desc`), `description`=VALUES(`description`);

INSERT INTO `mob` (`vnum`,`name`,`short_desc`,`long_desc`,`description`,`actions`,`affects`,`faction`,`fact_perc`,`letter`,`attacks`,`class`,`level`,`tohit`,`ac`,`hpbonus`,`damage_level`,`damage_precision`,`gold`,`race`,`weight`,`height`,`str`,`bra`,`con`,`dex`,`agi`,`intel`,`wis`,`foc`,`per`,`cha`,`kar`,`spe`,`pos`,`def_position`,`sex`,`spec_proc`,`skin`,`vision`,`can_be_seen`,`max_exist`,`local_sound`,`adjacent_sound`,`align`,`body_type`) VALUES
(7908, 'dragon keeper Torvald', 'Torvald the dragon-keeper', 'Torvald the dragon-keeper tends the roost above Logrus.',
 '  Torvald is a broad, weathered man missing two fingers on his left\r\nhand -- an old dragon-handling mishap, if the scars are any guide. He\r\nkeeps a wary eye on the horizon.\r\n',
 2, 0, 0, 50, 'A', 1.0, 4, 20, 0, 15.0, 15.0, 15.0, 15, 0, 1, 165, 71, 0,0,0,0,0,0,0,0,0,0,0,0, 9, 9, 1, 0, 68, 0, 0, 1, '', '', 0, 1)
ON DUPLICATE KEY UPDATE `name`=VALUES(`name`), `short_desc`=VALUES(`short_desc`), `long_desc`=VALUES(`long_desc`), `description`=VALUES(`description`);

INSERT INTO `mob` (`vnum`,`name`,`short_desc`,`long_desc`,`description`,`actions`,`affects`,`faction`,`fact_perc`,`letter`,`attacks`,`class`,`level`,`tohit`,`ac`,`hpbonus`,`damage_level`,`damage_precision`,`gold`,`race`,`weight`,`height`,`str`,`bra`,`con`,`dex`,`agi`,`intel`,`wis`,`foc`,`per`,`cha`,`kar`,`spe`,`pos`,`def_position`,`sex`,`spec_proc`,`skin`,`vision`,`can_be_seen`,`max_exist`,`local_sound`,`adjacent_sound`,`align`,`body_type`) VALUES
(7909, 'dragon keeper Marisol', 'Marisol the dragon-keeper', 'Marisol the dragon-keeper watches the sea from the lighthouse roost.',
 '  Marisol is a lean, sun-browned woman with salt-crusted braids and a\r\nspyglass tucked under one arm, always scanning the horizon for a\r\ndragon''s silhouette.\r\n',
 2, 0, 0, 50, 'A', 1.0, 4, 20, 0, 15.0, 15.0, 15.0, 15, 0, 1, 145, 65, 0,0,0,0,0,0,0,0,0,0,0,0, 9, 9, 0, 0, 68, 0, 0, 1, '', '', 0, 1)
ON DUPLICATE KEY UPDATE `name`=VALUES(`name`), `short_desc`=VALUES(`short_desc`), `long_desc`=VALUES(`long_desc`), `description`=VALUES(`description`);

INSERT INTO `mob` (`vnum`,`name`,`short_desc`,`long_desc`,`description`,`actions`,`affects`,`faction`,`fact_perc`,`letter`,`attacks`,`class`,`level`,`tohit`,`ac`,`hpbonus`,`damage_level`,`damage_precision`,`gold`,`race`,`weight`,`height`,`str`,`bra`,`con`,`dex`,`agi`,`intel`,`wis`,`foc`,`per`,`cha`,`kar`,`spe`,`pos`,`def_position`,`sex`,`spec_proc`,`skin`,`vision`,`can_be_seen`,`max_exist`,`local_sound`,`adjacent_sound`,`align`,`body_type`) VALUES
(7910, 'dragon keeper Ghurn', 'Ghurn the dragon-keeper', 'Ghurn the dragon-keeper stands watch above the Obsidian Citadel.',
 '  Ghurn is a grim, heavily-scarred man in blackened leather gauntlets,\r\nsaying little as he watches the dark stone battlements below for any\r\nsign of trouble.\r\n',
 2, 0, 0, 50, 'A', 1.0, 4, 20, 0, 15.0, 15.0, 15.0, 15, 0, 1, 175, 72, 0,0,0,0,0,0,0,0,0,0,0,0, 9, 9, 1, 0, 68, 0, 0, 1, '', '', 0, 1)
ON DUPLICATE KEY UPDATE `name`=VALUES(`name`), `short_desc`=VALUES(`short_desc`), `long_desc`=VALUES(`long_desc`), `description`=VALUES(`description`);

INSERT INTO `dragon_route` (`from_room`,`to_room`,`dest_name`,`fee`) VALUES
(7900, 7907, 'Amber', 1500)
ON DUPLICATE KEY UPDATE `dest_name`=VALUES(`dest_name`), `fee`=VALUES(`fee`);
INSERT INTO `dragon_route` (`from_room`,`to_room`,`dest_name`,`fee`) VALUES
(7907, 7900, 'Tobin City', 1500)
ON DUPLICATE KEY UPDATE `dest_name`=VALUES(`dest_name`), `fee`=VALUES(`fee`);

INSERT INTO `dragon_route` (`from_room`,`to_room`,`dest_name`,`fee`) VALUES
(7900, 7908, 'Logrus', 1500)
ON DUPLICATE KEY UPDATE `dest_name`=VALUES(`dest_name`), `fee`=VALUES(`fee`);
INSERT INTO `dragon_route` (`from_room`,`to_room`,`dest_name`,`fee`) VALUES
(7908, 7900, 'Tobin City', 1500)
ON DUPLICATE KEY UPDATE `dest_name`=VALUES(`dest_name`), `fee`=VALUES(`fee`);

INSERT INTO `dragon_route` (`from_room`,`to_room`,`dest_name`,`fee`) VALUES
(7900, 7909, 'Xanesla', 1500)
ON DUPLICATE KEY UPDATE `dest_name`=VALUES(`dest_name`), `fee`=VALUES(`fee`);
INSERT INTO `dragon_route` (`from_room`,`to_room`,`dest_name`,`fee`) VALUES
(7909, 7900, 'Tobin City', 1500)
ON DUPLICATE KEY UPDATE `dest_name`=VALUES(`dest_name`), `fee`=VALUES(`fee`);

INSERT INTO `dragon_route` (`from_room`,`to_room`,`dest_name`,`fee`) VALUES
(7900, 7910, 'Mithros', 1500)
ON DUPLICATE KEY UPDATE `dest_name`=VALUES(`dest_name`), `fee`=VALUES(`fee`);
INSERT INTO `dragon_route` (`from_room`,`to_room`,`dest_name`,`fee`) VALUES
(7910, 7900, 'Tobin City', 1500)
ON DUPLICATE KEY UPDATE `dest_name`=VALUES(`dest_name`), `fee`=VALUES(`fee`);

INSERT INTO `room` (`vnum`,`x`,`y`,`z`,`name`,`description`,`zone`,`room_flag`,`sector`,`teletime`,`teletarg`,`telelook`,`river_speed`,`river_dir`,`capacity`,`height`,`spec`,`mine_trapped`) VALUES
(7911, 6100000, -5, 11, 'Skimming a Wide River', 'The dragon drops low, skimming just above a broad, slow-moving river.\r\nSunlight scatters across the water in long streaks, and you can\r\nalmost feel the spray off the current far below.\r\n', 63, 0, 29, 0, 0, 0, 0, 0, 0, -1, 0, 0)
ON DUPLICATE KEY UPDATE `name`=VALUES(`name`), `description`=VALUES(`description`);

INSERT INTO `room` (`vnum`,`x`,`y`,`z`,`name`,`description`,`zone`,`room_flag`,`sector`,`teletime`,`teletarg`,`telelook`,`river_speed`,`river_dir`,`capacity`,`height`,`spec`,`mine_trapped`) VALUES
(7912, 6100002, -5, 12, 'The City Lights Ahead', 'Rooftops and torchlight begin to gather on the horizon, a scattered\r\nconstellation against the darkening land. The dragon angles toward\r\nthem, wings cupping the air to slow its approach.\r\n', 63, 0, 29, 0, 0, 0, 0, 0, 0, -1, 0, 0)
ON DUPLICATE KEY UPDATE `name`=VALUES(`name`), `description`=VALUES(`description`);
