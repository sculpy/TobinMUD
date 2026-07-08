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

-- Session 38 (home): code-change log for immortals, plain English. Standing
-- habit -- every code change gets a wiznews entry written for a human, not
-- in code-speak.
INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'A New Way to Look Around',
 'A scan command has been added. Anyone can now peer several rooms deep down each exit and see who or what is out there, and about how far off. It follows the map straight from the database, so it can trace passages even through rooms no one is standing in, and a closed or hidden door blocks the view that way.'),
('The TobinMUD Team', 'Finding Things by Name',
 'Builders have a new vnum command. Type vnum room, vnum obj, or vnum mob followed by part of a name and it lists the matching prototype numbers, lowest first -- a quick way to find what to load or where to go without querying the database by hand.')
ON DUPLICATE KEY UPDATE `title` = `title`;

-- Session 38 follow-ups (home): help-list size, editor keys, vnum paging.
INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Editors Speak One Language',
 'All the in-game editors now share the same handful of slash-commands, each named for its action: slash-s saves, slash-a aborts, slash-b blanks the text, and slash-f reformats it to the screen width. Those four are the only editor keys to remember now.'),
('The TobinMUD Team', 'Room to Breathe in Help',
 'The help and wizhelp command lists can now hold many more entries without running out of room, so nothing gets cut off as more commands are added. And the vnum command no longer stops at forty matches -- it shows the whole list a page at a time.')
ON DUPLICATE KEY UPDATE `title` = `title`;

-- The editor entry's body changed (legacy keys removed); the INSERT above is
-- a no-op on the already-seeded row, so update it explicitly.
UPDATE `wiznews` SET `body` = 'All the in-game editors now share the same handful of slash-commands, each named for its action: slash-s saves, slash-a aborts, slash-b blanks the text, and slash-f reformats it to the screen width. Those four are the only editor keys to remember now.'
  WHERE `title` = 'Editors Speak One Language';
