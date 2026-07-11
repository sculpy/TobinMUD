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

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Cast Your Gaze Afar',
 'A new scan command lets you peer several rooms down each passage and spot who -- or what -- lurks nearby, and roughly how far off. Scan a single direction, or scan by name to hunt for someone in particular. Closed and hidden doors still keep their secrets.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'A Place for Everything',
 'Bags, chests, and other containers now actually hold things. Put an item inside, take it back out, and look in to see what a container holds -- whether it is sitting on the ground or riding in your own pack. Some containers open and close: a closed one keeps its contents to itself until you open it. Mind what you cram in, though, for every container has only so much room.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'A Firmer Grip',
 'Holding something in hand now works properly. A weapon must be wielded, while anything else you carry in a free hand is simply held -- switch swaps whatever is in your two hands without letting go of either. Equipment listings read more clearly too, and no longer pretend a spot exists where nothing could ever actually be worn.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'A Dropped Connection Is Not the End',
 'Losing your connection no longer means losing your place in the world. Your character now simply stands where you were, quietly linkdead, until you reconnect -- no one else can lay a hand on you while you are gone, so you will find yourself right back where you left off.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Losing a Limb Is Now Literal',
 'Fights carry real stakes now. Take enough punishment to a single limb and it gives out for good, leaving something grim behind on the ground for anyone to find. Lose your head entirely and that is the end of you -- so keep an eye on more than just your overall health.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Death Leaves a Body Behind',
 'Whatever falls in battle -- player or monster -- now leaves a corpse behind, holding everything it was carrying. You cannot haul the body off, but you can get anything out of it right away, no fumbling with a lock or a lid.'),
('The TobinMUD Team', 'Killing Now Pays, and a New Way to Fight',
 'Winning a fight now earns experience toward your next level. A foe caught sitting, resting, or otherwise not on their feet is easier to land a hit on. And a new hit command lets anyone -- immortals included -- pick a real, honest fight instead of an instant kill.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'The World Wakes Up',
 'Rooms across the realm are no longer standing empty. Monsters and items are back where they belong, and the world quietly restocks itself over time as things get used up or cleared out.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Time Marches On',
 'The world now keeps its own clock and calendar. Type time to see it: the hour, the day of the week, and the date. Noon and midnight are announced to everyone, and so is the turn of a new month or a new year.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Your Own Time, Too',
 'Creating a new account now asks one more question: how many hours your own clock differs from the game''s home time zone. Answer it once and time will always show a second line with the real-world time where you are. Change it anytime with time followed by the difference in hours.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Read Without Interruption',
 'Paging through news a screen at a time is now uninterrupted -- nothing else pushes its way onto your screen mid-page. Anything that happened while you were reading is waiting for you afterward with catchup, which anyone can now use, not just staff.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Brighter Colors, Fixed',
 'A color bug is fixed: bright text no longer stays stuck bright when it should have faded back to regular. Colors throughout the game now behave the way they were always meant to.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'A Few Looking Fixes',
 'Looking around got a little more polished: some descriptions were losing their capital letter, looking at certain creatures could show a jumble of raw keywords instead of a proper name, and a few longer descriptions were cutting off mid-sentence. All fixed.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Scan Sees Only the Living',
 'Scan no longer reports someone whose connection has dropped. A linkdead character was never a real target to begin with -- now scan agrees.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Got an Idea?',
 'Type idea followed by your suggestion to send it straight to the immortals. Same idea as bug, just for the things you wish the game did rather than the things it does wrong.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Time Keeps Going',
 'The world clock no longer forgets itself when the server restarts. Whatever time it was before, it still is.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'A Quiet Heartbeat',
 'Once every hour, on the half hour, your screen gets a single blank line -- no message, just a nudge that time is passing.')
ON DUPLICATE KEY UPDATE `title` = `title`;
