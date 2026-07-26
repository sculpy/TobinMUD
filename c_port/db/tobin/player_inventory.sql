-- Persists which object PROTOTYPE instances a player is carrying/wearing/
-- holding across a reconnect (Phase 2C -- see obj_repo.h/obj.h). Tobin-
-- specific (not upstream seed); the prototypes themselves live in the
-- upstream-seeded `obj` table (db/tobin/obj.sql, unchanged).
--
-- Room-floor objects (via bare `oload`, not carried) are deliberately NOT
-- persisted here -- there's no zone-reset system yet to repopulate them at
-- boot, so they're lost on restart (see STATUS.md). Only a player's own
-- carried/worn/held instances survive.
--
-- `slot` encoding (obj_repo.h): -1 carried loose, 0..12 a worn limb_t index,
-- -2/-3 the held[] primary/off-hand pair.
--
-- CREATE TABLE IF NOT EXISTS (not the mysqldump-style unconditional DROP+
-- CREATE some earlier tables use) so re-running apply-tobin-schema.sh as
-- the "apply new migrations" step never wipes live player inventories.

CREATE TABLE IF NOT EXISTS `player_inventory` (
  `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `player_id` bigint(20) unsigned NOT NULL,
  `vnum` int(11) NOT NULL,
  `slot` int(11) NOT NULL DEFAULT -1,
  PRIMARY KEY (`id`),
  KEY `player_id` (`player_id`),
  CONSTRAINT `fk_player_inventory_player_id` FOREIGN KEY (`player_id`)
    REFERENCES `player` (`id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;
