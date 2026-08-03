-- Player feature requests: filed with the `idea` command, listed by
-- immortals with bare `idea`, and removed with `delidea <id>` (59+). Same
-- shape as bug.sql -- records who submitted each idea and when
-- (cmd_idea.c / cmd_delidea.c / idea_repo.c). Tobin-specific (not upstream
-- seed). CREATE ... IF NOT EXISTS keeps re-runs idempotent.

CREATE TABLE IF NOT EXISTS `idea` (
  `id` int NOT NULL AUTO_INCREMENT,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `submitter` varchar(64) NOT NULL DEFAULT '',
  `body` text NOT NULL,
  `room_vnum` int DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
