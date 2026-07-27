-- Newbie equipment suits (user 2026-07-26): "6 sets of newbie equipment
-- to load on the character when connecting for the first time... a
-- shield and a weapon based upon class choice." A `suit` is just a named,
-- optionally class-restricted bundle of obj vnums -- builder-extensible
-- (any new suit just needs a row here plus its own suit_item rows, no
-- code change), backing three real features: automatic issue at
-- character creation (player_repo.c's player_create()), the `loadsuit`
-- immortal command (cmd_loadsuit.c, level 56+), and the Grimhaven
-- Welfare Department social worker's gear reissue (cmd_say.c's
-- SPEC_PROC_NEWBIE_EQUIPPER dispatch, mob vnum 90 at room 570).
--
-- suit_grant() (suit.c) just loads every item straight into inventory --
-- deliberately NOT auto-equipped/auto-wielded (user 2026-07-26: "they
-- can hold the items themselves, just load into inventory"), so no
-- item ordering matters here.
CREATE TABLE IF NOT EXISTS `suit` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `name` varchar(32) NOT NULL,
  `class` int(11) DEFAULT NULL, /* player_class_t value (being.h); NULL = any class */
  `description` varchar(128) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `name` (`name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

CREATE TABLE IF NOT EXISTS `suit_item` (
  `suit_id` int(11) NOT NULL,
  `obj_vnum` int(11) NOT NULL,
  PRIMARY KEY (`suit_id`, `obj_vnum`),
  CONSTRAINT `fk_suit_item_suit_id` FOREIGN KEY (`suit_id`) REFERENCES `suit` (`id`) ON DELETE CASCADE,
  CONSTRAINT `fk_suit_item_obj_vnum` FOREIGN KEY (`obj_vnum`) REFERENCES `obj` (`vnum`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

-- One suit per class (player_class_t: 0=Mage,1=Cleric,2=Warrior,3=Thief,
-- 4=Druid,5=Monk, being.h). Every suit gets the same shield (1010,
-- "shield training"), the same torch (105, "torch generic") and backpack
-- (600, "backpack pack leather"), plus a class-appropriate weapon --
-- three reused from real upstream "training"-tier items (177/325/329),
-- three newly added at the same stat profile (90003-90005, newbie_gear.sql).
INSERT INTO `suit` (name, class, description) VALUES
('mage_newbie', 0, 'Newbie gear for a Mage'),
('cleric_newbie', 1, 'Newbie gear for a Cleric'),
('warrior_newbie', 2, 'Newbie gear for a Warrior'),
('thief_newbie', 3, 'Newbie gear for a Thief'),
('druid_newbie', 4, 'Newbie gear for a Druid'),
('monk_newbie', 5, 'Newbie gear for a Monk')
ON DUPLICATE KEY UPDATE `description` = VALUES(`description`);

INSERT INTO `suit_item` (suit_id, obj_vnum)
SELECT s.id, v.vnum FROM `suit` s
JOIN (
  SELECT 'mage_newbie' AS suit_name, 177 AS vnum UNION ALL
  SELECT 'mage_newbie', 1010 UNION ALL
  SELECT 'mage_newbie', 105 UNION ALL
  SELECT 'mage_newbie', 600 UNION ALL
  SELECT 'cleric_newbie', 90003 UNION ALL
  SELECT 'cleric_newbie', 1010 UNION ALL
  SELECT 'cleric_newbie', 105 UNION ALL
  SELECT 'cleric_newbie', 600 UNION ALL
  SELECT 'warrior_newbie', 329 UNION ALL
  SELECT 'warrior_newbie', 1010 UNION ALL
  SELECT 'warrior_newbie', 105 UNION ALL
  SELECT 'warrior_newbie', 600 UNION ALL
  SELECT 'thief_newbie', 325 UNION ALL
  SELECT 'thief_newbie', 1010 UNION ALL
  SELECT 'thief_newbie', 105 UNION ALL
  SELECT 'thief_newbie', 600 UNION ALL
  SELECT 'druid_newbie', 90004 UNION ALL
  SELECT 'druid_newbie', 1010 UNION ALL
  SELECT 'druid_newbie', 105 UNION ALL
  SELECT 'druid_newbie', 600 UNION ALL
  SELECT 'monk_newbie', 90005 UNION ALL
  SELECT 'monk_newbie', 1010 UNION ALL
  SELECT 'monk_newbie', 105 UNION ALL
  SELECT 'monk_newbie', 600
) v ON v.suit_name = s.name
ON DUPLICATE KEY UPDATE `suit_id` = `suit_id`;
