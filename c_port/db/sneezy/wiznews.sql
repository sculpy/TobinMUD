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

-- Session home-migration follow-ups (2026-07-09): merged load command,
-- equipment/hold/wield/switch rework.
INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'One Command to Load Them All',
 'The old separate oload and mload builder commands are gone, replaced by a single load command. Type load mob or load obj (a single letter works too) followed by a vnum or a name to spawn either kind into your room.'),
('The TobinMUD Team', 'Hands, Properly Sorted',
 'Wearing something in a hand has been split into two honest verbs: wield for weapons, hold for everything else, with switch to swap them. The equipment list now lines its labels up neatly and no longer lists a wear spot for genitalia, since nothing was ever meant to be worn there.')
ON DUPLICATE KEY UPDATE `title` = `title`;

-- Linkdead persistence (2026-07-09).
INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Linkdead Bodies Stay Put',
 'A dropped connection no longer deletes the character from the world. They stay right where they were, marked (linkdead) in a room listing, until the same account logs back in -- which resumes them in that same room, with whatever the database says is current (a promotion or an edit made while they were gone still applies). Nobody can attack or otherwise target a linkdead character. A copyover or restart still clears them out, same as before.')
ON DUPLICATE KEY UPDATE `title` = `title`;

-- World death taunt scoped to players only (2026-07-09).
INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'A Quieter End for Monsters',
 'The world-wide taunt that announces a death on the INFO channel now fires only when a player is the one who dies. A monster meeting its end no longer interrupts everyone''s evening, though a monster still gets full credit if it''s the one doing the killing.')
ON DUPLICATE KEY UPDATE `title` = `title`;

-- Crit-hit/decapitation system (2026-07-09).
INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Combat Gets Some Teeth',
 'A limb battered down to nothing now genuinely gives out -- it sheds a lootable, grisly memento in the room, and if that limb happens to be the head, the fight is over right there. Only affects players for now, and only reachable through a real, sustained beating -- a low-level scrap will not end in a coin-flip beheading, since every limb was given a firmer minimum health pool first. A new immortal-only hurtlimb command exists purely to test this without waiting on the dice.')
ON DUPLICATE KEY UPDATE `title` = `title`;

-- Corpses on death + a backlog batch (2026-07-09).
INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Death Now Leaves Something Behind',
 'A death, whether a player''s or a monster''s, now leaves a proper corpse in the room -- a real container holding everything the deceased was carrying, wearing, or holding. It cannot be picked up whole, but its contents can be looted straight out with no need to open it first.'),
('The TobinMUD Team', 'A Handful of Small Fixes',
 'Typing @set now works instead of dead-ending. Immortals can no longer instantly slay an equal or higher-ranked immortal, even one who has toggled themselves mortal. Killing something now grants experience. Attacking someone who is sitting, resting, or otherwise not standing is easier to land. A few more socials speak with the right pronoun. Copyover''s countdown is now tinted. Help for color and who mention name substitution and the full tag list. And a new hit command lets an immortal pick a real fight instead of an instant kill.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'The Zones Wake Up',
 'Zone reset data has finally started doing something -- rooms populate with their monsters and objects the moment the server starts, and each zone quietly restocks itself afterward on its own timer. Covers the load mob, load object, equip, give, place, and door opcodes; the rest are logged and skipped for now. A new zone reset command force-runs any zone on demand, handy for testing edits without waiting or rebooting.'),
('The TobinMUD Team', 'Zones Have Owners Now',
 'A builder can be assigned to a zone with the new edzone command -- a menu-driven editor covering a zone''s name, enabled state, lifespan, vnum range, and its list of assigned builders (any number of builders per zone, not just one). A level 51-54 builder can only edit content in a zone they are assigned to; unzoned content, and 55-plus immortals, are unrestricted. There''s also a zone list command to see every zone and who owns what. Currently enforced on edroom and edzone -- the other editors will pick up the same rule as they get built.')
ON DUPLICATE KEY UPDATE `title` = `title`;

-- The "Zones Have Owners Now" body changed (zoneassign command replaced by
-- the edzone editor) -- the INSERT above is a no-op on the already-seeded
-- row, so update it explicitly.
UPDATE `wiznews` SET `body` = 'A builder can be assigned to a zone with the new edzone command -- a menu-driven editor covering a zone''s name, enabled state, lifespan, vnum range, and its list of assigned builders (any number of builders per zone, not just one). A level 51-54 builder can only edit content in a zone they are assigned to; unzoned content, and 55-plus immortals, are unrestricted. There''s also a zone list command to see every zone and who owns what. Currently enforced on edroom and edzone -- the other editors will pick up the same rule as they get built.'
  WHERE `title` = 'Zones Have Owners Now';

-- Editors-absolute-quiet bug fix (2026-07-10).
INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Editors Really Are Quiet Now',
 'Found and fixed a real bug: being mid-edit in edplayer or edzone did NOT actually protect you from incoming messages the way edroom always has -- the shared quiet-while-editing check only ever recognized edroom. Fixed centrally, so it now covers every menu editor uniformly, with everything you missed waiting for catchup as always. Also fixed several places (who, promote, set, copyover, the users roster) that were quietly treating anyone mid-edit as offline -- invisible from who, stale after a stat change, and at risk of losing their whole session across a copyover. All now correctly recognize an editing immortal as online.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'A Bold New Tag, and Ground to Stand On',
 'Two writing tools for descriptions: <d> (or <D>) bolds whatever color is currently active, e.g. <g><d>bold green<z>, rather than being a color of its own. And $$g (or $g) in an object description now substitutes the room''s ground surface -- street, road, water, mud, sand, floor, or plain ground -- so one description can read correctly in a dozen different rooms.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'A Home Time Zone for Everyone',
 'The server''s own clock and database both run on Eastern time, confirmed and left as-is. New account creation now also asks how many hours your own clock differs from Eastern (Pacific players enter -3, for instance), right after the color question. That choice is saved, and time now shows a second line with the real-world time where you are, adjusted accordingly. It can be changed anytime with time <difference>.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Reading in Peace',
 'Paging through a long listing -- news, for instance -- is now just as quiet as being in an editor: nothing interrupts you mid-page, and anything that happened while you were reading is waiting for you in catchup once you finish. Since news is open to everyone, catchup no longer requires immortal rank. The MORE prompt at the bottom of a page also got some color.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'A Real Bug Squashed: Bright Stayed Bright',
 'Found and fixed a genuine color bug while polishing the pager prompt: switching from a bright tag to a regular one of the same color (like <C>ENTER<c>) did not actually turn the bold off, so everything after it stayed bright too. Every regular-intensity color tag now forces a clean reset first, so bright really means bright and regular really means regular, everywhere in the game -- not just the pager.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Three Look Bugs, One Test Mob',
 'Chased down three real look bugs, all on the seeded dirty refuse hauler (vnum 33271): its short_desc starts with a color tag, which was defeating capitalization (the code was uppercasing the tag''s bracket instead of the real first letter); look <mob> was showing the raw keyword-match list instead of the short_desc ("You look at man dirty refuse hauler" instead of "a dirty refuse hauler"); and its real description (over a thousand characters) was silently cut off mid-sentence, because the buffer was sized for a player''s much shorter appearance text, not a mob''s. All three fixed -- builders, keep an eye out for other content with leading color tags in short_desc, since that''s what exposed the capitalization bug.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Ideas Welcome',
 'Players can now file feature requests with idea, the same shape as bug: your name and the date go with it, and immortals list outstanding ones with a bare idea and clear a handled one with delidea <id>. Filings show up as a typed [IDEA] log, and idea has its own setsev toggle if you want to opt out of seeing them live.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Know What''s Running', 'A new test command (58+) shows the name of whatever smoke test is currently running against this server, straight from the same @test hook that already logs it -- handy when watching a sweep from in-game instead of the console.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'A Quiet Paper Trail for Get and Drop', 'Every get and drop is now written to the day''s log file -- who, what (with its vnum), and which room -- silently, with no live [TAG] echo to anyone online. Pull it up later with log search <name> if a dispute over who had what needs sorting out. Also caught the same capitalization bug from the recent look fixes lurking in two more places (scan and get/drop messages both have their own copy of the same helper) -- fixed there too.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'The Clock Remembers Now', 'The world clock no longer resets to 8:00 AM day one every time the server restarts -- it saves itself every tick and picks right back up where it left off. Caught a nasty bug while building this: our query helper only understands %i for whole numbers, not the usual %d, and would have silently swallowed every save without complaint. Worth remembering for the next feature that writes numbers to the database.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'A Serious Repo Bug, Found and Fixed', 'Found something worth knowing about: a stray line in .gitignore meant to ignore crash-dump files was accidentally matching our own src/core folder by name, silently keeping gametime.c and zone.c out of the git repository the whole time, even though both were pushed as part of earlier commits. The actual code was never lost -- it was live on the server the whole time -- but the repository itself was missing two real source files. Fixed the ignore rule and both files are now properly tracked. Also added a quiet heartbeat: once every real hour, on the half hour, everyone gets a blank line so a tick is visible without any actual message.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Mobs Learn Right From Wrong', 'Characters now have an alignment (good/evil/neutral, set with set <name> alignment), shown in score as a word rather than a raw number. Aggressive mobs check it before picking a fight: a sufficiently good-aligned target gets left alone instead of attacked on sight. A scoped-down first step toward the original''s full Mobile_Attitude system -- suspicion, greed, malice, hate/fear lists, and the rest are still future work.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Pools, Puddles, and Poor Life Choices', 'A new immortal-only pee command (51+) leaves a puddle on the floor -- purely for flavor, but it plugged into two other systems: a bad enough limb injury now leaves a pool of blood behind too, and a new drink <puddle> command lets anyone sample either one, with a real chance of a (non-lethal) case of poisoning. Repeat the same kind of mess in one room and it grows -- puddle, then pool, then large pool, the color escalating from dim to bright the bigger it gets -- rather than littering the floor with duplicates. Left alone, every puddle shrinks back down a size every tick or so until it''s soaked fully into the ground.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'A Wandering Mob Bug, and Immortals Get Their Own Entrance', 'Fixed a real bug in mob wandering: the leave/arrive messages were printing a mob''s raw look-up keywords instead of its actual description ("lady stroll walk leaves" instead of anything sensible) -- now reads properly, with the real direction, e.g. "A lady walks to the east." Immortals can also now set their own custom arrival/departure messages with bamfin and bamfout, in place of the default wording -- e.g. "drags $p cross in from the $d" reads correctly for any gender, since $p/$d are filled in with the right pronoun and direction for whoever set it.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'One Edit Command To Rule Them All', 'edroom, edzone, edplayer, edhelp, ednews, edwiznews, and edrules are gone -- everything now goes through a single edit command instead: edit room [vnum], edit zone <n>, edit player <name>, edit help <topic>, edit news <headline>, edit wiznews <headline>, edit rules <n> <title>. Nothing about how any of them work changed, and every level requirement is exactly what it always was -- just one door to knock on instead of seven.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Scripting Arrives: edit trigger', 'Builders (51+) can now attach scripted behavior to a room, mob, or object with edit trigger -- no recompile needed, unlike SneezyMUD''s old spec proc system. Room triggers fire on enter or ambiently at random; mob triggers fire on greet (someone walks in), speech (a matching keyword), death, or random; object triggers fire on get or wear. The script itself is a short list of actions -- echo, echoroom, emote, teleport, give, damage, log -- written in the same line editor news and rules already use. See help trigger for the full rundown. This is a first pass: the action list is deliberately small on purpose, not a general-purpose language.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Shout It From the Rooftops', 'A new shout command reaches every playing character in the game, not just the shouter''s room -- asleep characters don''t hear it, and toggle noshout lets a mortal opt out entirely (an immortal''s shout always gets through regardless). See help shout.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'A Slow Tick, Fixed', 'Found and fixed a real performance bug in the new scripting system: the random-trigger check was quietly hitting the database once for every single mob and room in the world, every tick, even on servers with no random triggers set up at all. Now it checks once per tick instead, and aitick/the real world clock both run at their proper speed again.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Loot It All At Once', 'get all <container> empties an entire corpse, bag, or chest into your hands in one command instead of naming each item -- get all corpse after a kill is now all it takes.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'News Was Quietly Losing Its Oldest Entries', 'Found and fixed a real bug: both news and wiznews build the whole feed into a fixed-size buffer before showing it a page at a time, and with this many entries now on the books, that buffer was overflowing and silently cutting off everything past a certain point -- including some of the oldest stories. Both are sized generously now, with plenty of room to keep growing.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Fountains Are Drinkable Again', 'Fixed a real bug: drink only ever recognized ground puddles (pee/blood), so a real fountain or drink container already sitting in a room -- and there are plenty seeded in the world -- failed with "You don''t see that here to drink." Drink now also resolves any real drink-category object: clean water, no poison, never runs dry.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Fixtures Get Top Billing', 'Room descriptions now list permanent scenery -- fountains, lampposts, statuary, anything that can''t be picked up -- ahead of ordinary loot and whoever else is standing there, instead of whatever order they happened to load in.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'A Starter Taste of Scripting', 'Two of SneezyMUD''s old hardcoded behaviors have been reborn as real edit trigger content: the dirty refuse hauler mutters something rude if you say hello nearby (and grumbles unprompted now and then), and a new pickable tangle of thorny brambles scratches whoever grabs it. Both are ordinary database rows now, not compiled-in code -- see edit trigger list mob 33271 or edit trigger list obj 1000001 to see exactly how they''re built.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Browse Prototypes by Number, Not Just Name', 'vnum <room|obj|mob> <pattern> now also accepts a bare vnum or a vnum range (vnum obj 1017, or vnum obj 100-200) to browse prototypes directly by number, alongside its existing name search -- one command instead of three separate list commands.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Game-Wide Switches Get Their Own Room', 'toggle no longer shows or accepts multiplay or any other global, everyone-affecting switch -- those moved entirely to a new gametog command (58+). toggle is now purely about your own personal switches (color, hp in prompt, and the rest).')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Bugs Can Be Fixed, Not Just Deleted', 'A new edbug <id> [note] command (59+) marks a filed bug resolved without deleting it -- if the reporter is online right now, they get a live notice, note included. delbug is still there for a report that never deserved to be kept around at all.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'A Quiet Watcher Joins the Staff', 'A new snoop <name> command (59+) mirrors everything a lower-level player sees and types straight to your own screen -- you cannot snoop anyone your own level or higher, it just fails. Bare snoop (no name) stops it. Covert by design: the target is never told.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Help Files Get a Cleanup', 'Help topics now title-case the command name in their header, render the description in bright white instead of magenta, and drop redundant "Administrator (N+) only" phrasing now that the footer already shows Minimum Level. The /f (format) command in every text editor also indents each paragraph''s first line two spaces.')
ON DUPLICATE KEY UPDATE `title` = `title`;

-- Companion fix (user 2026-07-11: "/format doesn't work in room editor" --
-- the real syntax is the one-letter "/f", "/format" was only ever a doc/
-- comment shorthand, never an accepted alias): the INSERT above silently
-- no-ops on this already-seeded row (ON DUPLICATE KEY UPDATE title=title),
-- same lesson as help_topic.sql's snoop entry -- an explicit UPDATE is
-- required to actually correct the live body text.
UPDATE `wiznews` SET `body` = 'Help topics now title-case the command name in their header, render the description in bright white instead of magenta, and drop redundant "Administrator (N+) only" phrasing now that the footer already shows Minimum Level. The /f (format) command in every text editor also indents each paragraph''s first line two spaces.'
WHERE `title` = 'Help Files Get a Cleanup';

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Exits Now Match the Room', 'The [Exits:] line in look used to always be green -- now it takes on the same color as the room name itself, so a lava cavern''s exits glow red, a forest clearing''s glow green, and so on, instead of clashing with whatever the room actually looks like.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Choose Your Path', 'Character creation now asks for a race (Human, Elf, Ogre, Dwarf, Hobbit, Gnome), a class (Mage, Cleric, Warrior, Thief, Druid, Monk), and an alignment (Good, Neutral, Evil) right after point-buy attributes -- each race and class carries its own stat bonuses and penalties, and class also scales starting hit points. Alignment isn''t just flavor: an aligned aggressive mob will only ever fight the opposite alignment (never its own kind), leaving neutral players alone in combat but occasionally giving them a one-line in-character reaction instead -- a nod of approval from something good, a sneer from something evil.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Snoop Marks Every Mirrored Line', 'snoop used to only prefix the target''s typed commands with "% " -- their own output was mirrored to you completely unmarked, easy to mistake for your own screen. Every mirrored line is now prefixed the same way, commands and output alike.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Build It Live, Then Zonefile Create', 'New builder tool: `zonefile create <zone>` snapshots a zone''s CURRENT live mobs and objects -- load a mob, drop a chest, put something inside it -- into real reset data, so the next boot or periodic reset recreates exactly what you built. It''s safe to re-run: anything already covered is left alone, so deleting one line from the reset data and running it again only fills that gap back in, never duplicating what''s still there. See `help zonefile`.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Bamfin/Bamfout Now Belong to Goto', '`bamfin`/`bamfout` set your custom `goto` teleport messages now, shown to the room you leave and the room you arrive in -- three tokens available: <N> for your name (anywhere in the message), $g for the room''s ground word, and $p for your pronoun. The WALKING move-message feature that used to answer to `bamfin`/`bamfout` is renamed `poofin`/`poofout` (its original name) -- nobody''s custom message was lost in the move. See `help bamfin`/`help poofin`.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Character Creation Explains What Went Wrong', 'A rejected character name used to get one generic message no matter the reason. Now it tells you exactly what''s wrong: too short (under 3 letters), too long (over 15 letters), or containing something other than letters -- each gets its own message.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Nospam Silences Missed Swings', 'A new personal `toggle nospam` (default off) hides "you miss"/"X misses you" combat messages from your own screen -- ported from Sneezy''s AUTO_NOSPAM. It''s checked independently for each side of a swing, so your own toggle only ever affects what YOU see.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Hostnames Instead of Raw IPs', 'Immortal-facing logs and `users` now show a reverse-DNS hostname instead of a bare IP address where one resolves (falls back to the IP otherwise) -- looked up in the background so a slow DNS server never stalls the game.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Every Class Gets Its Own Skill Sheet', 'A new `skills` command shows your class''s full roster of skills/spells, ported from Sneezy''s real discipline lists and organized into three tiers: Combat, <Class> Skills, and Advanced <Class> Skills. Warrior, Thief, Monk, Cleric, and Mage are all populated -- a skill is known once your level reaches its threshold, no separate practice step yet. Actual in-combat mechanics for most individual skills are still on the way; this is the roster/visibility layer.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Edit Player Can Now Change Class and Race', '`edit player` (58+) gained two new fields: 9) Class and 0) Race, alongside the existing level/xp/hp/attributes/gender/title/load-room/handedness. Changes save and sync to an already-connected target immediately, same as every other field.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Armor Now Actually Protects You', 'Worn armor has a real Armor Class now, shown on `score`, and it makes you harder to hit -- subtracted from the attacker''s hit roll in combat. Since the seeded item data never carried a real per-piece AC value, it''s derived from the piece''s weight (heavier = more protection, capped at 30). The to-hit formula also gained a guaranteed hit/miss floor: no stat or gear mismatch, however extreme, can make a hit or a miss completely impossible.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Ordinal Targeting: 2.sword, 3.goblin', 'When more than one thing in a room shares a keyword, "get 2.sword"/"kill 3.goblin" now reach the 2nd/3rd match instead of always the first. Bare names still work exactly as before. Works for get/drop/put/give/wear/remove and attack/kill alike.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Combat Messages No Longer Show Raw Mob Keywords', 'Fighting a mob whose name has several keywords (e.g. "lady stroll walk", matched by look lady/stroll/walk) used to leak that raw keyword list into miss/hit/death messages and even a mob''s own corpse description. All of it now correctly shows the mob''s short description instead ("a lady out for a stroll"). Several smaller color-tag-skip capitalization bugs (mob greet/speech/death triggers, wander/scavenge/aggress messages) were fixed the same pass.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Casting and Praying Need the Right Item', 'Mages and Druids can now `cast <spell>` -- but need a spell component (any item keyworded "component") on hand, consumed on a successful cast. Clerics `pray <spell>` instead, needing a holy symbol (keyworded "symbol") that is NOT consumed. Both check class and level against the spell first. Real per-spell mechanics are still limited (heal/damage-flavored spells work; the rest cast successfully but do nothing yet) -- the full effect system is follow-up work.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Room Listings Stack Identical Things', 'Multiple identical mobs or objects in a room now show as one line with an "(xN)" count instead of repeating the same line over and over -- three gremlins now read "A gremlin is here. (x3)".')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Immortals Ignore Class Restrictions on Skills/Spells', 'Immortals can now `cast` or `pray` any spell in the game regardless of their own class, and `skills` shows every class''s full roster (grouped by class heading) instead of just their own. Level requirements are bypassed the same way. Component/holy-symbol item requirements still apply -- those are an item gate, not a class restriction. Mortals see no change.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Practice with a Guildmaster to Learn Your Discipline', 'Reaching a spell/skill''s level requirement is no longer enough on its own -- you now also need to have practiced that discipline. A new `practice` command, used at a guildmaster mob of your own class, raises your Basic and Advanced discipline percentages (10% per visit). Basic-tier skills/spells need any Basic percentage above 0; Advanced-tier ones need Basic at 95%+ AND some Advanced percentage. `skills` shows both percentages and marks locked entries with the reason. Guildmaster mobs are recognized by the "guildmaster" keyword plus a class match (the old, previously-unused mob.class column now feeds this). Immortals bypass this gate entirely, same as the class/level bypass.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Heal Someone Else, Then Just `continue`', '`pray heal light <target>` (and every other heal-type prayer) can now be aimed at someone else in the room instead of only yourself -- leave the target off to heal yourself as before. A new `continue` command repeats that same heal automatically, round after round, until the target is fully healed or you run out of holy symbols. Speaking of which: holy symbols are now consumed on every successful prayer, just like a mage or druid''s spell components -- no longer a permanent keepsake.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'New `balance` Command for Implementors (60+)', 'A new menu-driven `balance class|race <name>` command lets an Implementor tune four gamewide modifiers per class or race: HP multiplier, damage multiplier, to-hit modifier, and AC modifier. Every class/race starts perfectly neutral (1.00x/1.00x/+0/+0) -- nothing changes until someone actually balances one. Saved changes apply immediately, server-wide, no restart needed.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Weapon Depth: Sharp Weapons and Dual Wielding', 'Edged and piercing weapons (swords, axes, daggers, spears, etc) now deal a little extra damage over blunt ones. Also, the "dual wield" skill (Warrior/Thief) now does something: it removes the usual off-hand damage penalty, so a trained dual-wielder swings evenly with both hands instead of favoring their main hand.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Fixed: Leveling Up Now Actually Raises Your HP', 'Found while testing combat depth: leveling up used to only raise your level number -- your max HP (and every limb''s own HP) stayed exactly where it started, forever, making even a high-level character just as fragile as a fresh one. Leveling up now properly raises your max HP and fully heals you, the reward it was always supposed to be.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Doors Can Be Trapped Now', 'A Thief who knows "set trap (door)" can rig a trap on any closed door with the new `settrap <direction>` command; "disarm trap" safely removes one with `disarmtrap <direction>`. Walk through a rigged door without knowing "detect trap" and it springs, hurting a random limb -- one use only. A Thief who DOES know "detect trap" spots it and steps around unharmed, leaving it rigged for whoever comes next.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Buffs and Debuffs: The New Affects System', 'A new general-purpose "affects" system tracks temporary buffs and debuffs with their own countdown, separate from combat. First one live: `pray sanctuary` wraps you (or an ally) in a shimmering aura that halves incoming damage for a while, then fades on its own with a "wears off" message. Check what''s currently affecting you any time with the new `affects` command.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Fixed: Reconnecting Could Leave Your Limbs Fragile', 'Found while testing the affects system: reconnecting to an already-created character reset your limbs'' toughness back to a brand-new level-1 character''s, no matter how strong you''d actually grown -- making a veteran just as easy to dismember as a newbie right after logging back in. Limbs are now properly resized to match your real strength on every fresh login.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Friendlier Unknown-Command Message', 'Typing a command that does not exist no longer replies with the terse "Huh?!" -- it now says "Command not found, maybe submit an idea if you believe TobinMUD should have it."')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Grimhaven is Now Tobin City', 'Every mention of "Grimhaven" across rooms, mobs, objects, and zone data (any capitalization) has been renamed to "Tobin City" to match the game''s actual setting.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Six New Commands: consider, examine, sip, show, tell, whisper', '`examine <target>` is a synonym for "look at" something. `consider <target>` (or `consider self`) sizes up a fight before you start one. `sip <liquid>` tastes a puddle or fountain with much less risk than a full `drink`. `show <item> <person>` holds an item up for someone to see without giving it up. `tell <name> <message>` reaches anyone playing anywhere; `whisper <name> <message>` is the same but room-only, with bystanders only knowing a conversation happened.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'New Implementor Toy: egotrip blast', 'A new `egotrip blast <target>` command (60+) hits a target with a non-lethal bolt of lightning, halving their current HP (never below 1). The original Sneezy egotrip''s other eleven subcommands aren''t here yet -- they depend on systems Tobin hasn''t built (disease, garble, mob AI hate/aggro, and more).')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Combat Messages No Longer Show Raw Damage Numbers', 'Regular combat, spell, and trap messages now read like "You stab a messenger''s left finger." instead of "...for 4 damage!" -- cleaner flavor text for everyone. Immortals still see the number (useful for testing and balancing); mortals don''t.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Look At Someone To See What They''re Wearing', '`look <person>` now shows their worn equipment (body slots and both hands) right after their description, the same listing the `equipment` command shows you for yourself.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'New Autoloot Toggle', '`toggle autoloot` now exists: when on, defeating an opponent (or instakilling one as an immortal) automatically pulls everything out of their corpse and into your own inventory, instead of leaving it there to loot by hand.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Major Limbs Are Now Fatal, and Bigger Targets Get Hit More', 'Losing your head, neck, waist, or torso to combat damage is now instant death -- losing an arm, leg, finger, or foot still is not. Taking off the neck also takes the head with it. Which limb gets hit is no longer a flat coin-flip across all thirteen either: a torso is a much bigger target than a finger, so it (and the other bigger limbs) gets hit far more often, mirroring real Sneezy''s own body-part weighting.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'New Builder Tool: stat (55+)', '`stat obj|mob|room <vnum>` dumps every single field of that prototype''s row, plus its exits (for a room) or its hitroll/damroll/AC affects (for an object). Unlike `vnum`, which searches by name, `stat` needs an exact vnum -- but shows everything about it once you have one.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Immortals No Longer Take Damage in Combat', 'Landing a hit on an immortal still works exactly as before (verb, messaging, and all) -- it just always deals zero damage now. You can still spar with an immortal for testing purposes; you just can''t actually hurt one.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'stat Speaks Plain English Now', '`stat` no longer dumps raw numbers for the columns that are really flags or lookups: an object''s wear slots and a mob''s action flags now list as readable names ("[ SENTINEL ] [ SCAVENGER ]"), a mob''s class and race show as text ("Cleric", "GOBLIN") instead of their raw codes, and a room exit''s direction, door type, and condition read as words ("dir=north door=Door cond=Closed") instead of numbers. Faction/fact_perc are dropped from the mob dump entirely -- factions are not and will not be supported. The twelve raw attribute columns seeded on every mob are also trimmed down to just the six Tobin actually uses (strength, constitution, dexterity, intelligence, wisdom, charisma); the other six are real Sneezy data but nothing reads them, so showing them would be misleading.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'A save Command, and Quitting Now Saves You Too', 'A new `save` command persists your character on demand -- useful reassurance mid-session. More importantly, quitting (`quit!`) or dying now automatically saves everything first, closing a real gap: taking damage in a fight was never written to the database until the fight actually ended, so a quit or a disconnect mid-battle could quietly roll your HP back to whatever it was last saved at. That state is now captured the moment you leave, one way or another.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'stat Decodes Even More Now', '`stat obj` now shows `type` and `action_flag` as readable names instead of raw numbers, and `stat room` does the same for `sector` and `room_flag` -- the same decoding `look` and the room/object editors already relied on internally, just not previously surfaced by `stat`.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'toggle Sorted Into Categories', '`toggle`''s listing is now grouped under three headings -- Preferences, Prompt, and Communication -- instead of one flat list, making it quicker to find the switch you want as more of them get added.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'A Few More Huh?!s Silenced', 'Found five more spots that still answered with the old terse "Huh?!" instead of the friendlier unknown-command message: casting or praying as the wrong class, `purge linkdead` below level 58, and a Thief-only door trap command used by someone who never learned the skill. All five now match the rest of the game.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'rent Works Now: The Safe Way to Log Off', 'A new `rent` command stores your belongings and ends your session cleanly, the recommended way to leave the game -- simply dropping your connection has always carried a small element of risk. Rent out for a while and you will find yourself healed up when you return, roughly in proportion to how long you were away. Renting is refused mid-fight. Per-item storage cost and restricting rent to inns are not in yet -- both need systems that do not exist yet (money, and a decision about which rooms count as inns).')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'quit! Is Now the Risky Way to Leave', 'Now that `rent` exists as the safe way to log off, plain `quit!` has earned its reputation: everything you are carrying, wearing, or holding spills onto the floor right where you typed it, both you and anyone else in the room are told, and there it stays until someone -- possibly not you -- picks it back up. Gold is not affected, since there is no money system yet to have any.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Help Gets a Real Front Door', 'Two new topics -- `help playing` and `help administration` -- are now the front door to the whole help system: a first-timer sees a one-time nudge toward `help playing` right at character creation, and both `help` and `wizhelp`''s own footers point newcomers there too. `playing` walks a new player through looking around, talking, fighting, classes and skills, and the safe (`rent`) versus risky (`quit!`) way to leave. `administration` explains the immortal level ladder (51 through 60) by WHY each tier sits where it does, not just what unlocks -- including why the debug tools (`hurtlimb`, `aitick`, `stat`, `balance`, `egotrip`, `test`) exist at all: real combat, world ticks, and decay are slow and random by design, and these make them instant and deterministic for testing. Alongside that, every command that had gone entirely without a help entry -- `stat`, `save`, `rent`, `cast`, `pray`, `practice`, `skills`, `affects`, `consider`, `continue`, `examine`, `show`, `sip`, `tell`, `whisper`, `balance`, `egotrip`, `settrap`, `disarmtrap`, `hurtlimb`, `aitick`, `immort`, `test` -- now has one. Found and fixed a real bug chasing this down: the old edroom/edzone/edplayer/edhelp/ednews/edwiznews/edrules -> edit-<noun> rename from Session 21 was never actually safe to re-run -- it would collide with itself and abort partway through on a second deploy. Fixed for good.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'goto guildmaster/rent/surplus: Directions, Not Transfers', '`goto` gained three mortal-usable landmark forms: `goto guildmaster` reports walking directions to the nearest guildmaster of your own class, `goto rent` to the inn, and `goto surplus` to the surplus store -- none of these teleport you, they just tell you the shortest real way there, by actual room exits. Standing right there already just says so. Every other form of `goto` (a room vnum, or another player''s name) is still immortal-only, and still teleports as before.')
ON DUPLICATE KEY UPDATE `title` = `title`;

-- goto guildmaster was redesigned from a teleport into directions before
-- this ever shipped in a push (user follow-up, same day: "goto
-- guildmaster should give them directions, not transfer. also add a
-- goto rent, goto surplus") -- the INSERT above already carries the
-- corrected wording under a new title (for a fresh install, and for any
-- DB that hasn't seeded the old title yet); the earlier "teleports you
-- straight to" row was already live-seeded once this session under the
-- OLD title, so it's now a stale duplicate of the same announcement --
-- delete it rather than rename it (renaming would collide with the new
-- title the INSERT above just created, same class of bug as the
-- edroom-rename migration fixed earlier this session).
DELETE FROM `wiznews` WHERE `title` = 'goto guildmaster: Mortals Can Find Their Trainer Now'
  AND EXISTS (SELECT 1 FROM (SELECT title FROM wiznews) w
              WHERE w.title = 'goto guildmaster/rent/surplus: Directions, Not Transfers');

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Character Creation: Choose Race and Class First, and See Who You Are Becoming', 'Two changes to new-character creation. First, you now choose your race and class BEFORE allocating attributes, not after -- so your point-buy choices can be made knowing who you already are. Second, the race and class screens no longer show raw stat bonuses ("+2 Dex, -4 Con"); each entry is now a short, evocative description of what that race or class is actually like to play. The real mechanics behind the scenes have not changed at all -- only how they are presented before you have even started playing.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'stat player: See Everything About a Character', '`stat` now has a fourth form: `stat player <name>` dumps a player''s identity, level/experience/HP/alignment, and attributes, decoded the same way the object/mob/room forms already were -- class, race, and gender as readable words, plus an alignment tier alongside the raw number. Also fixed a real dispatch bug found while testing this batch of changes: typing the exact command `set` was occasionally landing on `settrap` instead, because of how commands are matched by table order -- fixed for good.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'open door <direction> Works Now', '`open`/`close` now accept the word "door" in front of a direction (`open door north`, `close door east`), matching the original game''s documented phrasing -- previously only the bare direction form worked. A bare `open door` with no direction opens the room''s one door, if it has exactly one.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Command List Reordered: Mortal Commands First', 'The `help`/`wizhelp` command tables were fully reordered so every mortal-usable command now sorts ahead of every immortal-only one -- less chance of an immortal fat-fingering a mortal command''s abbreviation into an immortal one by accident. Found and fixed two real abbreviation collisions while at it: `set` was occasionally landing on `settrap` instead, and after `goto` became mortal-visible, `g` started reaching it instead of `get`. Further alphabetizing within each tier is on hold pending a follow-up decision.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Game Clock and Regen Now Keep True Real-Time Pace', 'Fixed a timing bug in the server''s core pulse scheduler (`game_loop.c`): every periodic system -- HP regen, combat rounds, the game clock, mob AI, zone aging, puddle decay -- was gated by how many times the main loop happened to iterate, not by real elapsed time. Since a busy socket lets the loop iterate far faster than its normal 100ms tick, heavy connection traffic could make these systems fire much more often than intended (found while chasing a flaky trigger-damage test: an HP regen tick was firing within 1.5 real seconds instead of its intended ~5). The scheduler now tracks real wall-clock time directly, so every pulse-based system keeps its documented pace regardless of how busy the server is.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Command Table Alphabetized -- With Zero Change to What You Type', 'The command table is now alphabetized within each tier, finishing the reorder that was left on hold. Mortal commands still come first as a block, movement still sits pinned at the very top (n/e/s/w/u/d must always mean movement), and everything else is now in plain A-to-Z order -- so finding a command in the source is no longer a hunt. Crucially, NOT ONE abbreviation changed: `sa` still says, `sc` still scores, `dr` still drops, `a` still attacks, `c` still closes. That is not a hopeful claim -- the reorder was generated and then verified by a script that resolves all 432 possible abbreviations at every level against both the old and new tables and diffs them, and it reports zero differences. Sixteen pairs in the mortal tier and one in the immortal tier are deliberately left out of strict alphabetical order because pure A-to-Z would have handed a shared abbreviation to the wrong command (`say` must precede `save` so `sa` still speaks); each is marked in the source with the abbreviation it protects. Also corrected some long-stale comments in that file that claimed `c` reached `color` and `h` reached `help` -- `close` and `hit` had quietly owned those for a while.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Practice System Redesigned: Three Disciplines and Practice Points', 'The practice system has been rebuilt from the ground up. Players now earn practice points when they level up (random 6-8 base, scaled by Wisdom via the `balance wisdom` modifier). Points are spent at guildmasters to raise discipline percentages -- each point buys a random 1-2%. Three discipline tiers now exist: Basic (level-51 guildmasters), Combat (level-80 guildmasters), and Advanced (level-100 guildmasters). Advanced practice is locked until both Basic and Combat hit 100%. The `practice` command now accepts batch syntax (`practice combat 7` to spend multiple points at once). New mortal landmark: `goto combat` gives walking directions to the nearest Combat guildmaster. Combat-tier skills now require some Combat discipline before they can be used, just as Basic and Advanced skills already did. All three percentages are shown in `skills` output.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Practice System Polish: Status Always Visible, Smarter Aliases', 'Four small follow-ups to yesterday''s practice system redesign, found by playtesting. Bare `practice` (no arguments) now always shows your own discipline percentages and practice points, even with no guildmaster in the room -- it is your own character sheet, not something you should need to ask a guildmaster for. When a guildmaster IS present, the status line now only invites training in the discipline that particular guildmaster actually teaches, instead of suggesting all three regardless of tier. `practice <yourclassname>` (e.g. `practice warrior`) now works as a synonym for `practice basic`, matching how `skills` already labels that tier by class name ("Warrior Skills"). And a new `goto <classname>` (e.g. `goto thief`) gives walking directions to that NAMED class''s own Basic guildmaster, not just your own class -- handy for immortals and curious players checking on another class''s trainer without switching characters.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', '`skills`/`bug`/`idea`/`rules` Now Page Their Output', 'These four commands could print more than a screenful in one shot with no way to slow it down -- `skills` especially, now that it lists three discipline percentages plus every tier. All four now go through the same pager `news`/`wiznews` already use: one screen at a time, ENTER for more, Q to stop. `skills` for an immortal (every class''s full roster) and the outstanding `bug`/`idea` lists benefit the most, but any of the four can grow past one screen as the game''s content grows, so all were switched over together.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Real Per-Skill Proficiency: Learn By Doing, Sneezy-Style', 'Discipline percentages (Basic/Combat/Advanced) have only ever gated ACCESS to a whole tier -- once unlocked, every spell in that tier always worked. That is no longer the whole story. Every individual skill and spell now has its OWN 0-100% proficiency, separate from the discipline gate, that climbs with actual use -- the classic "learn by doing" mechanic, ported from Sneezy''s own system. A freshly-accessible spell starts barely competent (1%) and climbs toward a ceiling set by your discipline percentage for that tier every time you attempt it, win or lose -- so practicing the discipline raises the CEILING, and using the skill climbs TOWARD it. Higher Wisdom softens the diminishing-returns curve, letting gains stay likely further into the climb. Proficiency now genuinely matters: `cast`/`pray` roll against it for every attempt (a low-proficiency spell often fizzles -- "You fumble the casting..." -- a well-practiced one rarely does), and `settrap`/`disarmtrap` and dual wield hook in the same way. `skills` now shows each known skill''s proficiency in brackets, e.g. "bash [34%]". Immortals are unaffected -- they still bypass every gate outright.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', '`practice <discipline>` Now Shows the Skill List Anywhere', '`practice combat` used to require standing at a Combat guildmaster just to attempt spending a point, and refused outright everywhere else -- not very useful for checking your own progress. Bare `practice <discipline>` (no count) is now a look-anywhere status view: it lists every skill/spell in that ONE discipline along with each one''s own individual proficiency percentage, no guildmaster required. Standing in front of the matching guildmaster adds a reminder to actually spend points there. Only an EXPLICIT count (`practice combat 5`) still spends practice points, and that form still needs the right guildmaster present, unchanged. Net effect: `practice basic`/`practice combat`/`practice advanced` are now readable any time, and only spending requires the trip.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', '`who` Now Reports Active Links, Linkdead, and Total Players', '`who` always ends with a summary line now: how many connections are actively linked, how many characters are sitting linkdead in the world (disconnected without a clean `quit!`/`rent`), and the total. Shows regardless of any filter (`who imm`/`who mort`/`who <name>`) since it''s a server-health stat, not a scoped listing. Built while investigating a pre-existing connection-handling issue (see the relevant TODO entry) to make linkdead accumulation directly observable instead of guessed at -- `purge linkdead` (Administrator+) already existed to clean them up, this just makes it obvious when there''s something to clean.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Connection Investigation Closed (False Alarm) + a Real Output-Buffering Fix', 'The "connection silently stalls" issue chased for most of today turned out not to be a server bug at all -- it was a test script correctly taking 45+ seconds to run (creating five characters, each needing about nine slow-by-design network round trips) while the diagnostic scripts checking on it were only willing to wait 15-25 seconds before calling it stuck. Re-run with a realistic timeout, the full test passed clean. While chasing it, though, a real (if apparently never-yet-triggered) bug in the networking code got found and fixed: outgoing data could, in rare circumstances, be silently dropped if a connection''s send buffer was momentarily full, with neither side ever finding out. Every connection now has a proper backlog that retries automatically until the data actually goes out, including across a `copyover`.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Boxed Account Menu + a New Login Banner', 'Reaching the account menu used to dump your full character list immediately, with a repeated block of C/N/D/X/Q instructions underneath every single time. It now opens with a clean boxed menu instead -- just the five letter options -- and only lists your characters by name once you actually press C, at which point it shows them numbered and asks which one to connect. A single-character account still connects straight through on C, same as before. The connection screen ahead of login also has a new look: a fortress gate now stands above the TobinMUD logo, greeting every new arrival before they even give an account name.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Boxed Character Creation Screens', 'The revealed character list at the account menu is now boxed too, with each number in bright cyan. New character creation got the same treatment: the race, class, and alignment screens each now present their full option list inside a bordered box, with the option numbers colorized to match. The attribute point-buy screen is boxed now too.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Long Command Output Now Pages Automatically', 'Any command whose reply runs past about 20 lines now shows it a page at a time (ENTER for more, Q to stop) instead of dumping it all at once -- the same behavior `news`/`wiznews`/`skills` already had. Covers `help`, `wizhelp`, `who`, `users`, `stat`, `inventory`, looking inside a well-stocked container, `log list`, and `edit trigger list`. Short output is completely unaffected -- no pager prompt appears unless there is actually more than one page to show.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', '`pee` Now Takes a Liquid Type', 'Bare `pee` still works exactly as before. `pee <liquid>` -- water, wine, beer, or acid so far, abbreviations welcome -- leaves a puddle of that liquid instead. A second pee of the same liquid grows the existing puddle bigger; a different liquid starts its own puddle alongside it.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'New: Define Your Own Command Aliases', '`alias <name> <expansion>` lets you set up your own shortcuts -- `alias k kill` means typing `k orc` sends `kill orc`. Bare `alias` lists what you have, `alias <name>` shows one, `alias remove <name>` deletes one. Aliases live on your account and follow you to every character you play, but mortal and immortal aliases are kept separate -- an immortal alias only works while playing an immortal character, even on the same account. Up to 20 per tier. See `help alias`.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Gold and Shops Have Arrived', 'Killing something now has a payoff: defeated creatures hand their killer gold on the spot, scaled to their level. Check your purse with `score`. All 264 of the game''s original shops are open for business too -- find a shopkeeper, `list` their wares, `buy <item>` or `sell <item>` (only certain goods, matching what each shop actually deals in). Every shop keeps its own personality in its buy/sell messages, same as the original game, just counted in gold now instead of talens. Not built yet: an in-game shop editor, and the old seeded piles of treasure lying around the world do not yet convert to gold when picked up -- both are on the list.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Treasure Piles Now Pay Out', 'Those old piles of gold scattered around the world -- treasure hoards, quest rewards, that sort of thing -- actually do something now. Pick one up and its worth goes straight into your purse instead of sitting in your inventory as a prop; check `score` to see it land.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'A Kinder Way to Close Up Shop', '`shutdown` is here, Implementor only. Bare `shutdown` counts down from 5 seconds, warning everyone along the way; `shutdown 120` (or any number of seconds) does the same over a longer window without freezing the game for anyone in the meantime. Every connected character is saved right before the world actually closes. Started one by mistake? `shutdown cancel` calls it off.'),
('The TobinMUD Team', 'Shop Listings Are Numbered', '`list` at a shop now numbers every item alongside its price. `buy 3` buys the third thing listed, same as typing its name -- whichever is quicker.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'The Mystery Tag Solved', 'A few old items and rooms had a stray `<h>` sitting in their text where a name was supposed to go -- leftover from the original game''s template system, never carried over to this port. It now fills in as "TobinMUD" wherever it appears, the way it always meant to. A handful of other leftover mentions of the old game''s name, scattered through a book, a couple of rooms, and an item description, are now TobinMUD too.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', '`wipe` Arrives -- Administrator Only', 'The most permanent command in the game is here: `wipe <name> <password>` erases a character for good, and `wipe account <name> <password>` takes the whole account with it, every character on it included. It needs the wipe master password to run at all (ask whoever holds it), and only reaches someone below your own level. There is no undo, so it earns its own confirmation: getting the password wrong just fails quietly, no second-guessing needed on our end.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `wiznews` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Dig Your Own Way', '`dig <direction>` is here for builders. Walk into a direction with no exit yet and it creates a brand new room right then, wires it back to where you were standing, and steps you straight through -- picking the lowest free room number in your own zone automatically. The new room starts bare; `edit room` gives it real shape.'),
('The TobinMUD Team', 'Account Editing for Admins', '`edit account <name>` lets an Administrator rename any account, reset its password, or see who''s on it and at what level. Every change lands immediately, no Save step to remember.')
ON DUPLICATE KEY UPDATE `title` = `title`;
