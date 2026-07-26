-- Zone ownership (Session 43, user: "add identity to zones... builder
-- gets assigned a zone then... a 51-54 wants to edit gets rejected except
-- for those assigned to that zone"). New-for-Tobin table, not part of the
-- upstream seed. See zone.h's zone_can_edit() for the actual gate.

CREATE TABLE IF NOT EXISTS `zone_owner` (
  `zone_nr` int NOT NULL,
  `player_id` bigint NOT NULL,
  `assigned_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`zone_nr`, `player_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
