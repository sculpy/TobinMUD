-- Periodic spell-component placement rules (the "forage reagents in the
-- wild" half of SneezyMUD's obj_component.cc, ported data-driven -- see
-- src/core/component_placement.c).  Each row spawns (or removes) a reagent
-- object into a room range on a time-of-day / weather window.  Read once
-- at boot by component_placement_load(); edit rows + reboot (or reload) to
-- retune.  Kept as DB data, NOT a hardcoded C table, precisely so the
-- planned room deletion/rebuild only needs these rows re-seeded, no code
-- change.
--
-- Columns:
--   action        'place' spawns the reagent; 'remove' despawns it.
--   room1,room2   room-vnum range (room2 = -1 for a single room).
--   comp_vnum     the reagent object (obj.vnum).
--   chance        percent chance to fire on a tick whose window matches.
--   hour1,hour2   game-hour window [hour1,hour2) (0-23); hour1=-1 any hour,
--                 hour2=-1 exactly hour1, hour1>hour2 wraps past midnight.
--   weather       bitmask over (1<<weather_t): CLEAR=1, CLOUDY=2, RAINY=4,
--                 STORMY=8 (OR them); -1 = any weather.
--   max_per_room  don't place if the room already holds this many (<=0 -> 1).
--   message       room echo on a successful place/remove ('' = silent).
--   enabled       0 disables the row without deleting it.

CREATE TABLE IF NOT EXISTS `component_placement` (
  `id`           INT NOT NULL AUTO_INCREMENT,
  `action`       ENUM('place','remove') NOT NULL DEFAULT 'place',
  `room1`        INT NOT NULL,
  `room2`        INT NOT NULL DEFAULT -1,
  `comp_vnum`    INT NOT NULL,
  `chance`       INT NOT NULL DEFAULT 100,
  `hour1`        INT NOT NULL DEFAULT -1,
  `hour2`        INT NOT NULL DEFAULT -1,
  `weather`      INT NOT NULL DEFAULT -1,
  `max_per_room` INT NOT NULL DEFAULT 1,
  `message`      VARCHAR(255) NOT NULL DEFAULT '',
  `enabled`      TINYINT NOT NULL DEFAULT 1,
  PRIMARY KEY (`id`)
);

-- Starter set (small on purpose -- see the room-rebuild note above).
INSERT INTO `component_placement`
  (`id`,`action`,`room1`,`room2`,`comp_vnum`,`chance`,`hour1`,`hour2`,`weather`,`max_per_room`,`message`) VALUES
  (1,'place', 1008,1015, 239, 60,  6, 20, 3, 1, 'The daylight scatters into a shimmering rainbow stone on the ground.'),
  (2,'place',  199, 245, 227, 50, 20,  6,-1, 1, 'A pixie torch flickers to life among the undergrowth.'),
  (3,'place',  199, 245, 262, 30, 20,  6,-1, 1, 'A stray bag of pixie dust settles softly onto the ground.'),
  (4,'place', 6798,  -1, 221, 50, -1, -1, 3, 2, 'The wind bares a patch of white silicon in the sand.'),
  (5,'remove', 199, 245, 227,100,  6, -1,-1, 0, 'The pixie torches wink out as dawn breaks over the wood.'),
  (6,'place', 7510,7512, 239, 50,  6, 20, 3, 1, 'A rainbow stone glints among the rocks.')
ON DUPLICATE KEY UPDATE
  `action`=VALUES(`action`), `room1`=VALUES(`room1`), `room2`=VALUES(`room2`),
  `comp_vnum`=VALUES(`comp_vnum`), `chance`=VALUES(`chance`), `hour1`=VALUES(`hour1`),
  `hour2`=VALUES(`hour2`), `weather`=VALUES(`weather`), `max_per_room`=VALUES(`max_per_room`),
  `message`=VALUES(`message`);
