-- Player typo/text reports: filed with the `typo` command, listed by
-- immortals with bare `typo`, and removed with `deltypo <id>` (59+). Same
-- shape as bug.sql/idea.sql -- records who submitted each report, when,
-- and where (cmd_typo.c / cmd_deltypo.c / typo_repo.c). Tobin-specific
-- (not upstream seed). CREATE ... IF NOT EXISTS keeps re-runs idempotent.

CREATE TABLE IF NOT EXISTS `typo` (
  `id` int NOT NULL AUTO_INCREMENT,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `submitter` varchar(64) NOT NULL DEFAULT '',
  `body` text NOT NULL,
  `room_vnum` int DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
