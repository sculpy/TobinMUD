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
