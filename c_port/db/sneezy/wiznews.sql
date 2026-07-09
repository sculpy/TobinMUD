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

-- Zones part 1 (home): zonefile reset commands converted to the DB.
INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'The Zones Come Home',
 'Groundwork: every zone''s reset instructions -- the rules that say which creatures and items belong in which rooms -- have been converted out of the old flat files and into the database, where the rest of the world already lives. Nearly thirty-six thousand instructions in all. The game does not act on them yet; that comes next, and will let rooms repopulate themselves instead of standing empty.')
ON DUPLICATE KEY UPDATE `title` = `title`;

-- Containers (work): put/get into containers, look inside, open/close.
INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Containers Hold Their Own',
 'Containers work now. A player can put a carried item into a bag or chest and take it back out, whether the container is on the ground or in their own hands, and looking at an open container shows what is inside. The open and close commands now operate on containers as well as doors, and a closed container refuses access until it is opened. Capacity is enforced by weight. Locking and keys are still to come, so a locked container simply cannot be opened yet. A container carried by a player keeps its contents across a relog, though for now those contents spill loose into the pack rather than staying nested -- proper nesting persistence needs a schema change and is deferred. This also clears the way for the next stage of zone resets, where the world will be able to load items pre-stashed inside containers.')
ON DUPLICATE KEY UPDATE `title` = `title`;
