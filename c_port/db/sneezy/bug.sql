-- Player bug reports: filed with the `bug` command, listed by immortals with
-- bare `bug`, and removed with `delbug <id>` (59+). Records who submitted each
-- report and when (cmd_bug.c / cmd_delbug.c / bug_repo.c). Tobin-specific
-- (not upstream seed). CREATE ... IF NOT EXISTS keeps re-runs idempotent.

CREATE TABLE IF NOT EXISTS `bug` (
  `id` int NOT NULL AUTO_INCREMENT,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `submitter` varchar(64) NOT NULL DEFAULT '',
  `body` text NOT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
