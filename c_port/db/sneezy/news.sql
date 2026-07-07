-- Game news: player-facing announcements shown newest-first by the `news`
-- command (cmd_news.c / news_repo.c). Tobin-specific (not upstream seed).
--
-- HOUSE RULE (user-directed, 2026-07-04): every code change that affects a
-- player's ability to play, changes a command, or adds new zones gets a news
-- entry appended below. Keep the text player-facing prose -- NO NUMBERS
-- (no vnums, levels, counts, versions) in news bodies or titles.
--
-- `title` is UNIQUE so re-running this file is idempotent (ON DUPLICATE KEY
-- UPDATE title=title -- a no-op that never clobbers in-game edits).

CREATE TABLE IF NOT EXISTS `news` (
  `id` int NOT NULL AUTO_INCREMENT,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `author` varchar(64) NOT NULL DEFAULT '',
  `title` varchar(120) NOT NULL,
  `body` text NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `uniq_title` (`title`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'The Room Builder Reborn',
 'Immortal builders now shape the world through a menu-driven room editor. Pick a field, make your change, and save when you are ready -- nothing is written to the world until you do.'),
('The TobinMUD Team', 'Doorways Arrive',
 'Exits between rooms can now bear doors, gates, and other barriers, complete with locks and hidden passages, ready for builders to place throughout the realm.'),
('The TobinMUD Team', 'Read All About It',
 'The news command has arrived. Type news at any time to catch up on the latest changes to the world and the ways you play.'),
('The TobinMUD Team', 'News Grows a Voice',
 'The news can now be read at greater length, and the immortals among us may pen news of their own for everyone to read.'),
('The TobinMUD Team', 'Take a Load Off',
 'You can now sit, rest, and sleep. Resting mends your wounds faster, and sleeping faster still -- though a sleeper sees nothing until they wake. Stand up before you travel or draw steel.'),
('The TobinMUD Team', 'Know Thy Wounds',
 'Your score now names your condition at a glance, from perfect health all the way down to near death.'),
('The TobinMUD Team', 'Say It With a Smile',
 'Socials have arrived. Smile, nod, wave, bow, cheer and more -- type socials to see them all, and aim one at a friend or at the whole room.'),
('The TobinMUD Team', 'By the Numbers',
 'A new mudstats command shows how big the world is -- how many rooms, creatures, and objects fill it.')
ON DUPLICATE KEY UPDATE `title` = `title`;

-- The held-message / editor feature is immortal-only, so it is not news for
-- players -- remove the entry if an earlier build seeded it.
DELETE FROM `news` WHERE `title` = 'Edit in Peace';

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Pick Up the Pieces',
 'The world can now hold real objects. Get and drop what you find lying around, check your inventory, and wear or remove anything suited to your body -- and hold onto your gear carefully, since it will scatter across the ground wherever you happen to fall in a fight.'),
('The TobinMUD Team', 'Something''s Watching You',
 'The world is no longer empty. Creatures now walk among the rooms, ready to defend themselves if provoked -- attack or kill one the same way you would another adventurer, and take a closer look at anything that catches your eye.')
ON DUPLICATE KEY UPDATE `title` = `title`;
