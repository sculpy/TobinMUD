-- Immortal news channel: announcements that concern immortals (builders,
-- staff), read with the level-51+ `wiznews` command and posted with
-- `edwiznews`. Same shape as news.sql, a separate table so immortal news
-- never mixes into the public `news` feed. Tobin-specific.
--
-- `title` is UNIQUE so re-running this file is idempotent.

CREATE TABLE IF NOT EXISTS `wiznews` (
  `id` int NOT NULL AUTO_INCREMENT,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `author` varchar(64) NOT NULL DEFAULT '',
  `title` varchar(120) NOT NULL,
  `body` text NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `uniq_title` (`title`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Immortal News Arrives',
 'This channel carries word meant for the immortals -- building notes, staff decisions, and matters that need not trouble the mortal world. Read it with wiznews; those of high enough rank post to it with edwiznews.')
ON DUPLICATE KEY UPDATE `title` = `title`;
