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

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Gold and Shops Have Arrived',
 'Killing something now has a payoff: defeated creatures hand their killer gold on the spot. Check your purse with score. Shops across the world are open for business too -- find a shopkeeper, list their wares, buy or sell (only certain goods, matching what each shop actually deals in). Every shop keeps its own personality in its buy and sell messages. Old piles of treasure lying around the world pay out in gold when picked up, same as anything else.'),
('The TobinMUD Team', 'Numbered Shopping', 'list at a shop now numbers every item alongside its price -- buy by number, or by name, whichever is quicker.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Sickness Is Now a Thing',
 'Drink from a puddle on the ground and you might come away with more than a poison scare -- there is a real chance of catching a nasty cold, or something considerably worse. A disease saps a little strength every so often while it runs its course, then wears off on its own -- or get it cured at a hospital, see below. NPCs can catch these too, and will visibly wince from a flare-up same as anyone else.'),
('The TobinMUD Team', 'A Poison That Actually Lingers',
 'Drinking something foul now leaves you genuinely poisoned for a while instead of just a single jolt of pain -- check affects to see what is wrong with you and how long it will last.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Hospitals Open Their Doors',
 'Real hospitals are now staffed and ready in several of the world''s major cities. Walk up to the doctor and list to see what ails you -- damaged limbs, active diseases, even poison -- then buy the cure on the spot for gold. Lost track of where the nearest one is? goto hospital points the way from anywhere in the world.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Component and Commodity Shops Are Finally Stocked',
 'Mages and Druids: the components shop in the Market Place now actually sells spellcasting components, so casting works again with something to buy rather than nothing to find. A handful of sister shops around the world now carry real metal bars, ingots, and animal hides too -- all buyable and re-sellable, same as any other goods.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Bulletin Boards Are Live',
 'The boards mounted on walls across the world finally do something. read at one lists what is posted; read followed by a number shows a message in full. write followed by a subject and a message posts a new one on the spot. If a room happens to have more than one board, name which one you mean.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Look Can Count Too',
 'kill and get have long been able to reach a second or third match when more than one thing shares a name -- look can now do the same, and so can drink, sip, open, show, and sell. Point at a specific one instead of always the first.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Torches and Lanterns Actually Work Now',
 'light and extinguish turn a light source on or off; refuel tops one up from a fuel item you are carrying. A lit light burns down over time and eventually goes out on its own, so refuel it before then -- though a plain torch can never be refueled, only replaced. A lit light source shows as lit wherever it is sitting so you can actually tell. And keep an eye out after dark: the streetlamps in a few of the world''s cities are tended once again.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'A Few Small Conveniences',
 'prompt can now show your gold alongside your hit points. point mentions whatever you are holding instead of always pointing around randomly. And toggle pk is here for anyone who wants to fight other players -- both sides have to opt in, so nobody is dragged into a fight they did not choose.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'A Tip a Day',
 'Type tips any time for a random piece of gameplay advice. If you are still on the newbie channel you will also get one every so often without asking.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Spells and Prayers Do More Now',
 'Cure poison and cure disease now genuinely cure you (or, for a prayer, someone you name). A family of armor, shield, and resistance spells and prayers now really do protect you, not just say so. And a Cleric locked in a fight can call down poison or disease on their foe for real. The skills list now tells you right up front, for every spell or prayer, whether you will need a component or a holy symbol to cast it.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Help Now Covers Every Skill and Spell',
 'Type help followed by any skill or spell name and you will get a real writeup instead of nothing at all -- what it does, which classes have it, and what you need on hand to use it. And engage now works as another word for hit.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'A Smoother Path to Every Skill',
 'The levels at which skills and spells unlock have been rebalanced across the board. Basic and Combat training now both wrap up well before the halfway mark of a mortal lifetime, and everything in the Advanced tier is reachable somewhere in the second half. Nothing was hidden behind an unreachable level anymore.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Spell Components Go Further, Holy Symbols Wear Down',
 'A spell component is no longer gone the instant you use it -- each one has real charges and lasts through a good handful of castings before it is spent. A holy symbol works differently: it genuinely wears down a little with every prayer, and eventually shatters, so keep a spare in your pack.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Never Miss an Announcement Again',
 'If something has been posted to the news since you last checked, you will now see a quick reminder the moment you log in. Read it whenever suits you -- the reminder clears itself once you do.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Thieves Can Now Peek',
 'A Thief who knows the skill can attempt to peek at what someone nearby is carrying, without them ever knowing -- unless the attempt goes badly, in which case they will feel like someone just tried.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Locks Finally Mean Something',
 'Locked doors and chests were always around, but nothing could open them -- until now. Track down the right key and use `lock`/`unlock` to work a locked door or container. No need to name the key, just carry it; the wrong one simply will not turn.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'To the Victor Go the Spoils',
 'PvP now has real stakes: if you and another consenting player (`toggle pk`) fight it out and you win, their gold is now yours. Splitting the take across a group is still on the way -- for now the whole purse goes to whoever lands the winning blow.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Peace and Quiet, On Demand',
 'Tired of someone spamming your tells? `ignore <name>` blocks their tells and whispers from ever reaching you -- they will not even know it happened. `unignore <name>` lets them back in whenever you are ready.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Tips Have Their Own Off Switch',
 'The periodic tip messages used to be tied to the newbie help channel -- turning them off meant leaving that channel entirely. Now `toggle tips` mutes just the tips, leaving everything else alone.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Wondering How Close You Are to Leveling?',
 'The new `level` command tells you exactly that: how much experience you have, and how much more you need before your next level.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'A Fuller Prompt',
 '`prompt` now offers two more stats to show alongside HP, gold, and vitality: your current experience, and how much more you need to level. `prompt all` turns every available one on at once.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Ordinary Animals Do Not Carry Coin Purses',
 'Killing a mundane creature -- a rat, a bear, a bird, and the like -- no longer hands you gold; that never made much sense for an animal to be carrying. Dragons, orcs, and other fantastical foes are unaffected. Experience is unaffected either way.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Saddle Up',
 'Mounts have arrived. Find a loose horse and `ride` it, or buy one from the stable at Carnivorous Companions. A mount lets you cover ground faster and fights with you at your side -- `dismount` when you are done, though stepping indoors will do that for you automatically.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'A Whole Lot More Ways to Express Yourself',
 'The `socials` list just grew enormously -- smile, wave, and the old familiar faces are joined by a huge cast of new emotes pulled straight from the game''s roots. Aim any of them at someone (`grin Bob`) or use them on their own; a few, like `point`, even notice what you are holding. Try targeting yourself for a couple of them -- some now have a special reaction just for that. The list is long enough to page through now, so `socials` will prompt you to keep reading.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'New Fight Moves: bash, kick, and disarm',
 'Mid-fight, try `bash <target>` (Warriors), `kick <target>` (Thieves and Monks), or `disarm <target>` (Warriors, Thieves, and Monks) for a little extra edge on top of your normal swings. A solid bash knocks your target down and costs them a beat to recover; a good kick lands extra damage; a clean disarm knocks the weapon right out of their hand. All three cost you a moment to recover too, whether they land or not, so use them wisely. Warriors also now parry incoming blows automatically -- no command needed, just a passive chance to block a hit outright that grows as you fight.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'A Long Disconnect Now Cleans Up After Itself',
 'If your connection drops and you do not make it back within about 5 minutes, your character now saves and steps off stage on its own, rather than sitting frozen in the world indefinitely. Reconnecting after that window picks up fresh from your last save, same as normal -- nothing is lost, it just will not still be standing exactly where you left it.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Spellcasters Hit Harder, and Smarter',
 'Attack spells and prayers now scale with how powerful the spell itself is, not a single flat amount no matter what you cast. You can also aim `cast` at a specific target the same way `pray` always let you, and either one can now open a fight with someone on its own instead of only ever working on whoever you happened to already be fighting. And several spells that always described themselves as striking everyone nearby -- fireball and the like -- finally do exactly that.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Some Gear Is Actually Magical Now, and Scrolls/Wands/Staves Work',
 'Some rings, jewelry, and other wearable gear carry a real magical bonus -- a boost to one of your stats, your Armor Class, your health, or your stamina -- the moment you put it on, and it goes away the moment you take it off. Check `identify` before wearing something new to see what it actually does. Scrolls, wands, and staves are also usable now with the new `use` command: a scroll works once and crumbles away, a wand can be aimed at a target and has a limited number of charges before it goes quiet, and a staff unleashes its effect on everyone around you at once. Anyone can use one of these, whatever your class.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Wear and Tear Is Real Now',
 'Nothing lasts forever anymore. A corpse left lying around will eventually decay away on its own instead of sitting there forever, and the same goes for a severed limb. Gear takes a beating too -- a hard enough fight can actually break something you are wearing, leaving nothing but ruined scraps behind, so keep an eye on how your equipment is holding up and do not get too attached to anything fragile.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Broken Gear Can Be Repaired Now',
 'Warriors can learn a `repair` skill (level 5) to mend their own damaged equipment on the spot for a bit of gold in makeshift materials -- just type `repair <item>`. If you would rather have it done properly, take it to the Blacksmith''s Forge: `submit <item>` hands it over for a claim ticket quoting the price, `tickets` lists what you have waiting there, and `retrieve <ticket number>` pays up and hands the fixed item back, good as new. One thing to know: every repair, whether you do it yourself or pay a smith, wears an item down a little for good -- patch the same piece enough times and it will not be repairable to quite as good a condition as it started. Anyone can submit/retrieve at a repair shop; only Warriors can repair their own gear in the field.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Banking Has Arrived, and Shops Now Charge Tax',
 'The Tobin City First Kingdom Bank will now hold your gold for you. Visit and use `bank` to check your balance, `bank deposit <amount>` to put gold away safely, and `bank withdraw <amount>` to take it back out. Gold left in the bank earns a little interest every day, even while you are offline, so it is worth parking some there rather than carrying everything around. One catch: ordinary shop purchases now come with a small sales tax on top of the sticker price, which goes straight into the crown''s coffers -- something to budget for next time you go shopping.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'What Your Gear Is Made Of Actually Matters Now',
 'Every weapon and piece of armor has a real material behind it -- and now it shows. Steel, silver, mithril, and dozens of others are grouped into five broad tiers: Common, Fine, Superior, Rare, and Legendary. A higher tier hits harder, blocks more, holds up longer before it needs repair, and is worth more gold at a shop. Check `identify` on anything you are curious about to see its Material line. Your `inventory` and `equipment` also now show each item''s condition right next to its name, in color -- brand new, worn, or anywhere down to destroyed -- so you can tell at a glance what needs attention. And combat itself reads better: instead of a raw damage number, you will see how hard a hit actually landed, from "pathetically" all the way up to "into shreds."')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'A Quiet Way to Talk: Sign Language',
 'Everyone can now `sign` a message to the room instead of speaking it out loud -- handy when you would rather not be overheard. It takes both hands free and takes a moment to pick up, so a brand-new adventurer will need a little seasoning before it clicks. Only someone else who also knows how to sign actually understands you; everyone else just sees you gesturing. Word on the street is thieves have their own hand-talk that anyone can read, trained or not.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Some Habits Are Hard to Shake',
 'Pipeweed, opium, pot, and frogslime are all real, usable substances now -- `smoke` a dose from any of them for a genuine, temporary effect on how you feel. Hobbits in particular find pipeweed clears the mind wonderfully; everyone else finds it leaves them a little worse for wear. Whatever you choose, dose too often and you will start to feel it when you go too long without another one.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'A Grimmer, Wittier Way to Go',
 'Dying now hits a little differently: the word DEAD in your death message stands out in color, and the world-wide announcement of your demise has a handful of new, cheekier ways to break the news to everyone else. Sorry in advance.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Score Has a New Look',
 '`score` now lays everything out in a tidy grid instead of one long column -- your level, race, class, gold, HP, and stats all at a glance. Your age now genuinely climbs over time instead of just tracking how recently you were born, and casters see their class''s own resource named properly (Piety for the faithful, Lifeforce for druids, Mana for the rest).')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Summon a Companion to Fight and Follow You',
 'Mages can now `cast conjure elemental air/earth/fire/water` to call a loyal elemental to their side, Clerics can `pray summon swarm` for a cloud of locusts, and Druids can `cast animal companion` for a loyal beast. Whatever you summon will follow you from room to room and join you in a fight -- and it actually listens: say something like "attack guard" and it will turn on that target, or say "stop" to call it off. Try other things too -- say "dance" and see what happens. Just don''t be surprised if it looks confused and ignores you every so often; charmed creatures only listen so well. `dismiss` sends your companion away early, and the bond fades on its own after a while regardless. You can only keep one companion charmed at a time.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Become Someone -- or Something -- Else', 'Mages can now `cast polymorph` to twist their body into a brown bear for a while -- full strength of the new form, reverting on its own after a time (or early with `return`). Thieves can `disguise` themselves as a hooded stranger instead, hiding who they are from anyone else in the room -- `disguise` again to drop the act.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Grow Your Own -- Planting Has Arrived', 'Find a sack of seeds and `plant` it outdoors to start a real garden -- tomatoes, roses, apple and orange trees, a money tree, pipeweed, pumpkins, turnips, lettuce, pot, catnip, candy heart trees, and gray grapes, fifteen crops in all. Dig a hole, sow the seeds, cover it up, and watch it grow over time from a bare mound of dirt into a mature, fruit-bearing plant. Thieves have their own use for the word: `plant <item> <victim>` secretly slips something into someone else''s pocket instead of picking one.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Live Off the Land -- Skinning, Butchering, and Foraging', 'Druids can now put a fresh kill to use: `skin` a corpse for a hide, or `butcher` it for a raw steak -- each corpse gives up one of each, once. Away from a fight, `forage` gathers a bit of wild food from the terrain around you outdoors; give it a little while between attempts.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Inventory Now Stacks Identical Items', '`inventory` no longer lists three separate lines for three of the same thing -- identical items now group together with a count, like "a small sack of tomato seeds (x3)".')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Watch Your Step -- Falling Is Now Dangerous', 'Wandering into open sky with nothing but air underfoot will now send you plummeting, and the landing can genuinely hurt or even kill you depending on how far you fall. Monks trained in catfall land much softer. Monks trained in catleap can now leap across open space in any direction, gliding safely over ground they normally could not cross.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Meditation Skills Now Actually Do Something', 'Meditate (Mage, Druid) and penance (Cleric) now genuinely restore your vitality when used, same as yoginsa always has for Monks -- Druids also get their own meditate for the first time. Yoginsa itself will now sit you down automatically if you were standing, instead of making you sit first yourself.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Skill Gains Are Announced', 'You will now be told the moment any skill or spell you practice actually improves -- no more wondering whether you got any better from all that use.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Vanish From Sight', 'Mages can now `cast invisibility` on themselves or an ally, fading from view and out of reach of anyone trying to target them by name -- only an immortal can still see or target someone who has turned invisible. `cast dispel invisible` turns them visible again.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Leveling and Toughness Rebalanced', 'The amount of experience needed to gain a level, and how much tougher each level makes you, have both been reworked to better match the game''s intended design. Hardier classes like Warriors will now feel noticeably tougher as they climb in level compared to a spellcaster of the same rank.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'A Corner of the World Now Stays Lit', 'A cluster of early rooms will now always stay lit, so you won''t need a light source or wait for daytime to see clearly there.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Blindness and a Way Home', 'Clerics can now `pray blindness` on a foe, leaving them unable to look at anything and swinging far less accurately until it wears off. Clerics -- and Druids, once experienced enough -- can also `pray word of recall` to pull themselves or a willing ally straight back to Center Square, though a few especially defended places refuse to let anyone leave that way.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Meditation Takes Care of Itself', 'Yoginsa no longer needs to be re-cast over and over -- once you start meditating, it keeps quietly restoring you tick after tick until you stop, get up, or get pulled into a fight.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Spoils Now Wait On The Body', 'Gold from a defeated foe no longer lands straight in your pocket -- it''s left behind in a lootable pile on their corpse, same as anything else they were carrying.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'A Finer Countdown', 'The recovery countdown that shows in your prompt while you''re still catching your breath from a swing now counts down in tenths of a second instead of whole ones.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Drawing Their Ire', 'Warriors can now `taunt` a foe to pull its attention off whoever it''s fighting and onto themselves instead. Clerics can also `pray paralyze limb` on a foe, leaving one of their arms or legs unresponsive until it''s healed.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'One Body, One Login', 'Logging back into a character that''s already active elsewhere no longer creates a second copy of them wandering the world -- the old connection is bumped, and the new one picks up right where they were.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'The World Now Remembers Itself Through A Reboot', 'A copyover used to quietly reset every room to its original starting contents -- anything dropped, loaded, or otherwise left lying around would vanish the next time someone walked in. Loose items and creatures in rooms now survive a copyover along with everyone''s connections.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Doors Open And Close From Both Sides Now', 'A door used to remember whether it was open or closed only from whichever side you last touched it -- the other side kept its own separate state entirely. Now, wherever a door genuinely exists on both sides, opening or closing it from either room affects the whole door.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Groups Move, Talk, And Fight Together Now', 'Followers now walk right along with the person they''re following instead of being left behind in the last room. A new `gtell` command (or its short form, `gt`) lets everyone in your group talk to each other no matter where they are. And `assist <name>` lets you jump into a groupmate''s fight and help them take down whoever they''re up against.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Fights Are Less Gruesome Now', 'Limbs hold up better in a fight than they used to -- they take damage at half the pace of your overall health now, so a wound has to build up longer before it turns serious, and blood pools form less often as a result. Overall combat -- how hard you hit, how fast a fight ends -- hasn''t changed at all.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Wear Everything At Once', '`wear all` now equips every wearable item you''re carrying in one go, instead of typing `wear` over and over for each piece. Anything that can''t be worn (a weapon that needs `wield`, a slot you''ve already filled) is just quietly skipped.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Newbie Gear Doesn''t Litter The Ground Anymore', 'Starting equipment issued to new characters is meant to get you on your feet, not to be sold or hoarded. If you drop a piece of it (and it''s not stuffed full of something else), it now vanishes in a flash of white light instead of sitting on the floor.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Some Gear Just Isn''t Made For Your Kind', 'A few items in the world are built for a specific race and won''t fit anyone else -- try to wear, wield, or hold one that''s not made for you and you''ll be turned away with a clear reason why.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'New `uptime` Command', '`uptime` shows when the server last came back up (a fresh boot or a copyover) and how long it''s been running since then.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'New Characters Start With More', 'Fresh characters now arrive with a full starting outfit for their race (a matching set of clothing plus a racial weapon and a training shield), on top of their class''s own gear. Everyone also starts with a few rations of food and a small waterskin. Mages and Druids begin with a spellpouch stocked with real spell components, and Clerics start carrying a few wooden holy symbols.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'New `whittle` Command', 'Anyone can now whittle simple wooden items -- chairs, chests, boxes, rings, pipes, daggers, walking sticks, idols, and more -- from carried wood logs. You''ll need a weapon wielded and enough wood on hand; type `whittle` alone to see the full list of what you can make.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Choose Your Homeland at Creation', 'Character creation''s options menu now lets you choose a homeland -- where your character actually grew up: a city (sharper and more charismatic, but softer), a farming village (practical and sure-footed), or the frontier wilds (tough and strong, but blunt). It''s entirely optional -- skip it and your character simply has no homeland, no bonus or penalty either way. If chosen, it''s permanent and shows up in your `score` alongside your race and class.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Fresh Characters Now Get a Full Matching Pair', 'New characters were only ever given one sleeve, one bracelet, one glove, one legging, and one boot from their race''s starting gear -- covering just one arm, wrist, hand, leg, or foot and leaving the other bare. Fresh characters now start with two of each, enough to properly outfit both sides.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'New Mage Spell: Eyes of Fertuman', 'Mages can now cast eyes of Fertuman to search far and wide for a person or item by name, learning what room they''re in.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Experience Now Earned as You Fight', 'Killing a tough opponent used to pay out all your experience in one lump at the very end. Now you earn it hit by hit as the fight goes on -- you can even level up mid-fight -- with a single summary line telling you the total once the fight is over.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'A New Combat Trainer Near Town', 'A combat trainer has set up shop on Perimeter Road, right where it meets the East King''s Road at the edge of town.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'The Welfare Worker Now Greets You With What She Has', 'Walk into the Tobin City Welfare Department and the social worker there will now greet you with a list of the gear she can set you up with for your class -- just ask her for gear to actually receive it.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Faster Vitality Recovery, Limbs Command Speaks Plainly', 'Vitality now recovers 25% faster while resting. The `limbs` command now describes each limb''s health in plain words (perfect, hurt, near death, and so on) instead of a bare percentage.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Combat Discipline Refocused; Warriors Learn to Focus', 'Combat discipline now covers weapon and barehand proficiency alone -- every other combat skill has settled into Basic or Advanced discipline instead. Warriors also now occasionally focus their strikes automatically mid-fight, landing a more precise, harder-hitting blow without needing to do anything special to trigger it.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Fighting Wears You Out; Looting Tells You What You Got', 'Ongoing fights now gradually drain your vitality as the fight wears on, on top of any damage taken. Warriors'' automatic focused strikes now land less often than before. And turning on auto-loot no longer just tells you a corpse was looted -- it now lists out exactly what gold and items you picked up.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Bash Now Requires a Shield', 'You now need to be holding a shield to bash an opponent -- it will no longer work bare-handed or with a weapon alone.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Resting and Meditating Now Stand You Back Up', 'Once resting, sitting, or meditating brings you back to full health and vitality, you now stand back up automatically instead of staying seated.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Combat Fatigue Eased Further', 'The vitality drain from ongoing combat has been reduced again, on top of last update''s tuning.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Weapon Proficiency Now Rises With Your Combat Training', 'Slashing, blunt, piercing, unarmed, and ranged proficiency now automatically rise alongside your combat discipline as you train with a guildmaster -- no more separate grinding needed, and every class can now access all five.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'See Your Condition and Your Opponent''s While Fighting', 'Every round of combat now tells you your own condition and your opponent''s, in plain words like "good" or "wounded" rather than a raw number.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Bank Balance Now Shown on Score', 'Your score sheet now shows how much gold you have in the bank alongside what you''re carrying.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Leveling Up Now Restores Your Vitality Too', 'Gaining a level now fully refills your vitality along with raising its maximum, the same way it already does for health.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Remove Hold Works in the Dark', 'You can now type "remove hold" to remove whatever you''re holding, even in the dark where you can''t see what it is -- it goes straight back into your inventory.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Light Sources Now Visible in the Dark', 'A lit torch or other light source is now visible even in a pitch-black room, whether it''s lying on the ground or being carried by someone else nearby.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Fights Now Survive a Copyover', 'An ongoing fight now continues after the world reboots for an update, instead of quietly ending.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Monks Learn Kick Two Levels Earlier', 'Kick is now available to Monks starting at level 1, same as Thieves already had it, instead of level 3.'),
('The TobinMUD Team', 'Selling to a Shop No Longer Just Vanishes', 'Selling an item to a shopkeeper now takes a small sales tax out of what you are paid, but the shopkeeper keeps what you sold -- it shows up in that shop''s listing, marked as used, for anyone (including you) to buy back later.'),
('The TobinMUD Team', 'Some Mobs Now Sell One-Way Tickets', 'A few seeded characters carry a hidden trade: buy ticket from one of them, standing and with enough gold, and they will send you somewhere else entirely.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Warriors Can Now Kick', 'Kick is now available to Warriors from level 1, joining Thieves and Monks -- every class that isn''t a spellcaster now has it.'),
('The TobinMUD Team', 'Monks: Master Kick, and Advanced Kicking Takes Over', 'A Monk whose kick has reached 100% proficiency now automatically gains the benefit of advanced kicking -- a chance at a bonus strike each unarmed round -- even before reaching the level it''s normally taught at.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Wild Boars and Other Tusked Mobs Can Now Gore You', 'Some mobs -- wild boars among them -- can now attempt a goring attack mid-fight: a solid hit knocks you to the ground, though most of the time they just miss with a bit of flavor instead.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Three New Cross-Class Skills: Toughness, Focused Avoidance, Evaluate', 'Every class can now learn three new advanced-tier skills. Toughness makes you genuinely harder to hurt in a fight, the more you practice it. Focused avoidance gives you a real, growing knack for dodging incoming blows. And evaluate lets you appraise an item''s worth -- the more skilled you are, the more you can tell about its price, condition, and what it''s made of. A new `evaluate <item>` command goes with it.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Combat Now Has Music -- On Supported Clients', 'Fighting now triggers a random music track on clients that understand the MSP protocol (like the new TobinMUD Client, or Mudlet), and it stops automatically once the fight ends. Nothing changes for plain telnet clients.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Spellcasting Mobs Now Carry Real Spellbags', 'Mages and Druids you fight now carry a proper spellbag sized to their own strength, holding a real spell reagent inside -- loot it same as any other belongings.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Casting and Praying Now Show Real Flavor Text', 'Every cast and prayer now shows a gesture, a few spoken words, and a moment where you feel the magic take shape, before its effect happens -- more of the feel of actually working the spell, not just an instant result.')
ON DUPLICATE KEY UPDATE `title` = `title`;

INSERT INTO `news` (`author`, `title`, `body`) VALUES
('The TobinMUD Team', 'Mages Now Have a Real Mana Pool', 'Mages finally have a real mana pool, shown in score and toggleable into your prompt with `prompt mana`. Every spell has its own real cost drawn straight from the classic game, and running dry means you can''t cast until you recover. `meditate` is no longer something you cast -- it''s its own command now, just like `yoginsa`: it sits you down and restores your mana over time on its own.')
ON DUPLICATE KEY UPDATE `title` = `title`;
