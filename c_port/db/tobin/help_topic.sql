-- Help topics for the Tobin c_port's `help <topic>` command, editable
-- in-game via `hedit` (immortal level 56+). Same schema-only pattern as
-- player_attrs.sql / player_progress.sql (new-for-Tobin, not part of the
-- original's seed data -- the original used flatfiles under lib/help/).
--
-- The seed INSERTs below use ON DUPLICATE KEY UPDATE name=name (a no-op)
-- so re-running this file never clobbers in-game edits.

CREATE TABLE IF NOT EXISTS `help_topic` (
  `name` varchar(32) NOT NULL,
  `body` text NOT NULL,
  `updated_by` varchar(64) NOT NULL DEFAULT '',
  `updated_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT INTO `help_topic` (`name`, `body`, `updated_by`) VALUES
('look', 'Usage: look\n\nShows the room you are in: its name, description, and everyone\nstanding there with you. You also look automatically whenever you\nenter the world. The name and description are tinted by the room''s\nterrain -- a snowy waste reads differently from a jungle or a city\nstreet.', 'seed'),
('who', 'Usage: who [name|immortals|mortals]\n\nLists everyone currently playing, with their level (or immortal rank\ntitle) shown in brackets before their name and any personal title after\nit. With an argument, filters the list: `who imm` shows only immortals,\n`who mort` only mortals, and any other word matches part of a name.', 'seed'),
('title', 'Usage: title [text]\n\nSets the free-form title shown in who. Normally it trails your name\n("Yourname the Brave"). Include <N> anywhere and it is replaced by your\nname, and the title is shown on its own -- e.g.\n`title You are not paranoid, <N> really is out to get you!`. `title`\nwith no text, or `title none`, clears it. Saved with your character.', 'seed'),
('toggle', 'Usage: toggle [name]\n\nWith no argument, lists your on/off switches and their current values.\n`toggle <name>` flips one (abbreviations welcome). Player toggles like\ncolor and hp affect only you; game toggles like multiplay are global\nand only 55+ immortals may change them.', 'seed'),
('score', 'Usage: score\n\nShows your character sheet: name, level, experience, hit points,\nposition, attributes, handedness, gender, and appearance. Limbs appear\nhere only once they are hurt -- see `help limbs` for the full breakdown.', 'seed'),
('gender', 'Chosen during character creation: `gender male`, `gender female`, or\n`gender neuter` (the default). Your gender sets the pronouns the game\nuses for you -- he/she/it, him/her/it, his/her/its. It shows on your\nscore sheet.', 'seed'),
('appearance', 'Set during character creation with `appearance <text>` -- a free-form\ndescription of how you look to others (for example: `appearance a tall,\nscarred warrior with piercing eyes`). Other players see it when they\n`look <your name>`, and it shows on your own score sheet. Type\n`appearance` with no text to clear it.', 'seed'),
('color', 'Usage: color [on|off]\n\nToggles ANSI color rendering for your connection. With no argument,\nshows the current setting. Color tags in the world (like <r>this<z>)\nrender as real colors when on and are stripped when off.', 'seed'),
('attack', 'Usage: attack <player>   (alias: kill -- identical)\n\nMortals: starts a fight with another player in your room; combat\nresolves in rounds, every hit lands on a specific limb, and you can\nabbreviate the target''s name (`attack clau` reaches Claudius).\nImmortals: an instant slay -- no rounds, no wait, the target dies.', 'seed'),
('kill', 'Usage: kill <player>   (alias: attack -- identical)\n\nMortals: starts a fight with another player in your room; combat\nresolves in rounds, every hit lands on a specific limb, and you can\nabbreviate the target''s name (`kill clau` reaches Claudius).\nImmortals: an instant slay -- no rounds, no wait, the target dies.', 'seed'),
('say', 'Usage: say <message>   (shorthand: ''<message>)\n\nSays something to everyone in your room. The apostrophe shorthand\nneeds no space: ''hello says "hello".', 'seed'),
('shout', 'Usage: shout <message>\n\nUnlike say, a shout reaches everyone playing, anywhere in the game --\nnot just your room. A sleeping character never hears it. Don''t want\nto hear shouts at all? `toggle noshout` opts you out -- except an\nimmortal''s shout always gets through regardless.', 'seed'),
('limbs', 'Usage: limbs\n\nShows the health of all thirteen of your limbs as percentages, with\nan injury note on any limb below 20%. A destroyed limb (0%) makes\nyour own attacks less accurate until you are made whole again.', 'seed'),
('flee', 'Usage: flee\n\nWhile fighting, makes a desperate attempt to escape through a random\nexit. You do not choose the direction and it does not always work -- a\nfailed flee leaves you in the fight. On success both sides stop\nfighting and you bolt to a neighbouring room.', 'seed'),
('bug', 'Usage: bug <description>\n\nReports a bug to the immortals -- your name and the date are recorded\nwith it. Please be specific about what you did and what went wrong.\nImmortals can type bug with no argument to list outstanding reports.', 'seed'),
('delbug', 'Usage: delbug <id>\n\nRemoves a bug report once it has been\nhandled. The id is the number shown beside each report in `bug`.', 'seed'),
('newbie', 'Usage: newbie <message>\n\nA help channel for new players. Everyone starts on it, so newcomers can\nask questions and veterans can answer. Turn it off (or back on) with\n`toggle newbie`; you must be on the channel to speak on it.', 'seed'),
('rules', 'Usage: rules [number]\n\nWith no argument, lists the numbered game rules. `rules <number>` shows\nthat rule in full. Please read them -- ignorance is no excuse.', 'seed'),
('edrules', 'Usage: edrules <number> <title>\n\nAdministrator (59+) only: writes or rewrites a numbered game rule. Give\nthe rule number and a title, then type the rule text into the line\neditor (/s saves, /a aborts, /b blanks, /f reflows to width). Players\nread rules with the rules command.', 'seed'),
('help', 'Usage: help [topic]\n\nWith no argument, lists every command available to you. With a topic\n(any command name, abbreviations welcome), shows its full help text.', 'seed'),
('wizhelp', 'Usage: wizhelp\n\nImmortals only: lists the immortal-only commands, with the minimum\nlevel each one requires.', 'seed'),
('exec', 'Usage: exec <shell command>\n\nRuns a command on the host box and shows\nits output. Fenced for safety -- a blocklist refuses dangerous commands\n(process kills, disk wipes, reboots, privilege escalation, touching the\nmud), every command runs under a timeout so it cannot freeze the game,\nand each use is logged. Not a root shell.', 'seed'),
('goto', 'Usage: goto <room vnum | player>\n\nImmortals only: teleport directly to a room by its vnum, or to another\nonline player by name (you land in their room). Useful vnums: 0 (The\nVoid), 1 (Imperia).', 'seed'),
('promote', 'Usage: promote <name> [level]\n\nImmortals only: set another player''s level (default 51, the first\nimmortal rank). You cannot set anyone above your own level, and the\nname must be typed in full. Works on offline players too; an online\ntarget changes immediately and is told. Also demotes.', 'seed'),
('edplayer', 'Usage: edplayer <name>\n\nAdministrator (58+) only: a menu-driven editor for a player''s level,\nexperience, HP/max HP, attributes, gender, title, load room, and\nhandedness -- an admin superset of promote. Works on any player,\nonline or offline, by exact name. Pick a numbered field, enter a new\nvalue, then (S)ave to write it to the database (an online target is\nupdated immediately, no relog needed) or (Q)uit to discard.', 'seed'),
('set', 'Usage: set <name> <field> <value>\n\nAdministrator (58+) only: a one-shot sibling of edplayer for quick,\nscriptable single-field edits -- one line in, one field changed, no\nmenu. Works on any player, online or offline, by exact name; an online\ntarget is updated immediately. Fields: level, xp, hp <hp> <max hp>,\nstr/dex/con/int/wis/cha, gender, title (or ''none'' to clear),\nloadroom, handed. See edplayer for a menu covering every field at once.', 'seed'),
('edhelp', 'Usage: edhelp <topic>\n\nLevel 56+ only: edit (or create) a help topic in a line editor. Any\nexisting text is shown first; lines you type are appended. Finish\nwith `/s` to save, `/a` to abort, `/b` to blank the buffer and start\nover, or `/f` to reflow it to the display width. Topics are stored in\nthe database and shown by `help <topic>`.', 'seed'),
('quit!', 'Usage: quit!   (must be typed exactly, with the !)\n\nWhile playing: leaves your character and returns to the account menu.\nAt the account menu: disconnects. It is never matched by abbreviation\nand nothing else starts with q -- a typo can never quit you.', 'seed'),
('movement', 'Usage: north / east / south / west / up / down\n\nWalks you through the room''s exits (shown by `look` as "Obvious\nexits"). The single letters n/e/s/w/u/d always mean movement. You\ncannot walk while fighting.', 'seed'),
('north', 'Usage: north (or just n)\n\nWalks you north. See `help movement`.', 'seed'),
('east', 'Usage: east (or just e)\n\nWalks you east. See `help movement`.', 'seed'),
('south', 'Usage: south (or just s)\n\nWalks you south. See `help movement`.', 'seed'),
('west', 'Usage: west (or just w)\n\nWalks you west. See `help movement`.', 'seed'),
('up', 'Usage: up (or just u)\n\nWalks you up. See `help movement`.', 'seed'),
('down', 'Usage: down (or just d)\n\nWalks you down. See `help movement`.', 'seed'),
('northeast', 'Usage: northeast (or just ne)\n\nWalks you northeast. See `help movement`.', 'seed'),
('northwest', 'Usage: northwest (or just nw)\n\nWalks you northwest. See `help movement`.', 'seed'),
('southeast', 'Usage: southeast (or just se)\n\nWalks you southeast. See `help movement`.', 'seed'),
('southwest', 'Usage: southwest (or just sw)\n\nWalks you southwest. See `help movement`.', 'seed'),
('log', 'Usage: log [lines] | log search <text> | log rotate | log list\n\nLevel 54+: reads the server''s game log from in game. Bare `log` shows\nthe last 20 lines (or `log 50` for more, up to 100). `search` finds\nlines containing your text, case-insensitively. `list` shows all log\nfiles in the logs/ directory. `rotate` (level 59+ only) closes the\ncurrent file and starts a fresh one.', 'seed'),
('setsev', 'Usage: setsev [type]\n\nImmortals only: controls which typed log messages (the colored [TAG]\nlines other commands echo to you as they happen) reach your screen.\nBare `setsev` lists every type with its current on/off state; `setsev\n<type>` (abbreviations welcome) flips one. Everything is on by default\neach time you log in. Types: game, pio, combat, bug, db, edit.', 'seed'),
('exits', 'Usage: exits\n\nLists this room''s exits and the name of the place each one leads to.\n(`look` shows the same directions as a one-line summary.) A secret exit\nnever appears here even if you know it''s there -- you can still walk\nit, it just isn''t listed.', 'seed'),
('open', 'Usage: open <direction>\n\nOpens a door blocking that exit, if there is one. A locked door can''t\nbe opened this way -- see `help unlock`. Once open, you (and everyone\nelse) can walk through; closing it again with `close` blocks movement\nuntil it''s reopened.', 'seed'),
('close', 'Usage: close <direction>\n\nCloses a door blocking that exit, if there is one and it is open.\nA closed door blocks movement through it (`The door is closed.`)\nuntil someone opens it again with `open`.', 'seed'),
('loadroom', 'Usage: loadroom [vnum]\n\nImmortals only: sets the room your character enters the game in at\nlogin (e.g. `loadroom 43`). Bare `loadroom` shows the current setting.\nThe room must exist.', 'seed'),
('users', 'Usage: users\n\nLevel 58+ only: lists every live connection -- character, account,\nIP address, and connection state (logging in, at the menu, creating,\nplaying, mid-editor). The admin''s who-is-really-here view.', 'seed'),
('prompt', 'Usage: prompt [hp]\n\n`prompt hp` toggles your hit points into the prompt line ("HP: 25 >").\nBare `prompt` shows the current setting. Your choice is saved with\nyour character. More stats will join the prompt as they exist.', 'seed'),
('mortal', 'Usage: mortal   (and later: immort)\n\nImmortals only: set your divinity aside and walk the world as a level\n50 mortal -- wait-states apply, you can be killed, and your immortal\ncommands are out of reach. Your true rank is kept safe (even through\ndeath or logout); type `immort` at any time to reclaim it.', 'seed'),
('edroom', 'Usage: edroom [<vnum>]   (level 51+ builders)\n\nOpens the Sneezy-style menu-driven room builder for the room you are\nstanding in, or for <vnum> from anywhere. Edits are held in a working\ncopy -- nothing touches the DB until you Save.\n\n  1) Name          2) Description (/s saves, /a cancels, /b wipes,\n                       /f reflows to width)\n  3) Flags         4) Sector Type\n  5) Exits         6) Max Capacity\n  7) Room Height\n\nExits: pick a direction, then set its Target vnum, Door type, and\nConditions; a missing target room is created on save and the reverse\nexit auto-fixed.\n\n  C) Clear room out (blanks it, exits included)\n  S) Save    Q) Quit (warns on unsaved changes)', 'seed'),
('news', 'Usage: news [lines-per-page]\n\nShows the whole game news feed -- announcements of new features, command\nchanges, and additions to the world, newest first -- a page at a time.\nAt a "more" prompt, press ENTER for the next page or Q to stop. Give a\nnumber (news 10, 20, 50, or 100) to set the page size; the default is 20.', 'seed'),
('ednews', 'Usage: ednews <headline>\n\nLevel 56+ only: post a news item. The words after the command are the\nheadline; you then type the story into a line editor (`/s` saves, `/a`\naborts, `/b` blanks, `/f` reflows to width). Everyone can read it\nwith the `news` command. Headlines must be unique.', 'seed'),
('positions', 'Usage: stand / sit / rest / sleep / wake\n\nYour body position. You must be standing to walk or start a fight.\nResting heals you faster than sitting, and sleeping fastest of all --\nbut while asleep you cannot see the room until you wake. You cannot\nchange position in the middle of a fight. Your current position shows\nin `score`.', 'seed'),
('stand', 'Usage: stand\n\nStand up. You must be standing to walk or to start a fight. See\n`help positions`.', 'seed'),
('sit', 'Usage: sit\n\nSit down. See `help positions`.', 'seed'),
('rest', 'Usage: rest\n\nSit down and rest -- you heal faster than while sitting or standing.\nSee `help positions`.', 'seed'),
('sleep', 'Usage: sleep\n\nLie down and sleep -- you heal fastest, but cannot see the room until\nyou `wake`. See `help positions`.', 'seed'),
('wake', 'Usage: wake\n\nWake up from sleep. See `help positions`.', 'seed'),
('catchup', 'Usage: catchup\n\nReplays any game messages (says, fights, arrivals) that arrived while\nyou were in an editor -- they are held rather than interrupting your\nwork, and cleared once you read them (or automatically after five\nminutes). When you leave an editor you are told if anything is waiting.', 'seed'),
('wiznews', 'Usage: wiznews [lines-per-page]\n\nLevel 51+ only: the immortals'' news channel -- read like `news` (whole\nfeed, newest first, a page at a time), but for matters that concern\nimmortals. Post to it with `edwiznews`.', 'seed'),
('edwiznews', 'Usage: edwiznews <headline>\n\nLevel 56+ only: post an item to the immortal news channel (read with\n`wiznews`). The words after the command are the headline; then type the\nstory into a line editor (`/s` saves, `/a` aborts, `/b` blanks,\n`/f` reflows to width).', 'seed'),
('socials', 'Usage: socials   |   <social> [target]\n\nSocials are emotes -- smile, nod, wave, laugh, bow, and more. Type\n`socials` to list them all, then use one on its own (`smile`) or aim it\nat someone in your room (`smile <name>`). Everyone in the room sees it.', 'seed'),
('wiznet', 'Usage: wiznet <message>   (shorthand: ;<message>)\n\nImmortals only: a private broadcast channel among the immortals. Your\nmessage reaches every online immortal (and yourself), out of sight of\nmortals. The `;` shorthand needs no space: `;hi` broadcasts "hi".', 'seed'),
('system', 'Usage: system <message>\n\nImmortals only: broadcast an atmosphere line to everyone in the game.\nPlayers read the bare message (e.g. "You hear a thud."); you see it\nprefixed with "system" as confirmation.', 'seed'),
('mudstats', 'Usage: mudstats\n\nShows basic statistics about the game world -- how many rooms, mobs\n(NPCs), and objects exist. More figures will be added over time.', 'seed'),
('multiplay', 'Usage: multiplay [on|off]\n\nLevel 59+ only: the global switch for whether mortals may run more than\none character at once. Off by default -- a mortal account gets a single\ncharacter in the game; immortals are always exempt. Persists across\nreboots.', 'seed'),
('colors', 'Color tags -- wrap text in <x>...<z> to colour it (needs `color on`):\n\n  <r> red       <R> bright red      <g> green      <G> bright green\n  <b> blue      <B> bright blue     <y> yellow     <Y> bright yellow\n  <p> purple    <P> bright purple   <c> cyan       <C> bright cyan\n  <w> white     <W> bright white    <k> grey       <z> reset\n\nExample: `say <r>Hello<z> there` shows "Hello" in red. Always close with\n<z> to reset. Colours work in say and in your title.', 'seed')
ON DUPLICATE KEY UPDATE `name` = `name`;

-- Migration: the room builder was renamed edit -> redit (Session 21);
-- drop the stale seed topic so it can't leak to mortals (no `edit`
-- command exists to gate it anymore). Hand-edited topics are spared.
DELETE FROM `help_topic` WHERE `name` = 'edit' AND `updated_by` = 'seed';

-- Migration: all ten directions (Session 21). ON DUPLICATE KEY UPDATE
-- name=name deliberately never touches existing rows, so the seed
-- movement topic needs an explicit refresh (hand-edited copies spared).
UPDATE `help_topic` SET `body` = 'Usage: north / east / south / west / up / down /\n       northeast / northwest / southeast / southwest\n\nWalks you through the room''s exits (shown by `look` as "Obvious\nexits"). Shortcuts: n, s, e, w, u, d, ne, nw, se, sw. You cannot walk\nwhile fighting.'
  WHERE `name` = 'movement' AND `updated_by` = 'seed';

-- Migration: Tier 3 gate changes (Session 21) -- log split 54/59 and
-- promote raised to 58; refresh the seed topics in existing DBs.
UPDATE `help_topic` SET `body` = 'Usage: log [lines] | log search <text> | log rotate | log list\n\nLevel 54+: reads the server''s game log from in game. Bare `log` shows\nthe last 20 lines (or `log 50` for more, up to 100). `search` finds\nlines containing your text, case-insensitively. `list` shows all log\nfiles in the logs/ directory. `rotate` (level 59+ only) closes the\ncurrent file and starts a fresh one.'
  WHERE `name` = 'log' AND `updated_by` = 'seed';
UPDATE `help_topic` SET `body` = 'Usage: promote <name> [level]\n\nLevel 58+ only: set another player''s level (default 51, the first\nimmortal rank). You cannot set anyone above your own level, and the\nname must be typed in full. Works on offline players too; an online\ntarget changes immediately and is told. Also demotes.'
  WHERE `name` = 'promote' AND `updated_by` = 'seed';
UPDATE `help_topic` SET `body` = 'Usage: say <message>   (shorthand: ''<message>)\n\nSays something to everyone in your room. The apostrophe shorthand\nneeds no space: ''hello says "hello". The say framing shows in cyan;\nyour message shows as typed, including any color tags you use.'
  WHERE `name` = 'say' AND `updated_by` = 'seed';

-- Migration: immortal look header ([vnum] name [sector] [flags]); look <player>.
UPDATE `help_topic` SET `body` = 'Usage: look [player]\n\nShows the room you are in: its name, description, obvious exits, and\neveryone standing there with you. You also look automatically whenever\nyou enter the world. `look <player>` describes another player in the\nroom (their appearance). Immortals additionally see the room''s vnum,\nsector type, and flags in the header line.'
  WHERE `name` = 'look' AND `updated_by` = 'seed';

-- Migration: attack/kill are full aliases (immortals instakill on both).
UPDATE `help_topic` SET `body` = 'Usage: attack <player>   (alias: kill -- identical)\n\nMortals: starts a fight with another player in your room; combat\nresolves in rounds, every hit lands on a specific limb, and you can\nabbreviate the target''s name (`attack clau` reaches Claudius).\nImmortals: an instant slay -- no rounds, no wait, the target dies.'
  WHERE `name` = 'attack' AND `updated_by` = 'seed';
UPDATE `help_topic` SET `body` = 'Usage: kill <player>   (alias: attack -- identical)\n\nMortals: starts a fight with another player in your room; combat\nresolves in rounds, every hit lands on a specific limb, and you can\nabbreviate the target''s name (`kill clau` reaches Claudius).\nImmortals: an instant slay -- no rounds, no wait, the target dies.'
  WHERE `name` = 'kill' AND `updated_by` = 'seed';

-- Migration: redit dropped to 51+ (every immortal builds).
UPDATE `help_topic` SET `body` = 'Usage: redit [field] [args]   (level 51+ builders)\n\nEdits the room you are standing in; every change saves to the\ndatabase immediately. Bare `redit` shows the room summary.\n\n  redit name <text>          -- set the room title\n  redit description          -- line editor (`.` saves, `~` aborts)\n  redit sector_type [n]      -- show or set the sector number\n  redit exit <dir> <toroom>  -- link an exit; creates the target room\n                                if needed and fixes the reverse exit\n  redit exit <dir> -1        -- delete an exit'
  WHERE `name` = 'redit' AND `updated_by` = 'seed';

-- Migration: redit gained an optional leading <vnum> to edit a room from
-- anywhere (not just the one you're standing in).
UPDATE `help_topic` SET `body` = 'Usage: redit [<vnum>] [field] [args]   (level 51+ builders)\n\nEdits a room; every change saves to the database immediately. With no\nvnum, edits the room you are standing in. Prefix a room number to edit\nthat room from anywhere (like goto, but for editing): `redit 100 name\nThe Plaza`. Bare `redit` (or `redit <vnum>`) shows the room summary.\n\n  redit name <text>          -- set the room title\n  redit description          -- line editor (`.` saves, `~` aborts)\n  redit sector_type [n]      -- show or set the sector number\n  redit exit <dir> <toroom>  -- link an exit; creates the target room\n                                if needed and fixes the reverse exit\n  redit exit <dir> -1        -- delete an exit'
  WHERE `name` = 'redit' AND `updated_by` = 'seed';

-- Migration: the shared line editor (redit description + hedit) gained
-- `/clear` to wipe the buffer (including preloaded text) and retype.
UPDATE `help_topic` SET `body` = 'Usage: redit [<vnum>] [field] [args]   (level 51+ builders)\n\nEdits a room; every change saves to the database immediately. With no\nvnum, edits the room you are standing in. Prefix a room number to edit\nthat room from anywhere (like goto, but for editing): `redit 100 name\nThe Plaza`. Bare `redit` (or `redit <vnum>`) shows the room summary.\n\n  redit name <text>          -- set the room title\n  redit description          -- line editor (`.` saves, `~` aborts,\n                                `/clear` wipes the buffer)\n  redit sector_type [n]      -- show or set the sector number\n  redit exit <dir> <toroom>  -- link an exit; creates the target room\n                                if needed and fixes the reverse exit\n  redit exit <dir> -1        -- delete an exit'
  WHERE `name` = 'redit' AND `updated_by` = 'seed';
UPDATE `help_topic` SET `body` = 'Usage: hedit <topic>\n\nLevel 56+ only: edit (or create) a help topic in a line editor. Any\nexisting text is shown first; lines you type are appended. Finish\nwith a single `.` to save, `~` to abort, or `/clear` to wipe the\nbuffer and start over. Topics are stored in the database and shown by\n`help <topic>`.'
  WHERE `name` = 'hedit' AND `updated_by` = 'seed';

-- Migration: redit became a menu-driven, working-copy editor (the old
-- one-shot `redit <field> <args>` command form was retired).
UPDATE `help_topic` SET `body` = 'Usage: redit [<vnum>]   (level 51+ builders)\n\nOpens the menu-driven room builder for the room you are standing in, or\nfor <vnum> from anywhere. Edits are held in a working copy -- nothing\ntouches the database until you Save.\n\n  (R) Room name         (D) Description (line editor: . saves, ~ cancels,\n  (F) Flags submenu         /clear wipes the buffer)\n  (T) Terrain submenu   (E) Exits submenu -- pick a direction, enter a\n                            target vnum; a missing room is created on\n                            save and the reverse exit auto-fixed\n  (C) Clear room out (blanks it, exits included)\n  (S) Save    (Q) Quit (warns if you have unsaved changes)'
  WHERE `name` = 'redit' AND `updated_by` = 'seed';

-- Migration: redit menu reformatted Sneezy-style (numbered fields), plus
-- Max Capacity / Room Height fields and per-exit door type + conditions.
UPDATE `help_topic` SET `body` = 'Usage: redit [<vnum>]   (level 51+ builders)\n\nOpens the Sneezy-style menu-driven room builder for the room you are\nstanding in, or for <vnum> from anywhere. Edits are held in a working\ncopy -- nothing touches the DB until you Save.\n\n  1) Name          2) Description (. saves, ~ cancels, /clear wipes)\n  3) Flags         4) Sector Type\n  5) Exits         6) Max Capacity\n  7) Room Height\n\nExits: pick a direction, then set its Target vnum, Door type, and\nConditions; a missing target room is created on save and the reverse\nexit auto-fixed.\n\n  C) Clear room out (blanks it, exits included)\n  S) Save    Q) Quit (warns on unsaved changes)'
  WHERE `name` = 'redit' AND `updated_by` = 'seed';

-- Migration: `news` now paginates the whole feed (was a line-limit).
UPDATE `help_topic` SET `body` = 'Usage: news [lines-per-page]\n\nShows the whole game news feed -- announcements of new features, command\nchanges, and additions to the world, newest first -- a page at a time.\nAt a "more" prompt, press ENTER for the next page or Q to stop. Give a\nnumber (news 10, 20, 50, or 100) to set the page size; the default is 20.'
  WHERE `name` = 'news' AND `updated_by` = 'seed';

-- Migration: the *edit editors were renamed to ed* (user 2026-07-05) --
-- redit->edroom, hedit->edhelp, addnews->ednews. The seed INSERT above adds
-- the new-named topics; drop the old seed rows so `help redit` etc. don't
-- linger. Hand-edited topics are spared (updated_by='seed' only).
DELETE FROM `help_topic` WHERE `name` IN ('redit', 'hedit', 'addnews')
  AND `updated_by` = 'seed';

-- Migration: wiznet gained the ; shorthand (user 2026-07-05).
UPDATE `help_topic` SET `body` = 'Usage: wiznet <message>   (shorthand: ;<message>)\n\nImmortals only: a private broadcast channel among the immortals. Your\nmessage reaches every online immortal (and yourself), out of sight of\nmortals. The `;` shorthand needs no space: `;hi` broadcasts "hi".'
  WHERE `name` = 'wiznet' AND `updated_by` = 'seed';

-- Migration: goto now accepts a player name too (2026-07-05); help edit is
-- dynamic (no topic needed).
UPDATE `help_topic` SET `body` = 'Usage: goto <room vnum | player>\n\nImmortals only: teleport directly to a room by its vnum, or to another\nonline player by name (you land in their room). Useful vnums: 0 (The\nVoid), 1 (Imperia).'
  WHERE `name` = 'goto' AND `updated_by` = 'seed';

-- Migration: who gained filter arguments (2026-07-05). The main INSERT's
-- ON DUPLICATE KEY UPDATE never touches existing rows, so refresh the seed
-- body explicitly (hand-edited copies spared).
UPDATE `help_topic` SET `body` = 'Usage: who [name|immortals|mortals]\n\nLists everyone currently playing, with their level (or immortal rank\ntitle) shown in brackets before their name and any personal title after\nit. With an argument, filters the list: `who imm` shows only immortals,\n`who mort` only mortals, and any other word matches part of a name.'
  WHERE `name` = 'who' AND `updated_by` = 'seed';

-- Migration: color preference is now remembered per account (2026-07-05),
-- asked once at account creation and updated by `color on|off`.
UPDATE `help_topic` SET `body` = 'Usage: color [on|off]\n\nToggles ANSI color rendering for your connection. With no argument,\nshows the current setting. Color tags in the world (like <r>this<z>)\nrender as real colors when on and are stripped when off. Your choice is\nremembered on your account -- you are asked once when the account is\ncreated, and `color on|off` updates it for next time.'
  WHERE `name` = 'color' AND `updated_by` = 'seed';

-- Migration: title gained <N> name substitution (2026-07-05).
UPDATE `help_topic` SET `body` = 'Usage: title [text]\n\nSets the free-form title shown in who. Normally it trails your name\n("Yourname the Brave"). Include <N> anywhere and it is replaced by your\nname, and the title is shown on its own -- e.g.\n`title You are not paranoid, <N> really is out to get you!`. `title`\nwith no text, or `title none`, clears it. Saved with your character.'
  WHERE `name` = 'title' AND `updated_by` = 'seed';

-- Migration: look's room name/description are now tinted by sector (2026-07-06).
UPDATE `help_topic` SET `body` = 'Usage: look\n\nShows the room you are in: its name, description, and everyone\nstanding there with you. You also look automatically whenever you\nenter the world. The name and description are tinted by the room''s\nterrain -- a snowy waste reads differently from a jungle or a city\nstreet.'
  WHERE `name` = 'look' AND `updated_by` = 'seed';

-- Migration: every ed* line editor gained a `/format` reflow-to-width
-- command alongside `.`/`~`/`/clear` (2026-07-06).
UPDATE `help_topic` SET `body` = 'Usage: edrules <number> <title>\n\nAdministrator (59+) only: writes or rewrites a numbered game rule. Give\nthe rule number and a title, then type the rule text into the line\neditor (/s saves, /a aborts, /b blanks, /f reflows to width). Players\nread rules with the rules command.'
  WHERE `name` = 'edrules' AND `updated_by` = 'seed';
UPDATE `help_topic` SET `body` = 'Usage: edhelp <topic>\n\nLevel 56+ only: edit (or create) a help topic in a line editor. Any\nexisting text is shown first; lines you type are appended. Finish\nwith `/s` to save, `/a` to abort, `/b` to blank the buffer and start\nover, or `/f` to reflow it to the display width. Topics are stored in\nthe database and shown by `help <topic>`.'
  WHERE `name` = 'edhelp' AND `updated_by` = 'seed';
UPDATE `help_topic` SET `body` = 'Usage: edroom [<vnum>]   (level 51+ builders)\n\nOpens the Sneezy-style menu-driven room builder for the room you are\nstanding in, or for <vnum> from anywhere. Edits are held in a working\ncopy -- nothing touches the DB until you Save.\n\n  1) Name          2) Description (/s saves, /a cancels, /b wipes,\n                       /f reflows to width)\n  3) Flags         4) Sector Type\n  5) Exits         6) Max Capacity\n  7) Room Height\n\nExits: pick a direction, then set its Target vnum, Door type, and\nConditions; a missing target room is created on save and the reverse\nexit auto-fixed.\n\n  C) Clear room out (blanks it, exits included)\n  S) Save    Q) Quit (warns on unsaved changes)'
  WHERE `name` = 'edroom' AND `updated_by` = 'seed';
UPDATE `help_topic` SET `body` = 'Usage: ednews <headline>\n\nLevel 56+ only: post a news item. The words after the command are the\nheadline; you then type the story into a line editor (`/s` saves, `/a`\naborts, `/b` blanks, `/f` reflows to width). Everyone can read it\nwith the `news` command. Headlines must be unique.'
  WHERE `name` = 'ednews' AND `updated_by` = 'seed';
UPDATE `help_topic` SET `body` = 'Usage: edwiznews <headline>\n\nLevel 56+ only: post an item to the immortal news channel (read with\n`wiznews`). The words after the command are the headline; then type the\nstory into a line editor (`/s` saves, `/a` aborts, `/b` blanks,\n`/f` reflows to width).'
  WHERE `name` = 'edwiznews' AND `updated_by` = 'seed';

-- Migration: exits now hides secret exits (2026-07-06, door mechanics).
UPDATE `help_topic` SET `body` = 'Usage: exits\n\nLists this room''s exits and the name of the place each one leads to.\n(`look` shows the same directions as a one-line summary.) A secret exit\nnever appears here even if you know it''s there -- you can still walk\nit, it just isn''t listed.'
  WHERE `name` = 'exits' AND `updated_by` = 'seed';

-- New topics: objects (Phase 2C, 2026-07-07) -- get/drop/inventory/wear/
-- remove/equipment/oload. New INSERT (not an UPDATE) since these topics
-- don't exist yet; ON DUPLICATE KEY UPDATE name=name is a no-op on rerun,
-- same pattern as the main seed INSERT above.
INSERT INTO `help_topic` (`name`, `body`, `updated_by`) VALUES
('get', 'Usage: get <item>\n\nPicks up an item from the room floor and adds it to what you are\ncarrying. Fixed scenery can''t be taken this way. See `inventory` to\nsee what you''re carrying, `wear` to put something on.', 'seed'),
('drop', 'Usage: drop <item>\n\nPuts down a carried item on the floor of the room you''re in. Only\nworks on loose carried items -- `remove` a worn or held item first.', 'seed'),
('inventory', 'Usage: inventory\n\nLists everything you are carrying loose (not worn or held -- see\n`equipment` for that).', 'seed'),
('equipment', 'Usage: equipment\n\nLists everything you are wearing and holding, by body part, plus your\nprimary and secondary hold. Genitalia isn''t listed here -- it can''t be\nworn (see `help limbs`).', 'seed'),
('wear', 'Usage: wear <item>\n       wear all\n\nPuts on a carried item into its body slot (head, body, legs, and so\non). Refuses if you''re already wearing something there, or if the\nitem isn''t wearable there at all -- a holdable item (weapon or\notherwise) isn''t worn this way; see `hold`/`wield` instead.\n`wear all` equips everything you''re carrying that fits an open slot,\nquietly skipping anything that doesn''t.', 'seed'),
('remove', 'Usage: remove <item>\n\nTakes off a worn item or lays down a held one, returning it to your\ncarried inventory.', 'seed')
ON DUPLICATE KEY UPDATE `name` = `name`;

-- Migration: attack/kill/look now also reach mobiles (Phase 2D, 2026-07-07).
UPDATE `help_topic` SET `body` = 'Usage: attack <player or mobile>   (alias: kill -- identical)\n\nMortals: starts a fight with another player or a mobile in your room;\ncombat resolves in rounds, every hit lands on a specific limb, and you\ncan abbreviate the target''s name (`attack clau` reaches Claudius,\n`attack vrock` reaches a vrock demon). Killing a mobile removes it from\nthe world for good. Immortals: an instant slay -- no rounds, no wait,\nthe target dies (a slain mobile is likewise removed).'
  WHERE `name` = 'attack' AND `updated_by` = 'seed';
UPDATE `help_topic` SET `body` = 'Usage: kill <player or mobile>   (alias: attack -- identical)\n\nMortals: starts a fight with another player or a mobile in your room;\ncombat resolves in rounds, every hit lands on a specific limb, and you\ncan abbreviate the target''s name (`kill clau` reaches Claudius, `kill\nvrock` reaches a vrock demon). Killing a mobile removes it from the\nworld for good. Immortals: an instant slay -- no rounds, no wait, the\ntarget dies (a slain mobile is likewise removed).'
  WHERE `name` = 'kill' AND `updated_by` = 'seed';
UPDATE `help_topic` SET `body` = 'Usage: look [player or mobile]\n\nShows the room you are in: its name, description, obvious exits, and\neveryone (and everything) standing there with you. You also look\nautomatically whenever you enter the world. `look <name>` describes\nanother player or a mobile in the room (their appearance/description).\nImmortals additionally see the room''s vnum, sector type, and flags in\nthe header line.'
  WHERE `name` = 'look' AND `updated_by` = 'seed';

UPDATE `help_topic` SET `body` = 'Usage: look [player, mobile, or item]\n\nShows the room you are in: its name, description, obvious exits, and\neveryone (and everything) standing there with you. You also look\nautomatically whenever you enter the world. `look <name>` describes\nanother player or a mobile in the room (their appearance/description),\nor an item -- on the room floor or in your own inventory/equipment --\nshowing its description and, if it has one, its condition. Immortals\nadditionally see the room''s vnum, sector type, and flags in the header\nline.'
  WHERE `name` = 'look' AND `updated_by` = 'seed';

-- New topics: scan (player) + vnum (builder) (2026-07-07, home session).
INSERT INTO `help_topic` (`name`, `body`, `updated_by`) VALUES
('scan', 'Usage: scan [direction | name]\n\nPeer several rooms deep down each of the room''s exits and report the\nplayers and mobiles you can make out, each tagged with roughly how far\noff it is and which way. `scan north` looks only that direction; `scan\n<name>` reports only beings whose name matches. A closed or secret\ndoor blocks your line of sight down that exit.', 'seed'),
('vnum', 'Usage: vnum <room|obj|mob> <pattern>\n\nBuilder tool (level 51+): lists the vnums and names of rooms, objects,\nor mobiles whose name contains <pattern> (case-insensitive). Handy for\nfinding a prototype''s number to oload/mload or goto. Results are listed\nlowest vnum first, a page at a time.', 'seed')
ON DUPLICATE KEY UPDATE `name` = `name`;

-- vnum's body changed after it was first seeded (40-cap -> pagination); the
-- INSERT above is a no-op on the existing row, so update it explicitly.
UPDATE `help_topic` SET `body` = 'Usage: vnum <room|obj|mob> <pattern>\n\nLists the vnums and names of rooms, objects,\nor mobiles whose name contains <pattern> (case-insensitive). <pattern>\ncan also be a bare vnum (vnum obj 1017) or a vnum range (vnum obj\n100-200) to browse by number directly instead of by name. Handy for\nfinding a prototype''s number to oload/mload or goto. Results are listed\nlowest vnum first, a page at a time.'
  WHERE `name` = 'vnum' AND `updated_by` = 'seed';

-- New topics: containers + put; get/open/close now also act on containers
-- (containers feature, work session).
INSERT INTO `help_topic` (`name`, `body`, `updated_by`) VALUES
('put', 'Usage: put <item> <container>\n\nMoves a carried item into a container -- a bag, chest, or the like --\nthat you''re carrying or that''s on the room floor. The container must\nbe open, and must have room for the item''s weight. See `get <item>\n<container>` to take it back out, and `containers` for the full\nrundown.', 'seed'),
('containers', 'Containers -- bags, chests, pouches, and the like -- can hold other\nitems.\n\n  put <item> <container>        stash a carried item inside\n  get <item> <container>        take an item back out\n  look <container>              see what''s inside (when open)\n  open / close <container>      shut or unshut a closeable container\n\nA closed container keeps its contents to itself until you open it, and\nnothing more fits once its weight capacity is full. Locks and keys\naren''t built yet, so a locked container can''t be opened for now.', 'seed')
ON DUPLICATE KEY UPDATE `name` = `name`;

-- Migration: get now also takes from a container; open/close now also act on
-- a container object, not just doors.
UPDATE `help_topic` SET `body` = 'Usage: get <item> [container]\n\nPicks up an item from the room floor and adds it to what you are\ncarrying. Fixed scenery can''t be taken this way. With a second word,\n`get <item> <container>` takes the item out of a container (one you''re\ncarrying or one on the floor) instead. See `inventory`, `put`, and\n`containers`.'
  WHERE `name` = 'get' AND `updated_by` = 'seed';

-- Migration (user 2026-07-11: "corpses are supposed to act like
-- containers -- get all corpse should get all items"): `get all
-- <container>` empties it in one go.
UPDATE `help_topic` SET `body` = 'Usage: get <item> [container]\n\nPicks up an item from the room floor and adds it to what you are\ncarrying. Fixed scenery can''t be taken this way. With a second word,\n`get <item> <container>` takes the item out of a container (one you''re\ncarrying or one on the floor, including a corpse) instead. `get all\n<container>` takes EVERYTHING out of it in one go. See `inventory`,\n`put`, and `containers`.'
  WHERE `name` = 'get' AND `updated_by` = 'seed';
UPDATE `help_topic` SET `body` = 'Containers -- bags, chests, pouches, corpses, and the like -- can hold\nother items.\n\n  put <item> <container>        stash a carried item inside\n  get <item> <container>        take an item back out\n  get all <container>           take EVERYTHING out at once\n  look <container>              see what''s inside (when open)\n  open / close <container>      shut or unshut a closeable container\n\nA closed container keeps its contents to itself until you open it, and\nnothing more fits once its weight capacity is full. Locks and keys\naren''t built yet, so a locked container can''t be opened for now.'
  WHERE `name` = 'containers' AND `updated_by` = 'seed';
UPDATE `help_topic` SET `body` = 'Usage: open <direction>   |   open door [direction]   |   open <container>\n\nOpens a closed door blocking that exit, or a closeable container (a\nbag, chest, and the like) that you''re carrying or that''s on the floor.\n`door` is an optional word in front of the direction (`open door\nnorth`, matching Sneezy''s original phrasing) -- `open north` alone\nstill works too. A bare `open door` with no direction opens the room''s\none door, if it has exactly one. A locked door or container can''t be\nopened this way -- that needs a key, which isn''t built yet. Close it\nagain with `close`.'
  WHERE `name` = 'open' AND `updated_by` = 'seed';
UPDATE `help_topic` SET `body` = 'Usage: close <direction>   |   close door [direction]   |   close <container>\n\nCloses a door blocking that exit, or a closeable container you''re\ncarrying or that''s on the floor. `door` is an optional word in front\nof the direction (`close door north`, matching Sneezy''s original\nphrasing) -- `close north` alone still works too. A bare `close door`\nwith no direction closes the room''s one door, if it has exactly one.\nA closed door blocks movement; a closed container keeps its contents\nsealed until someone opens it again with `open`.'
  WHERE `name` = 'close' AND `updated_by` = 'seed';

-- Merge oload/mload into `load` (user 2026-07-09: one command, category as
-- the first argument). Remove the two orphaned topics from any earlier
-- deploy, then seed the merged one. Also sweeping up 'redit'/'hedit' --
-- pre-ed*-rename topic names (now `edroom`/`edhelp`) that were never
-- deleted and still carried stale legacy editor-key text ('.'/'~'/
-- '/clear'/'/format', removed Session 32) -- found while chasing the
-- same staleness in edrules/ednews/edwiznews/edhelp/edroom above.
DELETE FROM `help_topic` WHERE `name` IN ('oload', 'mload', 'redit', 'hedit');

INSERT INTO `help_topic` (`name`, `body`, `updated_by`) VALUES
('load', 'Usage: load <mob|obj> <vnum|name>\n\nSpawns a copy of a mob or object prototype\ninto the room you''re standing in -- replaces the old separate oload/\nmload commands. Give the category (mob or obj -- a single letter M/O\nworks too) then an exact vnum, or a name/keyword (`load obj sword`\nloads the first object whose name contains "sword"; `load mob demon`\nthe first mobile whose name contains "demon"). There''s no automatic\nworld respawn yet, so anything placed this way is gone if the server\nrestarts -- only what players are actually carrying survives that.', 'seed')
ON DUPLICATE KEY UPDATE `name` = `name`;

-- New topics: hold/wield/switch (user 2026-07-09: split from the old
-- unified wear-onto-a-hand behavior).
INSERT INTO `help_topic` (`name`, `body`, `updated_by`) VALUES
('hold', 'Usage: hold <item>\n\nHolds a carried non-weapon item in a free hand (a torch, a shield, and\nthe like). Refuses a weapon -- those must be `wield`ed instead -- and\nrefuses anything that isn''t holdable at all. See `switch` to swap\nhands, and `equipment` to see what''s in each.', 'seed'),
('wield', 'Usage: wield <item>\n\nWields a carried weapon in a free hand. Refuses anything that isn''t a\nweapon -- a non-weapon holdable uses `hold` instead. See `switch` to\nswap hands, and `equipment` to see what''s in each.', 'seed'),
('switch', 'Usage: switch\n\nSwaps whatever is in your primary and secondary hold -- no need to\n`remove` either one first. Handy for bringing a second weapon or tool\nto the front without letting go of anything.', 'seed')
ON DUPLICATE KEY UPDATE `name` = `name`;

-- Migration (2026-07-09): `help color` now lists every tag itself instead
-- of only pointing at the separate `colors` topic, and both `color` and
-- `who` mention that a title can use <N>/<n> name substitution (titles
-- shown in `who` can use both tricks).
UPDATE `help_topic` SET `body` = 'Usage: color [on|off]\n\nToggles ANSI color rendering for your connection. With no argument,\nshows the current setting. Your choice is remembered on your account --\nyou are asked once when the account is created, and `color on|off`\nupdates it for next time.\n\nTags: <r> red  <R> bright red  <g> green  <G> bright green  <b> blue\n<B> bright blue  <y> yellow  <Y> bright yellow  <p> purple\n<P> bright purple  <c> cyan  <C> bright cyan  <w> white  <W> bright\nwhite  <k> grey  <z> reset -- wrap text in <x>...<z> and always close\nwith <z>. A title (see `help title`) can also use <N> or <n> anywhere\nto insert your own name.'
  WHERE `name` = 'color' AND `updated_by` = 'seed';
UPDATE `help_topic` SET `body` = 'Usage: who [name|immortals|mortals]\n\nLists everyone currently playing, with their level (or immortal rank\ntitle) shown in brackets before their name and any personal title after\nit. With an argument, filters the list: `who imm` shows only immortals,\n`who mort` only mortals, and any other word matches part of a name.\n\nA personal title (see `help title`) can use color tags (`help colors`)\nand <N>/<n> to insert your own name anywhere in the text.'
  WHERE `name` = 'who' AND `updated_by` = 'seed';

-- New topic: `hit` (2026-07-09) -- real combat for anyone, even immortals.
INSERT INTO `help_topic` (`name`, `body`, `updated_by`) VALUES
('hit', 'Usage: hit <player>   (alias: engage -- identical)\n\nStarts a real fight through the normal multi-round combat process --\nfor anyone, including immortals. Unlike kill/attack (an instant slay for\nan immortal), hit always resolves in rounds, every hit landing on a\nspecific limb, so an immortal can use it to actually fight something\ninstead of instakilling it.', 'seed'),
('engage', 'Usage: engage <player>   (alias: hit -- identical)\n\nStarts a real fight through the normal multi-round combat process --\nfor anyone, including immortals. Unlike kill/attack (an instant slay for\nan immortal), engage always resolves in rounds, every hit landing on a\nspecific limb, so an immortal can use it to actually fight something\ninstead of instakilling it.', 'seed')
ON DUPLICATE KEY UPDATE `name` = `name`;

-- `zonereset`/`zoneassign` merged into `zone`/`edzone` (2026-07-10) --
-- remove the superseded topics from any earlier deploy.
DELETE FROM `help_topic` WHERE `name` IN ('zonereset', 'zoneassign');

-- New topics: `zone` and `edzone` (2026-07-10) -- Zones Part 2 + identity.
INSERT INTO `help_topic` (`name`, `body`, `updated_by`) VALUES
('zone', 'Usage: zone reset <zone number>\n       zone list\n\n`zone reset` force-runs a zone''s reset right\nnow, exactly like its periodic timer would -- loads any mobs/objects\nstill under their per-room cap, equips/gives them items, sets doors.\nEvery zone also resets automatically: once (in full) whenever the\nserver starts, then periodically after that on its own lifespan.\n`zone list` shows every zone with its name, enabled state, lifespan,\nand assigned builders. For editing a zone''s own properties or\nassigning builders, see `edzone`.', 'seed'),
('edzone', 'Usage: edzone <zone number>\n\nMenu-driven zone editor (level 51+, but see below): change a zone''s\nname, enabled state, lifespan, and vnum range; assign or un-assign\nbuilders (selecting a builder already assigned un-assigns them); or\nforce a reset right now. A level 51-54 builder can only edzone (or\nedroom) a zone they are assigned to -- editing any other zone is\nrefused. 55+ can always edit anything, and content outside any zone is\nunrestricted for every builder.', 'seed')
ON DUPLICATE KEY UPDATE `name` = `name`;

-- New topic: `zonefile` (2026-07-11, user: "zonefile create should create
-- a zone file with the current status of the zone and its contents...
-- you should also be able to delete a line from the zone file, rerun
-- zonefile create and it fills in the blanks").
INSERT INTO `help_topic` (`name`, `body`, `updated_by`) VALUES
('zonefile', 'Usage: zonefile create <zone number>\n\nSame zone-assignment rule as `edzone` applies (see `help edzone`).\nSnapshots the zone''s CURRENT live state -- every mob and object\nactually sitting in its rooms right now, including a mob''s\nequipped/held/carried items and a container''s contents -- into new\nreset data, so the next boot or periodic reset recreates exactly what\nyou''ve built. Safe to re-run: a room+item pairing already covered by\nexisting reset data is left untouched, so deleting one reset line and\nrunning `zonefile create` again only fills in what that deletion left\nuncovered -- it never duplicates what''s already there. An item worn or\nwielded by a mob (as opposed to carried or sitting on the ground) can''t\nhave its own contents captured if it''s a container.', 'seed')
ON DUPLICATE KEY UPDATE `name` = `name`;

-- New topic: `time` (2026-07-10) -- the day/date system.
INSERT INTO `help_topic` (`name`, `body`, `updated_by`) VALUES
('time', 'Usage: time\n\nShows the current mud clock, the day of the week, and the date --\n"It is 3:45 PM, on Wednesday" / "The 5th day of March, Year 1." The\nclock advances on its own as real time passes; noon, midnight, and the\nturn of a new month or year are announced to everyone.', 'seed')
ON DUPLICATE KEY UPDATE `name` = `name`;

-- The `time` body changed (personal time-zone offset added, 2026-07-10) --
-- the INSERT above is a no-op on the already-seeded row, so update it
-- explicitly.
UPDATE `help_topic` SET `body` = 'Usage: time\n       time <difference>\n\nShows the current mud clock, the day of the week, and the date --\n"It is 3:45 PM, on Wednesday" / "The 5th day of March, Year 1." The\nclock advances on its own as real time passes; noon, midnight, and the\nturn of a new month or year are announced to everyone. A second line\nshows the real-world time where you are, based on the time-zone offset\nyou chose at account creation (hours difference from the server''s own\nEastern clock) -- `time <difference>` changes that offset later.'
  WHERE `name` = 'time';

-- The `catchup` body changed (now also covers pagination, and is
-- mortal-level, 2026-07-10) -- the INSERT above is a no-op on the
-- already-seeded row, so update it explicitly.
UPDATE `help_topic` SET `body` = 'Usage: catchup\n\nReplays any game messages (says, fights, arrivals) that arrived while\nyou were in an editor, or mid-way through reading a long, paginated\nlisting (like `news`) -- they are held rather than interrupting your\nwork, and cleared once you read them (or automatically after five\nminutes). You are told if anything is waiting once you finish.'
  WHERE `name` = 'catchup';

-- New topics: `idea`/`delidea` (2026-07-10).
INSERT INTO `help_topic` (`name`, `body`, `updated_by`) VALUES
('idea', 'Usage: idea <description>\n\nSuggests a new feature to the immortals -- your name and the date are\nrecorded with it. Immortals can type idea with no argument to list\noutstanding suggestions.', 'seed'),
('delidea', 'Usage: delidea <id>\n\nRemoves an idea once it has been handled.\nThe id is the number shown beside each idea in `idea`.', 'seed')
ON DUPLICATE KEY UPDATE `name` = `name`;

-- New topic: `purge` (2026-07-10).
INSERT INTO `help_topic` (`name`, `body`, `updated_by`) VALUES
('purge', 'Usage: purge\n       purge linkdead\n\nBare purge empties your current room of every mob\nand object in it (never players). Administrator (58+) only: purge\nlinkdead force-removes every linkdead character from the game.', 'seed')
ON DUPLICATE KEY UPDATE `name` = `name`;

-- New topic: `transfer` (2026-07-10).
INSERT INTO `help_topic` (`name`, `body`, `updated_by`) VALUES
('transfer', 'Usage: transfer <name>\n       transfer <name> <room vnum>\n\nImmortal-only: pulls an online player into your own room, or into a\nspecific room if you give its vnum. The player is told what happened\nand shown their new surroundings; anyone in the rooms they leave and\narrive in sees them vanish and appear in a puff of smoke.', 'seed')
ON DUPLICATE KEY UPDATE `name` = `name`;

-- New topic: `pee` (2026-07-11).
INSERT INTO `help_topic` (`name`, `body`, `updated_by`) VALUES
('pee', 'Usage: pee\n       pee <liquid>\n\nRelieves yourself, leaving a puddle on the floor of your current room\nfor everyone to see. Bare `pee` leaves plain pee; `pee <liquid>` (e.g.\n`pee water`, `pee wine`) leaves a puddle of that liquid instead, if\nrecognized -- abbreviations welcome. A second pee of the SAME liquid in\nthe same room grows the existing puddle bigger instead of starting a\nnew one; a different liquid starts its own puddle alongside it. Purely\nfor flavor.\n\nRelated: drink', 'seed')
ON DUPLICATE KEY UPDATE `name` = `name`;

-- New topic: `drink` (2026-07-11).
INSERT INTO `help_topic` (`name`, `body`, `updated_by`) VALUES
('drink', 'Usage: drink <puddle>\n\nDrinks from a puddle on the ground -- a pool of pee, or a pool of\nblood left by a badly wounded limb. There is a chance of getting\npoisoned (a scare, not lethal on its own). Purely for flavor.', 'seed')
ON DUPLICATE KEY UPDATE `name` = `name`;

-- New topics: `poofin`/`poofout` (2026-07-11; named "bamfin"/"bamfout"
-- until later the same session, when that name moved to `goto`'s
-- messages instead -- see the `bamfin`/`bamfout` topics further below).
--
-- Migration ordering matters here: an already-deployed DB still has rows
-- literally NAMED `bamfin`/`bamfout` (the OLD, pre-rename topic pair) --
-- rename them to `poofin`/`poofout` FIRST (preserving their content,
-- rewritten for the new WALKING-specific wording), THEN insert-if-missing
-- covers a fresh install that never had the old rows to rename. Doing the
-- INSERT first would collide with this rename on an already-deployed DB
-- (both would try to claim the name `poofin`).
-- Guarded with "poofin/poofout don't already exist" (as well as the
-- original updated_by='seed' check) so re-running this file after the
-- rename already happened doesn't grab the FRESH `bamfin`/`bamfout` pair
-- created later in this file for goto's own messages -- caught for real
-- the first time this file was re-applied post-goto-feature (2026-07-11).
UPDATE `help_topic` SET `name` = 'poofin',
  `body` = 'Usage: poofin <message>\n       poofin none\n\nImmortal-only: sets your own custom WALKING arrival message,\nreplacing the default "has arrived" wording -- e.g. "drags $p cross\nin from the $d" reads as "Jesus drags his cross in from the east."\n`$d` is replaced with the direction you arrived from; `$p` with your\ngender''s possessive pronoun (his/her/their), so the same message\nreads correctly no matter who sets it. `poofin none` (or `clear`)\nreverts to the default wording. For `goto`''s own messages, see\n`help bamfin`.'
  WHERE `name` = 'bamfin' AND `updated_by` = 'seed'
    AND NOT EXISTS (SELECT 1 FROM (SELECT `name` FROM `help_topic`) AS existing WHERE existing.`name` = 'poofin');
UPDATE `help_topic` SET `name` = 'poofout',
  `body` = 'Usage: poofout <message>\n       poofout none\n\nImmortal-only: sets your own custom WALKING departure message,\nreplacing the default "exits to the <direction>" wording. Same\n`$d`/`$p` token rules as `poofin` -- see `help poofin`.'
  WHERE `name` = 'bamfout' AND `updated_by` = 'seed'
    AND NOT EXISTS (SELECT 1 FROM (SELECT `name` FROM `help_topic`) AS existing WHERE existing.`name` = 'poofout');

INSERT INTO `help_topic` (`name`, `body`, `updated_by`) VALUES
('poofin', 'Usage: poofin <message>\n       poofin none\n\nImmortal-only: sets your own custom WALKING arrival message,\nreplacing the default "has arrived" wording -- e.g. "drags $p cross\nin from the $d" reads as "Jesus drags his cross in from the east."\n`$d` is replaced with the direction you arrived from; `$p` with your\ngender''s possessive pronoun (his/her/their), so the same message\nreads correctly no matter who sets it. `poofin none` (or `clear`)\nreverts to the default wording. For `goto`''s own messages, see\n`help bamfin`.', 'seed'),
('poofout', 'Usage: poofout <message>\n       poofout none\n\nImmortal-only: sets your own custom WALKING departure message,\nreplacing the default "exits to the <direction>" wording. Same\n`$d`/`$p` token rules as `poofin` -- see `help poofin`.', 'seed')
ON DUPLICATE KEY UPDATE `name` = `name`;

-- New topics: `bamfin`/`bamfout` (2026-07-11, freed up from the WALKING
-- move-message feature above, user: "bamfin|out should modify goto
-- messaging"; follow-ups the same session: "<N> should work in this as
-- well as $g" and "and $p").
INSERT INTO `help_topic` (`name`, `body`, `updated_by`) VALUES
('bamfin', 'Usage: bamfin <message>\n       bamfin none\n\nImmortal-only: sets your own custom `goto` ARRIVAL message, shown to\neveryone already in the room you teleport into -- replaces the\ndefault "<Name> appears in a puff of smoke." wording. Three tokens:\n`<N>` (your name -- may appear anywhere in the message, e.g. "The\nair crackles and <N> steps through."), `$g` (the room''s\nground-surface word, e.g. "ground"/"street"/"sand"), and `$p` (your\ngender''s possessive pronoun). `bamfin none` (or `clear`) reverts to\nthe default wording. For the WALKING equivalent, see `help poofin`.', 'seed'),
('bamfout', 'Usage: bamfout <message>\n       bamfout none\n\nImmortal-only: sets your own custom `goto` DEPARTURE message, shown\nto everyone in the room you teleport away from -- replaces the\ndefault "<Name> disappears in a puff of smoke." wording. Same\n`<N>`/`$g`/`$p` token rules as `bamfin` -- see `help bamfin`. Your\nown private "You vanish in a puff of smoke." line is unaffected\neither way.', 'seed')
ON DUPLICATE KEY UPDATE `name` = `name`;

-- The `set` body changed (now also covers alignment, 2026-07-10) -- the
-- INSERT above is a no-op on the already-seeded row, so update it explicitly.
UPDATE `help_topic` SET `body` = 'Usage: set <name> <field> <value>\n\nAdministrator (58+) only: a one-shot sibling of edit player for quick,\nscriptable single-field edits -- one line in, one field changed, no\nmenu. Works on any player, online or offline, by exact name; an online\ntarget is updated immediately. Fields: level, xp, hp <hp> <max hp>,\nalignment (-1000 evil .. 1000 good), str/dex/con/int/wis/cha, gender,\ntitle (or ''none'' to clear), loadroom, handed. See edit player for a\nmenu covering every field at once.'
  WHERE `name` = 'set';

-- New topic: `edit` (2026-07-11) -- unifies edroom/edzone/edplayer/edhelp/
-- ednews/edwiznews/edrules into one command; see cmd_edit.c. The old
-- per-editor topics (edroom, edzone, edplayer, edhelp, ednews, edwiznews,
-- edrules) are removed from the DB (see the deploy notes) since those
-- commands no longer exist standalone.
INSERT INTO `help_topic` (`name`, `body`, `updated_by`) VALUES
('edit', 'Usage: edit <noun> [args]\n\nOne entry point for every editor, replacing the old standalone ed*\ncommands. Each noun still needs its own level, same as before:\n\n  edit room [<vnum>]        (51+) menu-driven room builder\n  edit zone <zone number>   (51+) zone properties/builders\n  edit trigger ...          (51+) scripted mob/obj/room behavior --\n                                  see `help trigger`\n  edit social [name]        (55+) menu-driven social/emote editor\n  edit player <name>        (58+) menu-driven player editor\n  edit account <name>       (58+) rename an account, reset its\n                                  password, or list its characters\n  edit help <topic>         (56+) help topic line editor\n  edit news <headline>      (56+) post a news item\n  edit wiznews <headline>   (56+) post to the immortal news channel\n  edit rules <n> <title>    (59+) write a numbered game rule\n\nLeaving off a noun''s own arguments shows its full usage (e.g. bare\n`edit room` still edits the room you''re standing in).', 'seed')
ON DUPLICATE KEY UPDATE `name` = `name`;

-- The `edit` master topic body changed (added `edit social`, 2026-07-20) --
-- the INSERT above is a no-op on the already-seeded row, so update it
-- explicitly, matching the `score`/`wiznet` precedent above.
UPDATE `help_topic` SET `body` = 'Usage: edit <noun> [args]\n\nOne entry point for every editor, replacing the old standalone ed*\ncommands. Each noun still needs its own level, same as before:\n\n  edit room [<vnum>]        (51+) menu-driven room builder\n  edit zone <zone number>   (51+) zone properties/builders\n  edit trigger ...          (51+) scripted mob/obj/room behavior --\n                                  see `help trigger`\n  edit social [name]        (55+) menu-driven social/emote editor\n  edit player <name>        (58+) menu-driven player editor\n  edit account <name>       (58+) rename an account, reset its\n                                  password, or list its characters\n  edit help <topic>         (56+) help topic line editor\n  edit news <headline>      (56+) post a news item\n  edit wiznews <headline>   (56+) post to the immortal news channel\n  edit rules <n> <title>    (59+) write a numbered game rule\n\nLeaving off a noun''s own arguments shows its full usage (e.g. bare\n`edit room` still edits the room you''re standing in).'
  WHERE `name` = 'edit' AND `updated_by` = 'seed';

-- The `score` body changed (now also shows alignment, 2026-07-10) -- the
-- INSERT above is a no-op on the already-seeded row, so update it explicitly.
UPDATE `help_topic` SET `body` = 'Usage: score\n\nShows your character sheet: name, level, experience, hit points,\nposition, attributes, handedness, gender, and alignment (good vs\nevil -- neutral until an immortal sets it, see `help set`). Limbs\nappear here only once they are hurt -- see `help limbs` for the full\nbreakdown.'
  WHERE `name` = 'score';

-- New topic: `trigger` (2026-07-11) -- the in-game scripting system.
INSERT INTO `help_topic` (`name`, `body`, `updated_by`) VALUES
('trigger', 'Usage: edit trigger <room|mob|obj> <vnum> <trigger_type> [match_text|chance]\n       edit trigger list <room|mob|obj> <vnum>\n       edit trigger delete <id>\n\nBuilder (51+) only: attaches scripted behavior to a room, mob, or\nobject prototype -- no recompile needed, unlike the classic spec\nproc approach. Trigger types:\n\n  room: enter (someone walks in), random (ambient, rolled every tick)\n  mob:  greet (someone walks into its room), speech (someone says a\n        matching keyword nearby -- give the keyword as the last\n        argument), death (it dies), random (ambient tick)\n  obj:  get (picked up), wear (worn)\n\nFor a `random` trigger, the last argument is the percent chance per\ntick (default 25). After the header line, you land in the line editor\nto write the script -- one action per line, `/s` saves:\n\n  echo <text>      -- to the triggering player only\n  echoroom <text>  -- to everyone else in the room\n  emote <text>     -- "<Name> <text>" to the whole room\n  teleport <vnum>  -- moves the triggering player to that room\n  give <vnum>      -- spawns that object into their inventory\n  damage <n>       -- deals n damage (never fatal on its own)\n  log <text>       -- a silent log entry, never broadcast\n\n`edit trigger list <type> <vnum>` shows what''s already attached;\n`edit trigger delete <id>` removes one.', 'seed')
ON DUPLICATE KEY UPDATE `name` = `name`;

-- Migration (user bug report, 2026-07-11: "i just tried to drink from a
-- fountain in the game, it failed with You don't see that here to
-- drink"): `drink` now also resolves a real OBJ_CAT_DRINK object
-- (fountains, drink containers), not just pee/blood puddles.
UPDATE `help_topic` SET `body` = 'Usage: drink <puddle|fountain>\n\nDrinks from a puddle on the ground -- a pool of pee, or a pool of\nblood left by a badly wounded limb -- with a chance of getting\npoisoned (a scare, not lethal on its own). Also works on a real\nfountain or drink container in the room: clean water, no poison,\nnever runs dry. Purely for flavor.'
  WHERE `name` = 'drink' AND `updated_by` = 'seed';

-- Migration: `toggle` no longer shows/sets game-wide switches at all --
-- they moved to the new `gametog` (58+) command.
UPDATE `help_topic` SET `body` = 'Usage: toggle [name]\n\nWith no argument, lists your PERSONAL on/off switches (color, hp, ...)\nand their current values. `toggle <name>` flips one (abbreviations\nwelcome). Global game-wide switches like multiplay live in the\nseparate `gametog` command (58+) instead.'
  WHERE `name` = 'toggle' AND `updated_by` = 'seed';

INSERT INTO `help_topic` (`name`, `body`, `updated_by`) VALUES
('gametog', 'Usage: gametog [name]\n\nView or flip GLOBAL game-wide switches --\ncurrently just multiplay (whether mortals may run more than one\ncharacter at once). Split out of `toggle` so a mortal never sees a\nswitch that could affect other players. See `help toggle` for the\npersonal-switch command.', 'seed')
ON DUPLICATE KEY UPDATE `name` = `name`;

-- Migration: `bug` now also mentions `edbug` as the resolve-in-place
-- alternative to `delbug`.
UPDATE `help_topic` SET `body` = 'Usage: bug <description>\n\nReports a bug to the immortals -- your name and the date are recorded\nwith it. Please be specific about what you did and what went wrong.\nImmortals can type bug with no argument to list outstanding (not yet\nresolved) reports. See `edbug` to resolve one and `delbug` to remove\none outright.'
  WHERE `name` = 'bug' AND `updated_by` = 'seed';

-- New topic: `edbug` (2026-07-11) -- resolve a bug in place, TODO.md-planned.
INSERT INTO `help_topic` (`name`, `body`, `updated_by`) VALUES
('edbug', 'Usage: edbug <id> [note for the submitter]\n\nMarks a bug report resolved WITHOUT deleting\nit (unlike `delbug`), so the report stays on file. If the submitter is\nonline right now, they get a live notice of the resolution -- and your\nnote, if you gave one. A resolved report no longer appears in the\noutstanding `bug` list. The id is the number shown beside each report\nin `bug`.', 'seed')
ON DUPLICATE KEY UPDATE `name` = `name`;

-- New topic: `snoop` (2026-07-11, user: "implement a snoop command like
-- sneezy, the command should be 59+ where you cant snoop anyone of same
-- or higher level").
INSERT INTO `help_topic` (`name`, `body`, `updated_by`) VALUES
('snoop', 'Usage: snoop [name]\n\nSilently watches everything a lower-level player sees AND everything\nthey type, mirrored to your own screen in real time (their typed\ncommands are prefixed "% " so you can tell them apart from their\noutput). You cannot snoop anyone of your own level or higher -- it\nfails outright. Only one outgoing snoop at a time; snooping a new\ntarget drops the old one. Bare `snoop` (no name) stops your current\nsnoop. The target is never told. Covert by design -- logged quietly,\nnever broadcast.', 'seed')
ON DUPLICATE KEY UPDATE `name` = `name`;

-- Migration: `snoop`'s body changed three times within the same session
-- (the level-gate phrasing moved out to the Syntax/Minimum Level footer,
-- user 2026-07-11: "take this phrasing out"; then bare `snoop` gained the
-- self-stop default; then the output mirror itself gained the same "% "
-- marker the typed-command mirror already had, user 2026-07-11: "add a
-- special prompt to messages sent in snoop (%) snooped content") -- the
-- INSERT above is a no-op on the already-seeded row from the first
-- deploy, so update it explicitly each time.
UPDATE `help_topic` SET `body` = 'Usage: snoop [name]\n\nSilently watches everything a lower-level player sees AND everything\nthey type, mirrored to your own screen in real time -- every mirrored\nline (their typed commands AND their own output alike) is prefixed\n"% " so you can tell it apart from your own screen. You cannot snoop\nanyone of your own level or higher -- it fails outright. Only one\noutgoing snoop at a time; snooping a new target drops the old one.\nBare `snoop` (no name) stops your current snoop. The target is never\ntold. Covert by design -- logged quietly, never broadcast.'
  WHERE `name` = 'snoop' AND `updated_by` = 'seed';

-- "Related: topic topic ..." footer (user 2026-07-11: "for help topics
-- both wizhelp and help add a line at the end for related topics") --
-- cmd_help.c strips a trailing "Related:" line out of the body and shows
-- it as its own footer, same convention as the leading "Usage:" line.
-- This pass also fixes a real bug: edroom/edzone/edplayer/edhelp/ednews/
-- edwiznews/edrules have been dead topics ever since the ed* commands
-- were unified into `edit <noun>` (2026-07-11) -- a comment near the
-- `edit` topic above claimed they'd been deleted, but no DELETE was ever
-- actually added. Renamed in place to "edit <noun>" (matching cmd_help.c's
-- new two-word lookup for "help edit <noun>") with their existing bodies
-- kept (still accurate per cmd_edroom.c/cmd_edzone.c/cmd_edplayer.c/
-- cmd_hedit.c/cmd_addnews.c/cmd_edwiznews.c/cmd_rules.c) rather than
-- discarded, each gaining a Related line.
--
-- Found 2026-07-12 (chasing an unrelated deploy failure): this rename
-- block was never actually idempotent. The top-level seed INSERT above
-- still carries rows named 'edroom'/'edzone'/'edplayer'/'edhelp'/
-- 'ednews'/'edwiznews'/'edrules' with `ON DUPLICATE KEY UPDATE name =
-- name` -- a no-op ONLY while a row still exists under that old name.
-- The very first time this file ran, the seven UPDATEs below renamed
-- those rows away, freeing up the old names -- so the SECOND time this
-- file runs, the top-level INSERT silently RE-CREATES a fresh row under
-- each old name (nothing there to no-op against anymore), and the
-- rename UPDATE below then collides with the already-renamed row
-- ('Duplicate entry ... for key PRIMARY'), aborting the whole file
-- partway through. Deleting the stale re-inserted duplicate first --
-- only when the correctly-renamed row already exists, so a genuinely
-- fresh database still renames normally -- makes every rerun a no-op
-- again, same intent as the file's other stale-seed-row DELETEs above.
DELETE FROM `help_topic` WHERE `name` = 'edroom' AND `updated_by` = 'seed'
  AND EXISTS (SELECT 1 FROM (SELECT `name` FROM `help_topic`) `t` WHERE `t`.`name` = 'edit room');
DELETE FROM `help_topic` WHERE `name` = 'edzone' AND `updated_by` = 'seed'
  AND EXISTS (SELECT 1 FROM (SELECT `name` FROM `help_topic`) `t` WHERE `t`.`name` = 'edit zone');
DELETE FROM `help_topic` WHERE `name` = 'edplayer' AND `updated_by` = 'seed'
  AND EXISTS (SELECT 1 FROM (SELECT `name` FROM `help_topic`) `t` WHERE `t`.`name` = 'edit player');
DELETE FROM `help_topic` WHERE `name` = 'edhelp' AND `updated_by` = 'seed'
  AND EXISTS (SELECT 1 FROM (SELECT `name` FROM `help_topic`) `t` WHERE `t`.`name` = 'edit help');
DELETE FROM `help_topic` WHERE `name` = 'ednews' AND `updated_by` = 'seed'
  AND EXISTS (SELECT 1 FROM (SELECT `name` FROM `help_topic`) `t` WHERE `t`.`name` = 'edit news');
DELETE FROM `help_topic` WHERE `name` = 'edwiznews' AND `updated_by` = 'seed'
  AND EXISTS (SELECT 1 FROM (SELECT `name` FROM `help_topic`) `t` WHERE `t`.`name` = 'edit wiznews');
DELETE FROM `help_topic` WHERE `name` = 'edrules' AND `updated_by` = 'seed'
  AND EXISTS (SELECT 1 FROM (SELECT `name` FROM `help_topic`) `t` WHERE `t`.`name` = 'edit rules');

UPDATE `help_topic` SET `name` = 'edit room',
  `body` = 'Usage: edit room [<vnum>]   (level 51+ builders)\n\nOpens the menu-driven room builder for the room you are standing in,\nor for <vnum> from anywhere. Edits are held in a working copy --\nnothing touches the DB until you Save. A level 51-54 builder can only\nedit a room in a zone they are assigned to; 55+ can edit any room, and\ncontent outside any zone is unrestricted for everyone.\n\n  1) Name          2) Description (/s saves, /a cancels, /b wipes,\n                       /f reflows to width)\n  3) Flags         4) Sector Type\n  5) Exits         6) Max Capacity\n  7) Room Height\n\nExits: pick a direction, then set its Target vnum, Door type, and\nConditions; a missing target room is created on save and the reverse\nexit auto-fixed.\n\n  C) Clear room out (blanks it, exits included)\n  S) Save    Q) Quit (warns on unsaved changes)\n\nRelated: zone trigger zonefile'
  WHERE `name` = 'edroom';

UPDATE `help_topic` SET `name` = 'edit zone',
  `body` = 'Usage: edit zone <zone number>\n\nMenu-driven zone editor (level 51+, but see below): change a zone''s\nname, enabled state, lifespan, and vnum range; assign or un-assign\nbuilders (selecting a builder already assigned un-assigns them); or\nforce a reset right now. A level 51-54 builder can only edit a zone\n(or a room in it) they are assigned to -- editing any other zone is\nrefused. 55+ can always edit anything, and content outside any zone is\nunrestricted for every builder.\n\nRelated: room zonefile zone'
  WHERE `name` = 'edzone';

UPDATE `help_topic` SET `name` = 'edit player',
  `body` = 'Usage: edit player <name>\n\nAdministrator (58+) only: a menu-driven editor for a player''s level,\nexperience, HP/max HP, attributes, gender, title, load room, and\nhandedness -- an admin superset of promote. Works on any player,\nonline or offline, by exact name. Pick a numbered field, enter a new\nvalue, then (S)ave to write it to the database (an online target is\nupdated immediately, no relog needed) or (Q)uit to discard.\n\nRelated: set promote'
  WHERE `name` = 'edplayer';

UPDATE `help_topic` SET `name` = 'edit help',
  `body` = 'Usage: edit help <topic>\n\nLevel 56+ only: edit (or create) a help topic in a line editor. Any\nexisting text is shown first; lines you type are appended. Finish\nwith `/s` to save, `/a` to abort, `/b` to blank the buffer and start\nover, or `/f` to reflow it to the display width. Topics are stored in\nthe database and shown by `help <topic>`. A trailing "Related: topic\ntopic ..." line (this one has one) is shown as its own footer instead\nof body text.\n\nRelated: news wiznews rules'
  WHERE `name` = 'edhelp';

UPDATE `help_topic` SET `name` = 'edit news',
  `body` = 'Usage: edit news <headline>\n\nLevel 56+ only: post a news item. The words after the command are the\nheadline; you then type the story into a line editor (`/s` saves, `/a`\naborts, `/b` blanks, `/f` reflows to width). Everyone can read it\nwith the `news` command. Headlines must be unique.\n\nRelated: wiznews help'
  WHERE `name` = 'ednews';

UPDATE `help_topic` SET `name` = 'edit wiznews',
  `body` = 'Usage: edit wiznews <headline>\n\nLevel 56+ only: post an item to the immortal news channel (read with\n`wiznews`). The words after the command are the headline; then type the\nstory into a line editor (`/s` saves, `/a` aborts, `/b` blanks,\n`/f` reflows to width).\n\nRelated: news help'
  WHERE `name` = 'edwiznews';

UPDATE `help_topic` SET `name` = 'edit rules',
  `body` = 'Usage: edit rules <number> <title>\n\nAdministrator (59+) only: writes or rewrites a numbered game rule. Give\nthe rule number and a title, then type the rule text into the line\neditor (/s saves, /a aborts, /b blanks, /f reflows to width). Players\nread rules with the rules command.\n\nRelated: help'
  WHERE `name` = 'edrules';

-- Related-footer additions across the rest of the topic set (guarded by
-- "NOT LIKE '%Related:%'" so a re-run never double-appends).
UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: room zone trigger player help news wiznews rules zonefile') WHERE `name` = 'edit' AND `body` NOT LIKE '%Related:%';
UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: room zonefile') WHERE `name` = 'trigger' AND `body` NOT LIKE '%Related:%';
UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: room trigger zonefile') WHERE `name` = 'zone' AND `body` NOT LIKE '%Related:%';
UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: zone room trigger') WHERE `name` = 'zonefile' AND `body` NOT LIKE '%Related:%';
UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: promote') WHERE `name` = 'set' AND `body` NOT LIKE '%Related:%';
UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: set') WHERE `name` = 'promote' AND `body` NOT LIKE '%Related:%';
UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: promote') WHERE `name` = 'mortal' AND `body` NOT LIKE '%Related:%';
UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: load promote') WHERE `name` = 'loadroom' AND `body` NOT LIKE '%Related:%';
UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: vnum zone') WHERE `name` = 'load' AND `body` NOT LIKE '%Related:%';
UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: load') WHERE `name` = 'purge' AND `body` NOT LIKE '%Related:%';
UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: zone edit') WHERE `name` = 'vnum' AND `body` NOT LIKE '%Related:%';

UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: exits scan') WHERE `name` = 'movement' AND `body` NOT LIKE '%Related:%';
UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: movement exits') WHERE `name` IN ('north','east','south','west','up','down','northeast','northwest','southeast','southwest') AND `body` NOT LIKE '%Related:%';
UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: movement scan') WHERE `name` = 'exits' AND `body` NOT LIKE '%Related:%';
UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: close exits') WHERE `name` = 'open' AND `body` NOT LIKE '%Related:%';
UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: open exits') WHERE `name` = 'close' AND `body` NOT LIKE '%Related:%';

UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: flee limbs positions') WHERE `name` IN ('attack','kill') AND `body` NOT LIKE '%Related:%';
UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: attack limbs positions') WHERE `name` = 'flee' AND `body` NOT LIKE '%Related:%';
UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: attack positions') WHERE `name` = 'limbs' AND `body` NOT LIKE '%Related:%';
UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: attack limbs') WHERE `name` = 'positions' AND `body` NOT LIKE '%Related:%';
UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: positions') WHERE `name` IN ('stand','sit','rest','sleep','wake') AND `body` NOT LIKE '%Related:%';

UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: get put containers') WHERE `name` = 'drop' AND `body` NOT LIKE '%Related:%';
UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: drop put containers') WHERE `name` = 'get' AND `body` NOT LIKE '%Related:%';
UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: get drop containers') WHERE `name` = 'put' AND `body` NOT LIKE '%Related:%';
UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: get put inventory') WHERE `name` = 'containers' AND `body` NOT LIKE '%Related:%';
UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: equipment containers') WHERE `name` = 'inventory' AND `body` NOT LIKE '%Related:%';
UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: wear wield inventory') WHERE `name` = 'equipment' AND `body` NOT LIKE '%Related:%';
UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: wield equipment') WHERE `name` = 'hold' AND `body` NOT LIKE '%Related:%';
UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: hold equipment') WHERE `name` = 'wield' AND `body` NOT LIKE '%Related:%';
UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: remove equipment') WHERE `name` = 'wear' AND `body` NOT LIKE '%Related:%';
UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: wear equipment') WHERE `name` = 'remove' AND `body` NOT LIKE '%Related:%';
UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: pee') WHERE `name` = 'drink' AND `body` NOT LIKE '%Related:%';
UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: drink') WHERE `name` = 'pee' AND `body` NOT LIKE '%Related:%';

UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: shout socials wiznet') WHERE `name` = 'say' AND `body` NOT LIKE '%Related:%';
UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: say socials toggle') WHERE `name` = 'shout' AND `body` NOT LIKE '%Related:%';
UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: say shout') WHERE `name` = 'socials' AND `body` NOT LIKE '%Related:%';
UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: system wiznews') WHERE `name` = 'wiznet' AND `body` NOT LIKE '%Related:%';
UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: wiznet') WHERE `name` = 'system' AND `body` NOT LIKE '%Related:%';
UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: news wiznews') WHERE `name` = 'catchup' AND `body` NOT LIKE '%Related:%';
UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: toggle') WHERE `name` = 'newbie' AND `body` NOT LIKE '%Related:%';

UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: wiznews edit') WHERE `name` = 'news' AND `body` NOT LIKE '%Related:%';
UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: news edit') WHERE `name` = 'wiznews' AND `body` NOT LIKE '%Related:%';
UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: gametog prompt') WHERE `name` = 'toggle' AND `body` NOT LIKE '%Related:%';
UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: toggle multiplay') WHERE `name` = 'gametog' AND `body` NOT LIKE '%Related:%';
UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: gametog toggle') WHERE `name` = 'multiplay' AND `body` NOT LIKE '%Related:%';

UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: bamfout goto poofin') WHERE `name` = 'bamfin' AND `body` NOT LIKE '%Related:%';
UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: bamfin goto poofout') WHERE `name` = 'bamfout' AND `body` NOT LIKE '%Related:%';
UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: poofout movement') WHERE `name` = 'poofin' AND `body` NOT LIKE '%Related:%';
UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: poofin movement') WHERE `name` = 'poofout' AND `body` NOT LIKE '%Related:%';
UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: bamfin bamfout transfer') WHERE `name` = 'goto' AND `body` NOT LIKE '%Related:%';
UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: goto') WHERE `name` = 'transfer' AND `body` NOT LIKE '%Related:%';
UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: goto snoop') WHERE `name` = 'switch' AND `body` NOT LIKE '%Related:%';

UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: setsev system') WHERE `name` = 'log' AND `body` NOT LIKE '%Related:%';
UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: log') WHERE `name` = 'setsev' AND `body` NOT LIKE '%Related:%';
UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: wiznet log') WHERE `name` = 'snoop' AND `body` NOT LIKE '%Related:%';
UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: who log') WHERE `name` = 'users' AND `body` NOT LIKE '%Related:%';
UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: users score') WHERE `name` = 'who' AND `body` NOT LIKE '%Related:%';
UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: users') WHERE `name` = 'mudstats' AND `body` NOT LIKE '%Related:%';
UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: system') WHERE `name` = 'exec' AND `body` NOT LIKE '%Related:%';

UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: limbs positions who') WHERE `name` = 'score' AND `body` NOT LIKE '%Related:%';
UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: who score') WHERE `name` = 'title' AND `body` NOT LIKE '%Related:%';
UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: colors toggle') WHERE `name` = 'color' AND `body` NOT LIKE '%Related:%';
UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: color') WHERE `name` = 'colors' AND `body` NOT LIKE '%Related:%';
UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: score') WHERE `name` IN ('appearance','gender') AND `body` NOT LIKE '%Related:%';
UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: toggle score') WHERE `name` = 'prompt' AND `body` NOT LIKE '%Related:%';

UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: delbug edbug') WHERE `name` = 'bug' AND `body` NOT LIKE '%Related:%';
UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: bug edbug') WHERE `name` = 'delbug' AND `body` NOT LIKE '%Related:%';
UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: bug delbug') WHERE `name` = 'edbug' AND `body` NOT LIKE '%Related:%';
UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: delidea') WHERE `name` = 'idea' AND `body` NOT LIKE '%Related:%';
UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: idea') WHERE `name` = 'delidea' AND `body` NOT LIKE '%Related:%';

UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: wizhelp') WHERE `name` = 'help' AND `body` NOT LIKE '%Related:%';
UPDATE `help_topic` SET `body` = CONCAT(`body`, '\n\nRelated: help edit') WHERE `name` = 'wizhelp' AND `body` NOT LIKE '%Related:%';

-- Detailed help catch-up (user 2026-07-12: "catch up on the help file
-- entries... i want very detailed help files. Especially wiz* help
-- files. i want it so a first time player of this game will feel
-- comfortable playing because he knows where to find game play
-- information and administration detailed so new immortals can know
-- what commands do and why we use them"). Two new orientation topics
-- (`playing` for a first-time player, `administration` for a new
-- immortal, both prose-only -- no command-table entry, so no Syntax/
-- Minimum Level footer, same pattern as `movement`/`containers`) plus
-- every command that had gone entirely without a help topic. `help`/
-- `wizhelp`'s own footers point at these two below. This is a first
-- substantial pass, not a claim that every existing terse topic has
-- been rewritten -- those can keep deepening incrementally from here.
INSERT INTO `help_topic` (`name`, `body`, `updated_by`) VALUES
('playing', 'New to TobinMUD? Start here.\n\nLOOK AROUND: `look` shows your surroundings, and runs automatically\nwhenever you enter a room. `exits` lists the ways out, and a direction\n(north, east, up, ...) walks you through one -- single letters (n, e,\nu) work too. See `help movement`.\n\nYOUR CHARACTER: `score` is your character sheet -- level, experience,\nhit points, position, attributes, gender, appearance. `inventory`\nlists what you carry loose, `equipment` what you have worn or held.\n`limbs` shows the health of all thirteen of your body parts, once any\nof them are hurt.\n\nTALKING: `say` (shorthand ''hello) speaks to your room, `shout` reaches\neveryone playing anywhere, `tell <name> <msg>` and `whisper <name>\n<msg>` are private, and `socials` lists emotes like smile and wave.\nEvery new character starts on the newbie channel -- ask questions\nthere without fear of looking silly (`help newbie`).\n\nFIGHTING: `attack`/`kill`/`hit <target>` starts a fight; every hit\nlands on a specific limb, and losing a major one (head, neck, waist,\nor torso) is fatal, so do not pick fights above your weight. `consider\n<target>` sizes one up first, in plain English. `flee` is a desperate\nway out if a fight turns against you.\n\nCLASS AND SKILLS: your class and race were chosen at creation and\ndetermine what you can learn. `skills` lists your class''s full roster\nand marks what is usable yet and why not, if it isn''t. Reaching a\nskill or spell''s level requirement is not enough on its own -- most\nalso need practicing at a guildmaster of your own class first, see\n`help practice`. Mages and Druids `cast`; Clerics `pray`.\n\nLEAVING THE GAME: `rent` is the safe way to log off -- your belongings\nstay with you, and you even heal while you are away. Plain `quit!`\n(must be typed exactly, with the !) is the risky way: it drops\neverything you are carrying on the floor right where you stand.\nSimply losing your connection is also safe -- your character stays\nright where they were, marked (linkdead), until you reconnect.\n\nIF SOMETHING IS WRONG: `bug <description>` reports a problem, `idea\n<description>` suggests a feature, and `rules` lists the game''s\nrules -- please read them. `news` covers what has changed recently.\n\nEvery command has its own detailed help -- type `help <command>` any\ntime, and `help` alone lists everything available to you.\n\nRelated: score limbs skills practice rent quit! bug idea rules news socials', 'seed'),
('administration', 'New immortal? This is Tobin''s admin philosophy in one place -- not\njust what each command does, but why it sits at the level it does.\n\nTHE LADDER: level 51 is your first immortal rank, and it already opens\ngoto, load, wiznet, system, and every zone/room building tool --\nbuilding content is the most common immortal work and the least\ndangerous, so it is not gated further. Higher levels unlock\nprogressively more sensitive power: 54 the game log, 55 `stat` (see\nevery raw field of any prototype) and zone editing with no assignment\nrestriction, 56 the content editors (`edit help/news/wiznews/rules`),\n58 anything that touches a PLAYER''s own data (`promote`, `edplayer`,\n`set`, `users`, `purge linkdead`) plus `test`, 59 anything that\ndeletes or renumbers another immortal''s work (`delbug`, `delidea`,\n`edrules`) plus `multiplay` and `snoop`, and 60 -- Implementor -- the\ntools that reach outside the running game entirely or reshape it\nwholesale (`exec`, `balance`, `egotrip`, `copyover`). The pattern: the\nfurther a command reaches beyond your own building, the higher it\nsits. `wizhelp` only ever shows what YOUR level already grants --\nwhat a future promotion unlocks stays a surprise until it happens.\n\nBUILDING: `goto <vnum>` gets you there, `edroom`/`edzone` shape rooms\nand zones, `load <mob|obj> <vnum|name>` spawns a test copy, `zonefile\ncreate` snapshots what you have built back into real reset data so it\nsurvives a reboot, `vnum <room|obj|mob> <pattern>` searches prototypes\nby name or number, and `purge` cleans a room back out. A level 51-54\nbuilder can only edit a zone they are assigned to (see `edzone`); 55+\ncan edit anything, zoned or not.\n\nRUNNING THE PLAYERBASE: `promote`/`edplayer`/`set` change a player''s\nstats -- `edplayer` is the full menu, `set` is one field at a time,\n`promote` is level only. `users` shows every live connection, the\nadmin''s who-is-really-here view. `snoop` mirrors a lower-level\nplayer''s whole session to your screen, covertly. `mortal`/`immort`\nlets you walk the world as an ordinary player for testing without\nrisking your real rank -- it is kept safe even through death or a\nlogout.\n\nWHY THE DEBUG TOOLS EXIST: real combat, world ticks, and puddle decay\nare all slow and randomized by design -- great for players, useless\nfor testing. `hurtlimb <target> <limb> <hp>` sets a limb''s HP\ndirectly so a decapitation test does not need thirty real rounds of\ncombat luck. `aitick [count]` forces mob-AI/decay/trigger ticks that\nwould otherwise only fire once a minute. `stat <obj|mob|room> <vnum>`\ndumps a prototype''s entire row, decoded, instead of a raw database\nquery. `balance` tunes class/race combat multipliers gamewide without\na recompile, because balance is discovered by playtesting, not decided\nonce and left alone. `egotrip blast <target>` is a quick, non-lethal\nHP-halving tool for testing anything that depends on a character being\nbadly hurt. `test` shows what automated smoke test is currently\nexercising the server, if any.\n\nTHE DANGEROUS ONES: `exec <command>` runs a real shell command on the\nhost machine -- fenced by a blocklist, a timeout, and full logging, but\nstill not a toy. `copyover` reboots the running server in place\nwithout disconnecting anyone, so a code deploy does not mean kicking\neveryone off first; it fails safely (the world keeps running\nunchanged) if anything about the handoff goes wrong. Both are\nImplementor-only and every use is logged.\n\nEvery command has its own detailed help -- `help <command>`.\n\nRelated: wizhelp goto edroom edzone stat hurtlimb aitick exec copyover', 'seed'),
('stat', 'Usage: stat <obj|mob|room> <vnum>\n\nBuilder tool (level 55+): dumps EVERY column of that exact prototype\nrow -- name/short/long descriptions, every stat and flag -- decoded to\nreadable text wherever a raw number alone would not mean anything\n(wear/action flags as bracketed names, class/race/sector as words, not\nnumbers). A room additionally lists its exits; an object its\nhitroll/damroll-style affects. Unlike `vnum` (which searches BY NAME\nand shows one summary line per match), `stat` needs an exact vnum but\nthen holds nothing back.', 'seed'),
('save', 'Usage: save\n\nSaves your character right now -- attributes, level/experience/HP, and\neverything you are carrying. You do not strictly need to: quitting or\ndying already saves everything automatically. `save` exists purely for\nthe reassurance of doing it yourself mid-session.\n\nRelated: rent quit!', 'seed'),
('rent', 'Usage: rent\n\nThe RECOMMENDED way to leave the game -- stores your belongings\nsafely and ends your session cleanly, and you will find yourself\nhealed up when you return, roughly in proportion to how long you were\naway. Refused while fighting. Compare `quit!`, which is faster but\ndrops everything you are carrying on the floor where you stand.\n\nRelated: quit! save', 'seed'),
('cast', 'Usage: cast <spell>\n\nMages and Druids only: casts a spell from your class''s roster (see\n`skills`). You need a spell component -- any carried item keyworded\n"component" -- on hand; it is consumed on a successful cast. Also\nneeds enough practiced discipline in that spell''s tier -- see `help\npractice`. Clerics use `pray` instead.\n\nRelated: pray practice skills affects', 'seed'),
('pray', 'Usage: pray <spell> [target]\n\nClerics only: the Cleric equivalent of `cast` -- draws on your class''s\nroster of prayers instead of spells. You need a holy symbol -- any\ncarried item keyworded "symbol" -- on hand, consumed on every\nsuccessful prayer. A healing prayer can target someone else in the\nroom (`pray heal light <name>`), or, left blank, yourself. See `help\ncontinue` to repeat a heal automatically until it is no longer needed.\n\nRelated: cast continue practice skills affects', 'seed'),
('practice', 'Usage: practice                          (status)\n       practice <discipline>             (skill listing + proficiency)\n       practice <discipline> <count>     (spend points, at a guildmaster)\n       practice <class> [<discipline>]   (see ANY class''s skill listing)\n\nThree disciplines exist -- Basic, Combat, Advanced -- each raised by\nspending practice points at its own tier of guildmaster: Basic at\nlevel-51 guildmasters (`goto guildmaster`), Combat at level-80\n(`goto combat`), Advanced at level-100 (find them yourself). You earn\npractice points on level-up (scaled by your Wisdom attribute). Your\nown class name works as a synonym for Basic (matching how `skills`\nlabels that tier, e.g. "Warrior Skills"). Advanced is locked until\nBOTH Basic and Combat reach 100%.\n\n`practice <discipline>` -- e.g. `practice combat` -- shows that\ndiscipline''s skill/spell listing with each accessible skill''s own\nindividual PROFICIENCY percentage (see `help skills`), ANYWHERE, no\nguildmaster required. Standing in front of the matching guildmaster\nadds a reminder to spend points there.\n\n`practice <discipline> <count>` -- e.g. `practice combat 5` -- is the\nonly form that actually SPENDS points, raising the discipline\npercentage itself (which acts as the ceiling every skill''s own\nproficiency climbs toward through use). Each point spent awards a\nrandom 1-2%, capped at 100%; the spend stops early if you run out of\npoints or hit the cap. This form needs the matching guildmaster\npresent.\n\n`practice <class>` -- e.g. `practice warrior` -- browses ANY class''s\nBasic listing, not just your own (defaults to Basic; add a tier word,\ne.g. `practice warrior combat`, for Combat/Advanced). Someone else''s\nclass shows as a plain reference (skill names + minimum level, no\nproficiency -- you don''t have access) and can never be spent on. An\nimmortal granted every class at once sees the full listing for any\nclass they ask for instead, since they genuinely have access to all\nof them.\n\nSyntax: practice              (status: all three percentages + points)\n        practice basic         (Basic''s skill listing + proficiency)\n        practice combat 7      (spend up to 7 points on Combat)\n        practice mage           (see the Mage skill listing, read-only)\n\nMinimum Level: 1\n\nRelated: skills cast pray goto', 'seed'),
('skills', 'Usage: skills\n\nShows your class''s full roster of skills and spells, organized into\nthree tiers -- Combat, <Class> Skills, and Advanced <Class> Skills --\neach marked with whether you know it yet, and if not, exactly why\n(level too low, or discipline not yet practiced -- see `help\npractice`). Immortals see every class''s full roster, not just their\nown, and bypass both the level and discipline gates entirely.\n\nRelated: practice cast pray', 'seed'),
('affects', 'Usage: affects\n\nLists any temporary buffs or debuffs currently on you, each with its\nown countdown -- for example a Cleric''s `pray sanctuary`, which halves\nincoming damage for a while and wears off with a message once its\ntime is up.\n\nRelated: pray', 'seed'),
('consider', 'Usage: consider <target>   |   consider self\n\nSizes up a fight before you start one -- compares levels and, for\n`consider self`, your own equipment (armor class) too -- and gives a\nplain-English verdict ("this looks like a fair fight", "you would be\ncrazy to try this") instead of raw numbers. Considering an immortal\n(as a mortal) gets a suitably humbling response.\n\nRelated: attack flee limbs', 'seed'),
('continue', 'Usage: continue\n\nRepeats your last `pray heal <tier> <target>` prayer automatically,\nround after round, until the target is fully healed or you run out of\nholy symbols. Refused if you have not prayed a heal yet, or once the\ntarget is already at full health.\n\nRelated: pray', 'seed'),
('examine', 'Usage: examine <target>\n\nA synonym for `look at <target>` -- identical output, different word,\nfor players who expect "examine" from other MUDs.\n\nRelated: look', 'seed'),
('show', 'Usage: show <item> <person>\n\nHolds an item up for someone else in the room to see, without handing\nit over -- they see its description, you keep the item. Compare\n`give`, which actually transfers ownership.', 'seed'),
('sip', 'Usage: sip <puddle|fountain>\n\nTastes a liquid -- a puddle on the ground or a real fountain/drink\ncontainer in the room -- without committing to a full `drink`. Much\nlower risk than a full drink of whatever it might expose you to.\n\nRelated: drink', 'seed'),
('tell', 'Usage: tell <name> <message>\n\nA private message to anyone playing, anywhere in the game -- no need\nto share a room. Compare `whisper`, which only reaches someone in your\nown room but at least lets bystanders know a conversation is\nhappening.\n\nRelated: whisper say', 'seed'),
('whisper', 'Usage: whisper <name> <message>\n\nA private message to someone in your own room -- everyone else there\nsees that a conversation happened, but not what was said. Compare\n`tell`, which reaches anyone anywhere but gives bystanders no hint at\nall.\n\nRelated: tell say', 'seed'),
('balance', 'Usage: balance class <name>   |   balance race <name>\n\nImplementor only (level 60+): a menu-driven editor for four gamewide\ncombat modifiers per class or race -- HP multiplier, damage multiplier,\nto-hit modifier, AC modifier. Every class and race starts perfectly\nneutral (1.00x/1.00x/+0/+0); nothing changes until someone actually\nbalances one. Saved changes apply immediately, server-wide, with no\nrestart needed -- this exists so combat balance can be tuned by\nplaytesting over time, not just guessed at once and left alone.', 'seed'),
('egotrip', 'Usage: egotrip blast <target>\n\nHits a target with a non-lethal bolt of\nlightning, halving their current HP (never below 1). A quick,\ndeterministic way to test anything that depends on a character being\nbadly hurt, without waiting on a real fight''s randomness. Named for\nthe original Sneezy `egotrip` command, which had many more\nsubcommands -- only `blast` is ported so far; the rest depend on\nsystems (disease, mob AI hate/aggro) Tobin has not built yet.', 'seed'),
('settrap', 'Usage: settrap <direction>\n\nA Thief who knows "set trap (door)" rigs a trap on a closed door in\nthat direction. Whoever walks through it next without knowing "detect\ntrap" springs it -- a one-shot hit to a random limb -- while a Thief\nwho does know that skill safely steps around it, leaving it rigged for\nthe next person.\n\nRelated: disarmtrap', 'seed'),
('disarmtrap', 'Usage: disarmtrap <direction>\n\nA Thief who knows "disarm trap" safely removes a trap rigged on a\ndoor in that direction, with no risk of springing it.\n\nRelated: settrap', 'seed'),
('peek', 'Usage: peek <target>\n\nClasses: Thief (Combat, level 1)\n\nA Thief who knows "peek" attempts to covertly see what someone in\nthe room is carrying (loose inventory only -- not what they have\nworn or wielded; see `look <target>` for that). Success depends on\nyour proficiency with the skill (see `skills`): a clean success goes\nunnoticed, but a fumble tips the target off that someone just tried.\nRequires: nothing extra -- see `help peek`\nRelated: look inventory skills', 'seed'),
('hurtlimb', 'Usage: hurtlimb <target> <limb> <hp>\n\nImmortal debug tool: sets a target''s limb HP directly, bypassing real\ncombat entirely. Exists because testing decapitation/limb-loss\nbehavior through actual combat would mean waiting on real damage\nrolls to land in exactly the right spot -- this makes it instant and\ndeterministic. Destroying a major limb (head, neck, waist, body) this\nway is still instant death, exactly as it is in real combat.\n\nRelated: limbs aitick', 'seed'),
('aitick', 'Usage: aitick [count]\n\nImmortal debug tool: forces `count` (default 1, max 100) mob-AI /\npuddle-decay / random-trigger ticks to run right now, synchronously.\nThese all normally only fire on the real ~60-second world pulse -- far\ntoo slow to wait on live, and far too slow for an automated test -- so\nthis collapses real time into an instant for testing wander/scavenge/\ndecay/random-trigger behavior.\n\nRelated: hurtlimb', 'seed'),
('immort', 'Usage: immort\n\nReclaims your true immortal rank after `mortal` set it aside. Your\nreal level is kept safe the whole time you are mortal -- even through\ndeath or a logout -- so `immort` always restores exactly what you had.\n\nRelated: mortal', 'seed'),
('test', 'Usage: test\n\nShows the name of whatever automated smoke test is currently running\nagainst this server, if any -- handy for watching a test sweep from\nin-game instead of only the console.', 'seed'),
('copyover', 'Usage: copyover\n\nReboots the server in place, from a\nfreshly rebuilt binary, without disconnecting a single player --\nevery connection is handed off to the new process. Exists so a code\ndeploy does not mean kicking everyone off first. Fails safely (the\nworld keeps running, unchanged) if the recovery file cannot be written\nor the new process cannot start.', 'seed')
ON DUPLICATE KEY UPDATE `name` = `name`;

-- Point `help`/`wizhelp`'s own footers at the two new orientation topics
-- above -- the single most discoverable spot for a first-time reader.
-- Both topics already carry an earlier-migration Related line (`Related:
-- wizhelp` / `Related: help edit`), so a CONCAT-if-missing guard (the
-- pattern used everywhere else in this file) would either no-op or
-- double up a second Related line -- explicit full-body replacement
-- instead, same as the many other seed-only body corrections above.
UPDATE `help_topic` SET `body` = 'Usage: help [topic]\n\nWith no argument, lists every command available to you. With a topic\n(any command name, abbreviations welcome), shows its full help text.\nNew here? Start with `help playing`.\n\nRelated: playing wizhelp'
  WHERE `name` = 'help' AND `updated_by` = 'seed';
UPDATE `help_topic` SET `body` = 'Usage: wizhelp\n\nImmortals only: lists the immortal-only commands, with the minimum\nlevel each one requires. New immortal? Start with `help\nadministration`.\n\nRelated: administration help edit'
  WHERE `name` = 'wizhelp' AND `updated_by` = 'seed';

-- `stat player <name>` (user 2026-07-12: "stat player <name> to stat a
-- player") -- full-body replacement, same guarded pattern as help/wizhelp
-- above, since ON DUPLICATE KEY UPDATE name=name would otherwise leave
-- the old obj/mob/room-only wording in place for anyone who already has
-- this row seeded.
UPDATE `help_topic` SET `body` = 'Usage: stat <obj|mob|room> <vnum>   |   stat player <name>\n\nDumps EVERY column of that exact prototype\nrow -- name/short/long descriptions, every stat and flag -- decoded to\nreadable text wherever a raw number alone would not mean anything\n(wear/action flags as bracketed names, class/race/sector as words, not\nnumbers). A room additionally lists its exits; an object its\nhitroll/damroll-style affects. Unlike `vnum` (which searches BY NAME\nand shows one summary line per match), `stat` needs an exact vnum but\nthen holds nothing back.\n\n`stat player <name>` is the one form keyed by name instead of vnum --\nit looks a player up across all three of their tables (identity,\nlevel/experience/HP/alignment, and attributes) and dumps each as its\nown section, decoded the same way (class/race/gender as words, plus an\nalignment tier alongside the raw number).'
  WHERE `name` = 'stat' AND `updated_by` = 'seed';

-- `goto guildmaster`/`goto rent`/`goto surplus` (user 2026-07-12: "add a
-- goto class function that mortals can do to help find thier
-- guildmasters"; follow-up: "goto guildmaster should give them
-- directions, not transfer. also add a goto rent, goto surplus for now
-- with goto expanding for mortals") -- full-body replacement, same
-- guarded pattern as above.
UPDATE `help_topic` SET `body` = 'Usage: goto guildmaster|rent|surplus   |   goto <room vnum | player> (immortals)\n\nThree landmark forms are open to everyone: `goto guildmaster` reports\nwalking directions to the nearest guildmaster of your own class, `goto\nrent` to the inn, and `goto surplus` to the surplus store -- none of\nthese teleport you, they just tell you the way (shortest real path,\nby actual room exits). Standing right there already just says so.\nEvery other form of `goto` is immortals only, and DOES teleport -- a\nroom by its vnum, or another online player by name (you land in their\nroom). Useful vnums: 0 (The Void), 1 (Imperia).\n\nRelated: practice rent bamfin bamfout transfer'
  WHERE `name` = 'goto' AND `updated_by` = 'seed';

-- user 2026-07-12: remove "Sneezy always warned about" from `playing`'s
-- quit! paragraph -- full-body replacement, same guarded pattern as above.
UPDATE `help_topic` SET `body` = 'New to TobinMUD? Start here.\n\nLOOK AROUND: `look` shows your surroundings, and runs automatically\nwhenever you enter a room. `exits` lists the ways out, and a direction\n(north, east, up, ...) walks you through one -- single letters (n, e,\nu) work too. See `help movement`.\n\nYOUR CHARACTER: `score` is your character sheet -- level, experience,\nhit points, position, attributes, gender, appearance. `inventory`\nlists what you carry loose, `equipment` what you have worn or held.\n`limbs` shows the health of all thirteen of your body parts, once any\nof them are hurt.\n\nTALKING: `say` (shorthand ''hello) speaks to your room, `shout` reaches\neveryone playing anywhere, `tell <name> <msg>` and `whisper <name>\n<msg>` are private, and `socials` lists emotes like smile and wave.\nEvery new character starts on the newbie channel -- ask questions\nthere without fear of looking silly (`help newbie`).\n\nFIGHTING: `attack`/`kill`/`hit <target>` starts a fight; every hit\nlands on a specific limb, and losing a major one (head, neck, waist,\nor torso) is fatal, so do not pick fights above your weight. `consider\n<target>` sizes one up first, in plain English. `flee` is a desperate\nway out if a fight turns against you.\n\nCLASS AND SKILLS: your class and race were chosen at creation and\ndetermine what you can learn. `skills` lists your class''s full roster\nand marks what is usable yet and why not, if it isn''t. Reaching a\nskill or spell''s level requirement is not enough on its own -- most\nalso need practicing at a guildmaster of your own class first, see\n`help practice`. Mages and Druids `cast`; Clerics `pray`.\n\nLEAVING THE GAME: `rent` is the safe way to log off -- your belongings\nstay with you, and you even heal while you are away. Plain `quit!`\n(must be typed exactly, with the !) is the risky way: it drops\neverything you are carrying on the floor right where you stand.\nSimply losing your connection is also safe -- your character stays\nright where they were, marked (linkdead), until you reconnect.\n\nIF SOMETHING IS WRONG: `bug <description>` reports a problem, `idea\n<description>` suggests a feature, and `rules` lists the game''s\nrules -- please read them. `news` covers what has changed recently.\n\nEvery command has its own detailed help -- type `help <command>` any\ntime, and `help` alone lists everything available to you.\n\nRelated: score limbs skills practice rent quit! bug idea rules news socials'
  WHERE `name` = 'playing' AND `updated_by` = 'seed';

-- Practice system redesign (2026-07-17): three disciplines, practice points
-- as resource, combat guildmaster tier, `goto combat`.
UPDATE `help_topic` SET `body` = 'Usage: practice                          (status)\n       practice <discipline>             (skill listing + proficiency)\n       practice <discipline> <count>     (spend points, at a guildmaster)\n\nThree disciplines exist -- Basic, Combat, Advanced -- each raised by\nspending practice points at its own tier of guildmaster: Basic at\nlevel-51 guildmasters (`goto guildmaster`), Combat at level-80\n(`goto combat`), Advanced at level-100 (find them yourself). You earn\npractice points on level-up (scaled by your Wisdom attribute). Your\nown class name works as a synonym for Basic (matching how `skills`\nlabels that tier, e.g. "Warrior Skills"). Advanced is locked until\nBOTH Basic and Combat reach 100%.\n\n`practice <discipline>` -- e.g. `practice combat` -- shows that\ndiscipline''s skill/spell listing with each accessible skill''s own\nindividual PROFICIENCY percentage (see `help skills`), ANYWHERE, no\nguildmaster required. Standing in front of the matching guildmaster\nadds a reminder to spend points there.\n\n`practice <discipline> <count>` -- e.g. `practice combat 5` -- is the\nonly form that actually SPENDS points, raising the discipline\npercentage itself (which acts as the ceiling every skill''s own\nproficiency climbs toward through use). Each point spent awards a\nrandom 1-2%, capped at 100%; the spend stops early if you run out of\npoints or hit the cap. This form needs the matching guildmaster\npresent.\n\nSyntax: practice              (status: all three percentages + points)\n        practice basic         (Basic''s skill listing + proficiency)\n        practice combat 7      (spend up to 7 points on Combat)\n\nMinimum Level: 1\n\nRelated: skills cast pray goto'
  WHERE `name` = 'practice' AND `updated_by` = 'seed';

UPDATE `help_topic` SET `body` = 'Usage: goto guildmaster|combat|rent|surplus|<classname>   |   goto <room vnum | player> (immortals)\n\nLandmark forms are open to everyone: `goto guildmaster` reports\nwalking directions to the nearest Basic guildmaster of your own class,\n`goto combat` to the nearest Combat guildmaster, `goto rent` to the\ninn, and `goto surplus` to the surplus store -- none of these teleport\nyou, they just tell you the way (shortest real path, by actual room\nexits). `goto advanced` deliberately refuses -- finding the Advanced\nguildmaster is part of the challenge. Standing right there already\njust says so.\n`goto <classname>` (e.g. `goto thief`) gives directions to that\nNAMED class''s own Basic guildmaster, not just your own class -- handy\nfor checking on another class''s trainer without switching characters.\nEvery other form of `goto` is immortals only, and DOES teleport -- a\nroom by its vnum, or another online player by name (you land in their\nroom). Useful vnums: 0 (The Void), 1 (Imperia).\n\nRelated: practice rent bamfin bamfout transfer'
  WHERE `name` = 'goto' AND `updated_by` = 'seed';

UPDATE `help_topic` SET `body` = 'Usage: skills\n\nShows your class''s full roster of skills and spells, organized into\nthree tiers -- Combat, <Class> Skills, and Advanced <Class> Skills --\nwith your current discipline percentages (Basic, Combat, Advanced) at\nthe top. Each skill is marked with whether you know it yet, and if\nnot, exactly why (level too low, or discipline not yet practiced --\nsee `help practice`). Advanced skills are locked until Basic AND\nCombat both reach 100%. Immortals see every class''s full roster, not\njust their own, and bypass both the level and discipline gates\nentirely.\n\nRelated: practice cast pray'
  WHERE `name` = 'skills' AND `updated_by` = 'seed';

UPDATE `help_topic` SET `body` = 'Usage: balance class <name>   |   balance race <name>   |   balance wisdom [<value>]\n\nA menu-driven editor for four gamewide\ncombat modifiers per class or race -- HP multiplier, damage multiplier,\nto-hit modifier, AC modifier. Every class and race starts perfectly\nneutral (1.00x/1.00x/+0/+0); nothing changes until someone actually\nbalances one. Saved changes apply immediately, server-wide, with no\nrestart needed -- this exists so combat balance can be tuned by\nplaytesting over time, not just guessed at once and left alone.\n\n`balance wisdom` shows the current wisdom-to-practice-points scalar\n(default 1.0). `balance wisdom <value>` sets it -- higher means\nmore practice points per level for high-Wisdom characters. Updates\nimmediately, persisted in game_config.'
  WHERE `name` = 'balance' AND `updated_by` = 'seed';

-- Per-skill proficiency (learn-by-doing, user 2026-07-17): `practice`
-- only gates ACCESS to a tier; each individual skill/spell also has its
-- own percentage now, separate and learned by using it.
UPDATE `help_topic` SET `body` = 'Usage: cast <spell>\n\nMages and Druids only: casts a spell from your class''s roster (see\n`skills`). You need a spell component -- any carried item keyworded\n"component" -- on hand; it is consumed on every attempt, success or\nfail. Also needs enough practiced discipline in that spell''s tier --\nsee `help practice`.\n\nBeyond that access gate, each spell has its OWN proficiency\npercentage (shown in `skills`), separate from your discipline\npercentage -- a freshly-accessible spell starts barely competent and\nclimbs toward a ceiling set by your discipline percentage every time\nyou attempt it, win or lose. A low-proficiency spell often fizzles\n("You fumble the casting..."); a well-practiced one rarely does.\nClerics use `pray` instead.\n\nRelated: pray practice skills affects'
  WHERE `name` = 'cast' AND `updated_by` = 'seed';

UPDATE `help_topic` SET `body` = 'Usage: pray <spell> [target]\n\nClerics only: the Cleric equivalent of `cast` -- draws on your class''s\nroster of prayers instead of spells. You need a holy symbol -- any\ncarried item keyworded "symbol" -- on hand, consumed on every\nattempt, success or fail. A healing prayer can target someone else in\nthe room (`pray heal light <name>`), or, left blank, yourself. See\n`help continue` to repeat a heal automatically until it is no longer\nneeded.\n\nBeyond the class/level/discipline access gate, each prayer has its\nOWN proficiency percentage (shown in `skills`), separate from your\ndiscipline percentage -- it climbs toward a ceiling set by your\ndiscipline percentage every time you pray it, win or lose. A\nlow-proficiency prayer often fizzles ("You fumble the prayer...");\na well-practiced one rarely does.\n\nRelated: cast continue practice skills affects'
  WHERE `name` = 'pray' AND `updated_by` = 'seed';

UPDATE `help_topic` SET `body` = 'Usage: skills\n\nShows your class''s full roster of skills and spells, organized into\nthree tiers -- Combat, <Class> Skills, and Advanced <Class> Skills --\nwith your current discipline percentages (Basic, Combat, Advanced) at\nthe top. Each skill is marked with whether you have ACCESS to it yet,\nand if not, exactly why (level too low, or discipline not yet\npracticed -- see `help practice`). Advanced skills are locked until\nBasic AND Combat both reach 100%.\n\nEvery accessible skill also shows its own individual PROFICIENCY\npercentage in brackets (e.g. "[34%]") -- a separate number from your\ndiscipline percentage, learned by actually using the skill\n(`cast`/`pray`/`settrap`/`disarmtrap`/dual wield). It climbs toward a\nceiling set by your discipline percentage for that skill''s tier, and\ngates how often the skill actually succeeds.\n\nImmortals see every class''s full roster, not just their own, and\nbypass both the level and discipline gates entirely.\n\nRelated: practice cast pray'
  WHERE `name` = 'skills' AND `updated_by` = 'seed';

UPDATE `help_topic` SET `body` = 'Usage: settrap <direction>\n\nA Thief who knows "set trap (door)" attempts to rig a trap on a\nclosed door in that direction. Success depends on your proficiency\nwith the skill (see `skills`) -- a fumbled attempt wastes the try and\nleaves the door untrapped, but climbs your proficiency toward\nsuccess next time regardless. Once successfully rigged, whoever walks\nthrough it next without knowing "detect trap" springs it -- a\none-shot hit to a random limb -- while a Thief who does know that\nskill safely steps around it, leaving it rigged for the next person.\n\nRelated: disarmtrap'
  WHERE `name` = 'settrap' AND `updated_by` = 'seed';

UPDATE `help_topic` SET `body` = 'Usage: disarmtrap <direction>\n\nA Thief who knows "disarm trap" attempts to safely remove a trap\nrigged on a door in that direction, with no risk of springing it\neither way. Success depends on your proficiency with the skill (see\n`skills`) -- a fumbled attempt leaves the trap still rigged, but\nclimbs your proficiency toward success next time regardless.\n\nRelated: settrap'
  WHERE `name` = 'disarmtrap' AND `updated_by` = 'seed';

-- `set` grew practices/basic/combat/advanced fields (user 2026-07-17:
-- "need the ability for the set command to adjust practices and any
-- other stat you can think of, we'll get in the habit of updating set
-- with new items as we go") -- full-body replacement, same guarded
-- pattern as every other post-seed `set` update.
UPDATE `help_topic` SET `body` = 'Usage: set <name> <field> <value>\n\nA one-shot sibling of edplayer for quick,\nscriptable single-field edits -- one line in, one field changed, no\nmenu. Works on any player, online or offline, by exact name; an online\ntarget is updated immediately.\n\nFields: level, xp, hp <hp> <max hp>, alignment, str/dex/con/int/wis/cha,\ngender, title (or ''none'' to clear), loadroom, handed, practices\n(spendable practice points), basic/combat/advanced (discipline\npercentages, 0-100 -- see `help practice`). This field list grows over\ntime as new player-facing stats get added; see `edplayer` for a menu\ncovering every field at once.\n\nRelated: promote'
  WHERE `name` = 'set' AND `updated_by` = 'seed';

-- `who` grew a global active/linkdead/total footer (user 2026-07-17:
-- "who should report player count (active links) and linkdeads in a
-- total player count").
UPDATE `help_topic` SET `body` = 'Usage: who [name|immortals|mortals]\n\nLists everyone currently playing, with their level (or immortal rank\ntitle) shown in brackets before their name and any personal title after\nit. With an argument, filters the list: `who imm` shows only immortals,\n`who mort` only mortals, and any other word matches part of a name.\n\nA personal title (see `help title`) can use color tags (`help colors`)\nand <N>/<n> to insert your own name anywhere in the text.\n\nEvery `who` also ends with a global summary line -- active links,\nlinkdead bodies still in the world, and the total -- regardless of any\nfilter applied above. A linkdead character (disconnected without a\nclean `quit!`/`rent`) stays in the world until the same account\nreconnects or an immortal runs `purge linkdead`.\n\nRelated: users score'
  WHERE `name` = 'who' AND `updated_by` = 'seed';

-- `alias` command (user 2026-07-17: "players define their own aliases,
-- stored on the account and shared across that account's characters").
INSERT INTO `help_topic` (`name`, `body`, `updated_by`) VALUES
('alias', 'Usage: alias\n       alias <name>\n       alias <name> <expansion>\n       alias remove <name>\n\nDefine your own command shortcuts. Bare `alias` lists everything you\nhave defined; `alias <name>` shows one; `alias <name> <expansion>`\ncreates or overwrites one; `alias remove <name>` deletes one. Typing\nthe alias name later expands to its full text, with anything else you\ntyped after it tacked on the end -- e.g. `alias k kill` then `k orc`\nsends `kill orc`.\n\nAliases live on your ACCOUNT, not the character, so every character\nyou play shares the same set -- but they are split by tier: a mortal''s\naliases only work while playing a mortal character, an immortal''s only\nwhile playing an immortal one, even on the same account. Up to 20 per\ntier.\n\nRelated: help', 'seed')
ON DUPLICATE KEY UPDATE `name` = `name`;

-- Money + shops (user 2026-07-17: "implement money and shops").
INSERT INTO `help_topic` (`name`, `body`, `updated_by`) VALUES
('list', 'Usage: list\n\nShows what the shop in your current room has for sale, each item\nnumbered and priced in gold. You must be standing where a\nshopkeeper is actually present -- a shop with no keeper around is\nclosed. A listed item never runs out; the shop can always sell it.\n\nRelated: buy sell', 'seed'),
('buy', 'Usage: buy <item>\n       buy <#>\n\nPurchases an item from the shop in your current room, deducting its\ngold price from your purse (see `score`). Either name it or give its\nnumber from `list` -- `buy 3` and `buy` followed by its name buy the\nsame item. You need a live shopkeeper present and enough gold on\nhand.\n\nRelated: list sell score', 'seed'),
('sell', 'Usage: sell <item>\n\nSells a carried item (not worn or held) to the shop in your current\nroom for gold, credited to your purse (see `score`). Each shop only\nbuys certain kinds of goods -- offering something it does not deal in\nis refused. The item is gone for good once sold.\n\nRelated: list buy score', 'seed')
ON DUPLICATE KEY UPDATE `name` = `name`;

-- `shutdown` (user 2026-07-17: "write a shutdown command to kill the mud
-- kindly along with a time function that will shutdown in <X> seconds").
INSERT INTO `help_topic` (`name`, `body`, `updated_by`) VALUES
('shutdown', 'Usage: shutdown\n       shutdown <seconds>\n       shutdown cancel\n\nEnds the game. Every connected player is warned and their character\nis saved first. Bare `shutdown` does this right away; `shutdown 60`\ncounts down that many seconds first, warning everyone at intervals\nalong the way, without freezing the game for anyone in the\nmeantime -- `shutdown cancel` aborts a countdown already running.\nImplementor only (60).\n\nRelated: copyover', 'seed')
ON DUPLICATE KEY UPDATE `name` = `name`;

-- `wipe` (user 2026-07-17: "`wipe` command + a real (non-hardcoded)
-- master password"). Administrator (59+).
INSERT INTO `help_topic` (`name`, `body`, `updated_by`) VALUES
('wipe', 'Usage: wipe <name> <password>\n       wipe account <name> <password>\n\nPermanently erases a character, or an entire account and every\ncharacter on it. Requires the wipe master password (set outside the\ngame, not something any command shows or changes) -- get it wrong\nand nothing happens. You can only wipe someone below your own level\n(for an account, every character on it must be below your level).\nAn online target is disconnected and their belongings drop to the\nfloor first. There is no undo.\n\nRelated: promote set', 'seed')
ON DUPLICATE KEY UPDATE `name` = `name`;

-- `dig` (TODO.md, done 2026-07-18). BUILD_MIN_LEVEL (51+).
INSERT INTO `help_topic` (`name`, `body`, `updated_by`) VALUES
('dig', 'Usage: dig <direction>\n\nIf there''s no exit that way yet, creates a brand new room, connects\nit back to where you''re standing (both directions), and walks you\nstraight into it -- same as a normal move once the exit exists. The\nnew room lands within your current room''s own zone, at the lowest\nvnum not already in use there; refused if that zone has none left, if\nan exit already exists that way, or if you aren''t assigned to build\nin this zone. The new room starts bare -- `edit room` to give it a\nreal name and description.\n\nRelated: edit room', 'seed'),
('edaccount', 'Usage: edit account <name>\n\nAdministrator (58+) only: a menu-driven editor for any account --\nrename it, reset its password, or see every character on it and\ntheir level. Every action here takes effect immediately, there''s no\nSave step. No self-service equivalent; a player asks an immortal.\n\nRelated: edit edplayer promote', 'seed'),
('edsocial', 'Usage: edit social [name]\n\nLevel 55+ only: menu-driven editor for socials/emotes -- the same\nverbs the `socials` command lists. Leave off the name to browse the\nfull list (type a name there to open it, or `new` to create one); an\nexact existing name jumps straight to its detail view. Each social\nhas 8 message fields (self/others for no target, target found, and\ntargeting yourself, plus its own not-found line) -- pick a number to\nrewrite one. `H` toggles whether the message is hidden from someone\nwho can''t currently see the actor (no effect yet -- there''s no\ninvisibility system in the game to trigger it), `P` sets the minimum\nposition needed to use it (e.g. must be standing), `R` renames it, `D`\ndeletes it. Every change takes effect immediately, live, for every\nplayer -- there''s no Save step and no restart needed.\n\nRelated: socials edit', 'seed')
ON DUPLICATE KEY UPDATE `name` = `name`;

-- Diseases + Hospital (TODO.md, done 2026-07-18).
INSERT INTO `help_topic` (`name`, `body`, `updated_by`) VALUES
('disease', 'Drinking from a puddle on the ground carries a 15% chance of catching\nsomething -- a Cold, the Flu, Food Poisoning, or the Plague, picked at\nrandom, each running a different length of time. While it lasts, it\nsaps a little HP every so often (worse the nastier the disease) until\nit wears off naturally, or you get it cured at a hospital. Check\n`affects` to see what you''ve got and how long is left. Immortals are\nimmune.\n\nRelated: drink hospital affects', 'seed'),
('hospital', 'Usage: list   |   buy <#>   (at a hospital)\n\nSix hospitals are staffed by real doctors -- Tobin City, Amber,\nLogrus, Brightmoon, a field medic''s post, and Xanesla. `goto\nhospital` gives directions to the nearest one from anywhere. Find the\ndoctor and `list` to see what ails you -- every damaged limb and\nactive disease, each priced -- then `buy <#>` to be cured on the spot\nfor gold. Nothing to treat, and `list` just says you look healthy.\n\nRelated: limbs disease goto', 'seed')
ON DUPLICATE KEY UPDATE `name` = `name`;

-- `goto hospital` landmark (user 2026-07-18: "add a goto hospital to lead
-- new players to the hospital") -- full-body replacement, same guarded
-- pattern as the earlier goto migrations.
UPDATE `help_topic` SET `body` = 'Usage: goto guildmaster|combat|rent|surplus|hospital|<classname>   |   goto <room vnum | player> (immortals)\n\nLandmark forms are open to everyone: `goto guildmaster` reports\nwalking directions to the nearest Basic guildmaster of your own class,\n`goto combat` to the nearest Combat guildmaster, `goto rent` to the\ninn, `goto surplus` to the surplus store, and `goto hospital` to the\nnearest hospital -- none of these teleport you, they just tell you\nthe way (shortest real path, by actual room exits). `goto advanced`\ndeliberately refuses -- finding the Advanced guildmaster is part of\nthe challenge. Standing right there already just says so.\n`goto <classname>` (e.g. `goto thief`) gives directions to that\nNAMED class''s own Basic guildmaster, not just your own class -- handy\nfor checking on another class''s trainer without switching characters.\nEvery other form of `goto` is immortals only, and DOES teleport -- a\nroom by its vnum, or another online player by name (you land in their\nroom). Useful vnums: 0 (The Void), 1 (Imperia).\n\nRelated: practice rent hospital bamfin bamfout transfer'
  WHERE `name` = 'goto' AND `updated_by` = 'seed';

-- Disease roster expanded from 4 to all 26, and poison converted from a
-- one-shot hit into its own real affect (user 2026-07-18) -- full-body
-- replacement, same guarded pattern as above.
UPDATE `help_topic` SET `body` = 'Drinking from a puddle on the ground carries two independent risks.\nThere''s a 30% chance of being poisoned -- it saps HP every so often\nuntil it wears off or is cured, same as a disease (see below). There''s\nalso a separate 15% chance of catching one of 26 diseases, from a\ncommon Cold up through Frostbite, Leprosy, Gangrene, and the Plague --\neach with its own duration and its own bite, worse ones hitting\nharder and lasting longer. Either way, check `affects` to see what\nyou''ve got and how long is left, and get it cured at a hospital\nwhenever you''d rather not wait it out. Immortals are immune to both.\nNPCs can catch these too, and will visibly wince from a flare-up same\nas a player would.\n\nRelated: drink hospital affects'
  WHERE `name` = 'disease' AND `updated_by` = 'seed';

-- Bulletin boards (user 2026-07-18: "we need to make bulletin boards
-- function, read and write commands, from sneezy").
INSERT INTO `help_topic` (`name`, `body`, `updated_by`) VALUES
('read', 'Usage: read   |   read <#>   |   read 2.board [<#>]   |   read at <board name> [<#>]\n\nBulletin boards are mounted in rooms across the world. Standing next\nto one, bare `read` lists every message posted on it; `read <#>`\nshows one in full. Some boards need real rank to use at all -- try\nanyway and it''ll say so if you don''t qualify.\n\nIf a room happens to have more than one board, say which one you\nmean, either way: `read 2.board` (the second board in the room, same\nordinal counting `look`/`kill`/`get` use) or `read at wiz` (matches by\nany part of the board''s own name), then optionally a message number,\ne.g. `read at wiz 3`. With only one board in the room neither is ever\nneeded.\n\nRelated: write affects', 'seed'),
('write', 'Usage: write <subject> <message>   |   write 2.board <subject> <message>   |   write at <board name> <subject> <message>\n\nPosts a new message on the bulletin board in your current room --\nthe first word is the subject, everything after is the body, all on\none line. Signed with your own name automatically. "board" itself\nisn''t allowed as a subject.\n\nIf the room has more than one board, say which one first, either\nway: `write 2.board <subject> <message>` or `write at wiz <subject>\n<message>`. With only one board present neither is ever needed, even\nif your subject happens to start with a number or the word "at".\n\nRelated: read', 'seed')
ON DUPLICATE KEY UPDATE `name` = `name`;

-- `look`/`examine` gained ordinal support (user 2026-07-18: "make look
-- board, look 2.board... make it true as part of everything that can
-- exist") -- full-body replacement, same guarded pattern as above.
UPDATE `help_topic` SET `body` = 'Usage: look [player, mobile, or item]\n\nShows the room you are in: its name, description, obvious exits, and\neveryone (and everything) standing there with you. You also look\nautomatically whenever you enter the world. `look <name>` describes\nanother player or a mobile in the room (their appearance/description),\nor an item -- on the room floor or in your own inventory/equipment --\nshowing its description and, if it has one, its condition.\n\nIf more than one thing matches what you typed, `look 2.<name>` looks\nat the second one instead of always the first -- the same "N." ordinal\nprefix `kill`/`get` already support, e.g. `look 2.board` when a room\nhas two boards. Plain `look <name>` (no number) always means the\nfirst match, same as before.\n\nImmortals additionally see the room''s vnum, sector type, and flags in\nthe header line.'
  WHERE `name` = 'look' AND `updated_by` = 'seed';

-- `light`/`extinguish`/`refuel` (user 2026-07-18: "light refuel and the
-- lamp lighting boy code need to be implemented, from sneezy").
INSERT INTO `help_topic` (`name`, `body`, `updated_by`) VALUES
('light', 'Usage: light <item> [held|room]\n\nLights a light source -- a torch, lantern, or lamppost -- so long as\nit isn''t already lit and still has fuel left. Checks what you''re\ncarrying/wearing first, then the room floor; add "held" or "room" to\nsay which one you mean if it could be either. A lit light shows\n"(lit)" wherever it''s sitting or standing, and slowly burns through\nits own fuel over time -- see `refuel`.\n\nRelated: extinguish refuel', 'seed'),
('extinguish', 'Usage: extinguish <item> [held|room]\n\nPuts out a lit light source. Same carried-then-room search order as\n`light`, with the same optional "held"/"room" to disambiguate.\n\nRelated: light refuel', 'seed'),
('refuel', 'Usage: refuel <light> <fuel> [held|room]\n\nTops up a light source from a fuel item you''re carrying (bricks of\nsolid/lantern fuel are sold at Lumor''s Illuminations). Refuses if the\nlight is already full, already lit (you don''t refuel something while\nit''s burning -- it might explode), or can''t be refueled at all (a\nplain torch burns down and is gone for good, no refilling it). The\nfuel item is used up and destroyed once its own supply runs out.\n\nRelated: light extinguish', 'seed')
ON DUPLICATE KEY UPDATE `name` = `name`;

-- `prompt gold` (user 2026-07-18: "expand prompt command toggles to
-- include mana, piety, vitality, gold, etc" -- gold unblocked once the
-- Money system shipped) -- full-body replacement, same guarded pattern.
UPDATE `help_topic` SET `body` = 'Usage: prompt [hp|gold]\n\n`prompt hp` toggles your hit points into the prompt line; `prompt\ngold` toggles your gold. Either, both, or neither -- whatever''s on\nrenders together ("HP: 25 Gold: 40 >"). Bare `prompt` shows the\ncurrent setting. Your choice is saved with your character. More stats\nwill join the prompt as they exist (mana/piety/vitality aren''t real\nstats yet).\n\nRelated: toggle score'
  WHERE `name` = 'prompt' AND `updated_by` = 'seed';

-- `toggle pk` (user 2026-07-18: "PK opt-in flag; BOTH players must have
-- opted in for attack/kill between players") -- full-body replacement.
UPDATE `help_topic` SET `body` = 'Usage: toggle [name]\n\nWith no argument, lists your on/off switches and their current values.\n`toggle <name>` flips one (abbreviations welcome). Player toggles like\ncolor and hp affect only you; game toggles like multiplay are global\nand only 55+ immortals may change them.\n\n`toggle pk` is worth calling out specifically: it opts YOU in to\nfighting other players, but `attack`/`kill`/`hit` only ever reaches\nanother player if BOTH of you have it on -- otherwise they''re simply\nnot a valid target, same as if they weren''t in the room. Off by\ndefault. Mob combat is completely unaffected either way.\n\nRelated: gametog prompt'
  WHERE `name` = 'toggle' AND `updated_by` = 'seed';

-- Tips system (user 2026-07-18: "tips command + periodic tip echoes,
-- per-player newbie toggle, tipedit").
INSERT INTO `help_topic` (`name`, `body`, `updated_by`) VALUES
('tips', 'Usage: tips\n\nShows one random gameplay tip. If you''re still on the newbie\nchannel (`toggle newbie`), you''ll also get one echoed to you\nautomatically every so often without asking.\n\nRelated: toggle', 'seed'),
('tipedit', 'Usage: tipedit <text>   |   tipedit list   |   tipedit delete <id>\n\nImmortal only (53+): manages the pool `tips` draws from. Bare\n`tipedit <text>` adds a new one-line tip; `tipedit list` shows every\ntip with its number; `tipedit delete <id>` removes one.\n\nRelated: tips', 'seed')
ON DUPLICATE KEY UPDATE `name` = `name`;

-- Spell/skill affects expansion (user 2026-07-18: "implement spell/skill
-- affects and write help files for each including what symbol/
-- component/commodity is needed to cast/pray") -- `cast cure poison`/
-- `cure disease` now really cure, a family of armor/shield/resistance-
-- flavored spells now really apply Sanctuary, and Clerics' own "poison"
-- and "disease"/"infect" prayers now really inflict them on whoever
-- you're fighting -- all reusing this session's disease/poison/affect
-- work rather than ~30 bespoke mechanics. `skills`/`practice
-- <discipline>` now show inline, per class, exactly what `cast`/`pray`
-- consume (a component vs. a holy symbol) instead of a help_topic row
-- per spell -- that''s the "what''s needed" half of the request.
UPDATE `help_topic` SET `body` = 'Usage: cast <spell>\n\nMages and Druids only: casts a spell from your class''s roster (see\n`skills`, which now shows right at the top of your spell list whether\nit needs a component). You need a spell component -- any carried item\nkeyworded "component" -- on hand; it is consumed on every attempt,\nsuccess or fail. Also needs enough practiced discipline in that\nspell''s tier -- see `help practice`.\n\nBeyond that access gate, each spell has its OWN proficiency\npercentage (shown in `skills`), separate from your discipline\npercentage -- a freshly-accessible spell starts barely competent and\nclimbs toward a ceiling set by your discipline percentage every time\nyou attempt it, win or lose. A low-proficiency spell often fizzles\n("You fumble the casting..."); a well-practiced one rarely does.\n\nWHAT ACTUALLY HAPPENS: "cure poison"/"cure disease" genuinely cure\nthose (see `help affects`); healing- and armor/shield/resistance-\nflavored spells (stone skin, barkskin, self-wards, ...) apply a real\nprotective ward; damage-flavored spells hit whoever you''re fighting.\nAnything else in the roster is still a placeholder for now. Clerics\nuse `pray` instead.\n\nRelated: pray practice skills affects'
  WHERE `name` = 'cast' AND `updated_by` = 'seed';

UPDATE `help_topic` SET `body` = 'Usage: pray <spell> [target]\n\nClerics only: the Cleric equivalent of `cast` -- draws on your class''s\nroster of prayers instead of spells (see `skills`, which now shows\nright at the top of your prayer list that it needs a holy symbol). You\nneed a holy symbol -- any carried item keyworded "symbol" -- on hand,\nconsumed on every attempt, success or fail. A healing prayer can\ntarget someone else in the room (`pray heal light <name>`), or, left\nblank, yourself; `pray cure poison <name>`/`pray cure disease <name>`\nwork the same way. See `help continue` to repeat a heal automatically\nuntil it is no longer needed.\n\nBeyond the class/level/discipline access gate, each prayer has its\nOWN proficiency percentage (shown in `skills`), separate from your\ndiscipline percentage -- it climbs toward a ceiling set by your\ndiscipline percentage every time you pray it, win or lose. A\nlow-proficiency prayer often fizzles ("You fumble the prayer...");\na well-practiced one rarely does.\n\nWHAT ACTUALLY HAPPENS: "cure poison"/"cure disease" genuinely cure\nthose (see `help affects`); armor/shield-flavored prayers (armor,\nbless, plasma mirror, ...) apply a real protective ward; while\nfighting, "poison" and "disease"/"infect" genuinely inflict those on\nyour opponent, and damage-flavored prayers hit them too. Anything else\nin the roster is still a placeholder for now.\n\nRelated: cast continue practice skills affects'
  WHERE `name` = 'pray' AND `updated_by` = 'seed';

UPDATE `help_topic` SET `body` = 'Usage: affects\n\nLists any temporary buffs or debuffs currently on you, each with its\nown countdown, and says plainly if there aren''t any. Covers a\nprotective ward (Sanctuary -- from `pray sanctuary`/`armor`, `cast\nstone skin`/`barkskin`, and similar spells, halves incoming damage\nwhile it lasts), poison (from a bad drink, or a Cleric''s `pray\npoison` inflicted on you mid-fight), and any of two dozen diseases\n(same sources, or `pray disease`/`infect`) -- each wears off on its\nown with a message once its time is up, or can be cured early: a\nhospital (see `help hospital`), or the right cure spell/prayer.\n\nRelated: cast pray hospital'
  WHERE `name` = 'affects' AND `updated_by` = 'seed';

UPDATE `help_topic` SET `body` = 'Usage: skills\n\nShows your class''s full roster of skills and spells, organized into\nthree tiers -- Combat, <Class> Skills, and Advanced <Class> Skills --\nwith your current discipline percentages (Basic, Combat, Advanced) at\nthe top. A Mage/Druid/Cleric section opens with a line naming exactly\nwhat `cast`/`pray` needs on hand for every spell/prayer in it (a\ncomponent or a holy symbol) -- see `help cast`/`help pray`, or `help\n<spell name>` for that one spell specifically. Each skill is marked\nwith whether you have ACCESS to it yet, and if not, exactly why (level\ntoo low, or discipline not yet practiced -- see `help practice`).\nAdvanced skills are locked until Basic AND Combat both reach 100%.\n\nEvery accessible skill also shows its own individual PROFICIENCY\npercentage in brackets (e.g. "[34%]") -- a separate number from your\ndiscipline percentage, learned by actually using the skill\n(`cast`/`pray`/`settrap`/`disarmtrap`/dual wield). It climbs toward a\nceiling set by your discipline percentage for that skill''s tier, and\ngates how often the skill actually succeeds.\n\nImmortals see every class''s full roster, not just their own, and\nbypass both the level and discipline gates entirely.\n\nRelated: practice cast pray'
  WHERE `name` = 'skills' AND `updated_by` = 'seed';

-- Rewrite of the pre-existing 'disease' topic (stale since it only
-- mentioned the original 4 diseases): current 26-disease roster, plus
-- the Cleric's own offensive "disease"/"infect" prayer that didn''t exist
-- when this topic was first written. Kept as its OWN topic rather than
-- folded into skill_help.sql''s generated set -- see that file''s header
-- comment for why "disease" the general mechanic and "disease" the one
-- Cleric spell share this single name/topic instead of colliding.
UPDATE `help_topic` SET `body` = 'Drinking from a puddle on the ground carries a chance of catching one\nof 26 diseases (Cold, Flu, Plague, Leprosy, Dysentery, and more --\npicked at random), each running a different length of time. While it\nlasts, it saps a little HP every so often (worse the nastier the\ndisease) until it wears off naturally, or you get it cured at a\nhospital or the right cure spell/prayer. Check `affects` to see what\nyou''ve got and how long is left. Drinking never diseases an immortal.\n\nA Cleric fighting someone can also inflict a random disease directly\nwith `pray disease`/`pray infect` -- this CAN land on an immortal\nopponent, unlike the puddle roll above.\n\nRequires: (for the Cleric prayer) a holy symbol (`pray`)\nRelated: drink pray hospital affects'
  WHERE `name` = 'disease' AND `updated_by` = 'seed';

-- "wait"/"say" verbs added to the trigger action vocabulary (user: wanted
-- a market-vendor mob crying out food items one line at a time, which
-- needed both a proper `say` action and a real pause primitive).
UPDATE `help_topic` SET `body` = 'Usage: edit trigger <room|mob|obj> <vnum> <trigger_type> [match_text|chance]\n       edit trigger list <room|mob|obj> <vnum>\n       edit trigger delete <id>\n\nBuilder (51+) only: attaches scripted behavior to a room, mob, or\nobject prototype -- no recompile needed, unlike the classic spec\nproc approach. Trigger types:\n\n  room: enter (someone walks in), random (ambient, rolled every tick)\n  mob:  greet (someone walks into its room), speech (someone says a\n        matching keyword nearby -- give the keyword as the last\n        argument), death (it dies), random (ambient tick)\n  obj:  get (picked up), wear (worn)\n\nFor a `random` trigger, the last argument is the percent chance per\ntick (default 25). After the header line, you land in the line editor\nto write the script -- one action per line, `/s` saves:\n\n  echo <text>      -- to the triggering player only\n  echoroom <text>  -- to everyone else in the room\n  emote <text>     -- "<Name> <text>" to the whole room\n  say <text>       -- "<Name> says, ''<text>''" to the whole room\n  teleport <vnum>  -- moves the triggering player to that room\n  give <vnum>      -- spawns that object into their inventory\n  damage <n>       -- deals n damage (never fatal on its own)\n  log <text>       -- a silent log entry, never broadcast\n  wait <seconds>   -- pauses everything AFTER this line for that many\n                       real seconds, then resumes it -- e.g. a market\n                       vendor crying out one item at a time. Whoever\n                       triggered the script is NOT remembered across a\n                       wait (they may be long gone by the time it\n                       resumes), so only say/emote/echoroom/log make\n                       sense afterward -- echo/teleport/give/damage\n                       silently do nothing there.\n\n`edit trigger list <type> <vnum>` shows what''s already attached;\n`edit trigger delete <id>` removes one.'
  WHERE `name` = 'trigger' AND `updated_by` = 'seed';

-- aitick now also forces along any `wait`-paused trigger script.
UPDATE `help_topic` SET `body` = 'Usage: aitick [count]\n\nImmortal debug tool: forces `count` (default 1, max 100) mob-AI /\npuddle-decay / random-trigger ticks to run right now, synchronously,\nplus resolves any `wait`-paused trigger script immediately instead of\nwaiting on its real countdown. These all normally only fire on the\nreal world pulse (~60s for most; ~1s for a `wait`) -- far too slow to\nwait on live, and far too slow for an automated test -- so this\ncollapses real time into an instant for testing wander/scavenge/\ndecay/random-trigger/wait behavior.\n\nRelated: hurtlimb'
  WHERE `name` = 'aitick' AND `updated_by` = 'seed';

-- `lock`/`unlock` (TODO.md "Keys unlocking doors" -- the object system it
-- was blocked on now exists). A key is matched by its own object vnum
-- against a door's key_num / a container's val[2], not any val[] field on
-- the key itself -- see cmd_lock.c.
INSERT INTO `help_topic` (`name`, `body`, `updated_by`) VALUES
('lock', 'Usage: lock <direction>   |   lock <container>\n\nLocks a closed door or container, if you''re carrying the right key.\nA door or container without a keyhole at all can''t be locked this\nway (nothing to turn). Must already be closed -- `close` it first.\nA locked door/container blocks `open` until `unlock`ed again.\n\nRelated: unlock open close', 'seed'),
('unlock', 'Usage: unlock <direction>   |   unlock <container>\n\nUnlocks a locked door or container, if you''re carrying the matching\nkey -- you don''t need to name the key, just have it on you (carried,\nworn, or held). Wrong key, or no key at all, and it refuses. Once\nunlocked, `open` it as normal.\n\nRelated: lock open close', 'seed')
ON DUPLICATE KEY UPDATE `name` = `name`;

-- `ignore`/`unignore` (Sneezy → Tobin feature audit, "Ignore lists").
INSERT INTO `help_topic` (`name`, `body`, `updated_by`) VALUES
('ignore', 'Usage: ignore [<name>]\n\nBlocks `tell`s and `whisper`s from someone -- they still see their\nmessage as sent, but it never reaches you. Bare `ignore` lists\neveryone you''re currently ignoring (up to 30 at once). Doesn''t affect\nany other channel, and you can''t ignore yourself.\n\nRelated: unignore tell whisper', 'seed'),
('unignore', 'Usage: unignore <name>\n\nStops blocking someone''s `tell`s and `whisper`s (see `ignore`).\n\nRelated: ignore tell whisper', 'seed')
ON DUPLICATE KEY UPDATE `name` = `name`;

-- `ignore` cross-reference on the existing tell/whisper topics.
UPDATE `help_topic` SET `body` = 'Usage: tell <name> <message>\n\nA private message to anyone playing, anywhere in the game -- no need\nto share a room. Compare `whisper`, which only reaches someone in your\nown room but at least lets bystanders know a conversation is\nhappening. Blocked entirely by `ignore`.\n\nRelated: whisper say ignore'
  WHERE `name` = 'tell' AND `updated_by` = 'seed';
UPDATE `help_topic` SET `body` = 'Usage: whisper <name> <message>\n\nA private message to someone in your own room -- everyone else there\nsees that a conversation happened, but not what was said. Compare\n`tell`, which reaches anyone anywhere but gives bystanders no hint at\nall. Blocked entirely by `ignore`.\n\nRelated: tell say ignore'
  WHERE `name` = 'whisper' AND `updated_by` = 'seed';

-- `possess`/`return` (Sneezy → Tobin feature audit, "Switch / return
-- (puppet a mob)"). Named `possess` since `switch` already means
-- something else in Tobin (swap held items, cmd_object.c).
INSERT INTO `help_topic` (`name`, `body`, `updated_by`) VALUES
('possess', 'Usage: possess <mob>\n\nLevel 59+ only: puppet a mob''s body -- your commands drive the mob\ninstead of your own character until you `return`. The mob must be in\nyour room, unpossessed, and not a player. Your own immortal command\naccess stays with you the whole time (so `return` always works, even\nif the mob''s in-game level is low); a disconnect while possessing\nautomatically returns you first, so you never get stranded in the\nmob''s body.\n\nRelated: return', 'seed'),
('return', 'Usage: return\n\nComes back to your own body after `possess`ing a mob, or early from a\n`polymorph`.\n\nRelated: possess polymorph', 'seed')
ON DUPLICATE KEY UPDATE `name` = `name`;

-- `polymorph`/`disguise` (Sneezy → Tobin feature audit, "Transformation").
-- Polymorph reuses possess/return's descriptor-swap; disguise is a
-- lighter, purely cosmetic short_descr toggle.
INSERT INTO `help_topic` (`name`, `body`, `updated_by`) VALUES
('polymorph', 'Usage: cast polymorph\n\nMage spell. Twists your body into a brown bear for a while, taking on\nits full strength -- reverts on its own after a time, or early with\n`return`. Your own body stays behind in the room, linkdead, until you\nrevert.\n\nRelated: return', 'seed'),
('disguise', 'Usage: disguise\n\nThief skill. Pulls up your hood and becomes "a hooded stranger" to\neveryone else in the room, hiding your real name. Use `disguise`\nagain to drop it. Purely cosmetic -- doesn''t change your stats or\nequipment.\n\nRelated: none', 'seed')
ON DUPLICATE KEY UPDATE `name` = `name`;

-- `wiznet @<level>` targeting (Sneezy → Tobin feature audit, "OOC
-- channels" -- ported from the real `commune @<level>` behavior).
UPDATE `help_topic` SET `body` = 'Usage: wiznet <message>   |   wiznet @<level> <message>   (shorthand: ;<message>)\n\nImmortals only: a private broadcast channel among the immortals. Your\nmessage reaches every online immortal (and yourself), out of sight of\nmortals. The `;` shorthand needs no space: `;hi` broadcasts "hi".\nAdd `@<level>` to narrow delivery to only immortals at or above that\nlevel -- `wiznet @59 <msg>` reaches Administrator+ only.'
  WHERE `name` = 'wiznet' AND `updated_by` = 'seed';

-- `cast`/`pray` grew real offensive-spell breadth (2026-07-20): `cast`
-- now takes an optional target like `pray` always could, and an
-- offensive spell can open combat on its own instead of only ever
-- hitting whoever you already happened to be fighting -- the INSERTs
-- above are a no-op on the already-seeded rows, so update them explicitly.
UPDATE `help_topic` SET `body` = 'Usage: cast <spell> [target]\n\nMages and Druids only: casts a spell from your class''s roster (see\n`skills`). You need a spell component -- any carried item keyworded\n"component" -- on hand; it is consumed on a successful cast. Also\nneeds enough practiced discipline in that spell''s tier -- see `help\npractice`. An offensive spell can target someone else in the room\n(`cast gust <name>`) and will draw you into a fight with them if you\naren''t already fighting anyone; left blank, it keeps hitting whoever\nyou''re already fighting. Clerics use `pray` instead.\n\nRelated: pray practice skills affects'
  WHERE `name` = 'cast' AND `updated_by` = 'seed';
UPDATE `help_topic` SET `body` = 'Usage: pray <spell> [target]\n\nClerics only: the Cleric equivalent of `cast` -- draws on your class''s\nroster of prayers instead of spells. You need a holy symbol -- any\ncarried item keyworded "symbol" -- on hand, consumed on every\nsuccessful prayer. A healing prayer can target someone else in the\nroom (`pray heal light <name>`), or, left blank, yourself. An\noffensive prayer works the same way (`pray harm light <name>`) and\nwill draw you into a fight with them if you aren''t already fighting\nanyone; left blank, it keeps hitting whoever you''re already fighting.\nSee `help continue` to repeat a heal automatically until it is no\nlonger needed.\n\nRelated: cast continue practice skills affects'
  WHERE `name` = 'pray' AND `updated_by` = 'seed';

-- Magic items (Sneezy -> Tobin feature audit, full system): new `use`
-- command, plus wear/remove now mention that some worn gear carries
-- real stat/AC/HP/Vitality bonuses.
INSERT INTO `help_topic` (`name`, `body`, `updated_by`) VALUES
('use', 'Usage: use <scroll|wand|staff> [target]\n\nInvokes a magic item''s stored spell. A scroll is single-use --\nit crumbles to dust the moment you use it. A wand and a staff are\nrechargeable but have a limited number of charges each; once spent,\nthey just sit inert (no way to recharge one yet). A wand targets one\nperson -- someone else in the room (`use wand <name>`), or left blank,\nwhoever you''re already fighting for an offensive one, yourself for a\nhealing/protective one. A staff always hits every OTHER being in the\nroom at once, no targeting needed.\n\nAny character can use one of these regardless of class or level --\nthe stored magic doesn''t care who''s holding it.\n\nRelated: wear identify', 'seed')
ON DUPLICATE KEY UPDATE `name` = `name`;

UPDATE `help_topic` SET `body` = 'Usage: wear <item>\n\nPuts on a carried item into its body slot (head, body, legs, and so\non). Refuses if you''re already wearing something there, or if the\nitem isn''t wearable there at all -- a holdable item (weapon or\notherwise) isn''t worn this way; see `hold`/`wield` instead. Some gear\ncarries a real bonus (a stat, Armor Class, max HP, or max Vitality) --\nsee `identify` to check before you put something on -- which applies\nthe moment you wear it and goes away the moment you take it off.\n\nRelated: remove identify'
  WHERE `name` = 'wear' AND `updated_by` = 'seed';
UPDATE `help_topic` SET `body` = 'Usage: remove <item>\n\nTakes off a worn item or lays down a held one, returning it to your\ncarried inventory. Any stat/Armor Class/HP/Vitality bonus that item was\ngiving you (see `identify`) goes away the moment you take it off.\n\nRelated: wear identify'
  WHERE `name` = 'remove' AND `updated_by` = 'seed';

-- Sign language (Sneezy -> Tobin feature audit): new `sign` command.
INSERT INTO `help_topic` (`name`, `body`, `updated_by`) VALUES
('sign', 'Usage: sign <message>\n\nCommunicates silently to everyone in your room using hand signals --\nlike `say`, but no sound at all. Needs both hands free and neither arm\nbadly hurt; you can''t sign while fighting or asleep, and everyone\nlearns it (see `skills`). Only someone else who also knows sign\nlanguage actually reads your message -- everyone else just sees you\n"make funny motions with your hands," except a Thief, whose signs are\ncommon enough that anyone recognizes them.\n\nRelated: say skills', 'seed')
ON DUPLICATE KEY UPDATE `name` = `name`;

-- Drug tracking (Sneezy -> Tobin feature audit): new `smoke` command.
INSERT INTO `help_topic` (`name`, `body`, `updated_by`) VALUES
('smoke', 'Usage: smoke <item>\n\nConsumes a dose from a carried drug item (pipeweed, opium, pot, or\nfrogslime) for a real, temporary effect on your stats -- some good,\nmost not. A second dose before the first wears off just refreshes it,\nrather than stacking. Smoke the same drug too often without a break\nand you will start to feel real withdrawal pangs when you go too long\nwithout another dose. Each item holds a limited number of doses before\nit is spent.\n\nRelated: score affects', 'seed')
ON DUPLICATE KEY UPDATE `name` = `name`;

-- `edit object` (oedit, TODO.md's "NEXT UP" item -- builder-tools-OLC gap).
-- 2026-07-25 follow-up: a missing vnum now auto-creates a blank object
-- instead of refusing, `edit obj` abbreviates, and Four values (10) shows
-- an inline type-aware hint.
INSERT INTO `help_topic` (`name`, `body`, `updated_by`) VALUES
('edit object', 'Usage: edit object <vnum>   (also: edit obj <vnum>; level 51+ builders)\n\nMenu-driven editor for an object prototype (the `obj` table). Edits\nare held in a working copy -- nothing touches the DB until you Save.\nA vnum that doesn''t exist yet auto-creates a blank object and opens\nstraight into the editor.\n\n   1) Name                  2) Short description\n   3) Item type              4) Long description\n   5) Weight                 6) Volume\n   7) Extra flags            8) Take flags\n   9) Cost/value            10) Four values (meaning shown inline,\n  11) Decay time                depends on item type)\n  12) Max struct points     13) Struct points\n  14) Material              15) Can be seen\n  16) Special proc          17) Max exist\n  18) Anti-race flags\n\nItem type (3) lists every known type by number and name to pick from,\nrather than guessing a raw number blind. Extra/Take/Anti-race flags\n(7/8/18) open a toggle-by-number submenu; blank returns to the main\nmenu. Four values (10) takes all four numbers at once, e.g. \"0 0 0\n0\".\n\n  S) Save    Q) Quit (warns on unsaved changes)\n\nRelated: room zone mob stat load', 'seed')
ON DUPLICATE KEY UPDATE `name` = `name`;

-- `edit mob` (medit, closes the last builder-tools-OLC gap). Menu built
-- from a user-supplied wireframe (2026-07-25) -- 23 fixed fields,
-- Characteristics auto-computed on Save rather than editable.
INSERT INTO `help_topic` (`name`, `body`, `updated_by`) VALUES
('edit mob', 'Usage: edit mob <vnum>   (level 51+ builders)\n\nMenu-driven editor for a mob prototype (the `mob` table). Edits are\nheld in a working copy -- nothing touches the DB until you Save. A\nvnum that doesn''t exist yet auto-creates a blank mob and opens\nstraight into the editor.\n\n   1) Name                  2) Short desc\n   3) Long desc              4) Description\n   5) Action flags           6) Affect flags\n   7) Attacks                8) Level\n   9) Hitroll               10) Armor Level\n  11) HP Level              12) Damage\n  13) Gold                  14) Race\n  15) Sex                   16) Max exist\n  17) Default position      18) Class (bitmask)\n  19) Height/Weight         20) Vision\n  21) Can be seen           22) Skin\n  23) Alignment\n\nCharacteristics (strength/constitution/wisdom/intelligence/dexterity/\ncharisma) are NOT edited here -- Save auto-computes them from this\nmob''s level and class, the same formula a live-spawned mob gets.\n\n  S) Save    Q) Quit (warns on unsaved changes)\n\nRelated: room zone object stat load', 'seed')
ON DUPLICATE KEY UPDATE `name` = `name`;

-- DG Scripts-style trigger language revamp (user, 2026-07-25: "use the
-- DG_* source files to revamp triggers"). Full rewrite of the `trigger`
-- help body -- see docs/TRIGGER_SCRIPTING.md for the complete reference
-- (variable tables, worked examples) that doesn't fit in one help page.
UPDATE `help_topic` SET `body` = 'Usage: edit trigger <room|mob|obj> <vnum> <trigger_type> [match_text|chance]\n       edit trigger list <room|mob|obj> <vnum>\n       edit trigger delete <id>\n\nBuilder (51+) only: attaches a script to a room, mob, or object\nprototype -- no recompile needed. Full reference: docs/TRIGGER_SCRIPTING.md.\nTrigger types:\n\n  room: enter (someone walks in), random (ambient, rolled every tick)\n  mob:  greet (someone walks into its room), speech (someone says a\n        matching keyword nearby -- give the keyword as the last\n        argument), death (it dies), random (ambient tick)\n  obj:  get (picked up), wear (worn)\n\nFor a `random` trigger, the last argument is the percent chance per\ntick (default 25). After the header line you land in the line editor\nto write the script, one command per line, `/s` saves.\n\nVARIABLES: any %name% in a line''s text is substituted first --\n%self%/%actor%/%arg%/%time%/%random.N%, or a variable you set:\n\n  set <name> <value>    unset <name>    eval <name> <expr>   (local,\n                          this run only -- eval does simple + - * / %)\n  global <name> <value>  (persisted to the DB -- a DIFFERENT trigger''s\n                          script can read it later via plain %name%)\n\nCONTROL FLOW:\n\n  if <expr> / elseif <expr> / else / end\n  while <expr> / done            (break exits early)\n  switch <val> / case <val> / default / done\n                                   (REAL fallthrough -- a case with no\n                                    break runs into the next one too)\n\nExpr operators: == != < > <= >= && || ! -- numeric if both sides are\nfull numbers, else string comparison; || is lowest precedence,\nleft-to-right, no parentheses.\n\nACTIONS (all %var%-substituted, usable inside if/while/switch bodies):\n\n  echo <text>      -- to the triggering player only\n  echoroom <text>  -- to everyone else in the room\n  emote <text>     -- \"<Name> <text>\" to the whole room\n  say <text>       -- \"<Name> says, ''<text>''\" to the whole room\n  teleport <vnum>  -- moves the triggering player to that room\n  give <vnum>      -- spawns that object into their inventory\n  damage <n>       -- deals n damage (never fatal on its own)\n  log <text>       -- a silent log entry, never broadcast\n  wait <seconds>   -- pauses everything AFTER this line for that many\n                       real seconds, then resumes. Your set/eval/global\n                       variables (and your place inside a while loop)\n                       survive the pause; the triggering player does\n                       NOT (may be long gone by resume time) -- only\n                       say/emote/echoroom/log make sense afterward.\n\n`edit trigger list <type> <vnum>` shows what''s already attached;\n`edit trigger delete <id>` removes one.'
  WHERE `name` = 'trigger' AND `updated_by` = 'seed';

-- Menu-driven `edit trigger` redesign (2026-07-25, user: "should go into
-- a menu driven editor where you choose type with an option to delete
-- the trigger inside the menu"). Only the usage line + management footer
-- change here -- the language reference above (variables/control flow/
-- actions) is unaffected by this redesign and stays accurate.
UPDATE `help_topic` SET `body` = REPLACE(REPLACE(`body`,
    'Usage: edit trigger <room|mob|obj> <vnum> <trigger_type> [match_text|chance]\n       edit trigger list <room|mob|obj> <vnum>\n       edit trigger delete <id>\n\nBuilder (51+) only: attaches a script to a room, mob, or object\nprototype -- no recompile needed. Full reference: docs/TRIGGER_SCRIPTING.md.\nTrigger types:',
    'Usage: edit trigger <room|mob|obj> <vnum>   (opens a menu)\n       edit trigger list <vnum>                (all three target types)\n       edit trigger delete <id>\n\nBuilder (51+) only: opens a menu-driven manager for every trigger on a\nroom, mob, or object prototype -- no recompile needed. Full reference:\ndocs/TRIGGER_SCRIPTING.md. From the menu: a number opens that trigger''s\ndetail view (edit its match text/chance/script, or delete it); A adds a\nnew one, prompting for which type below, then its script. Trigger types:'),
    '`edit trigger list <type> <vnum>` shows what''s already attached;\n`edit trigger delete <id>` removes one.',
    '`edit trigger list <vnum>` shows everything attached to that vnum\nacross all three target types; `edit trigger delete <id>` removes one\nwithout opening the menu at all.')
  WHERE `name` = 'trigger' AND `updated_by` = 'seed';

-- Pet/charm (Sneezy -> Tobin feature audit). New `dismiss` command plus a
-- general `pet` topic covering the whole mechanic (summon spells, follow,
-- combat assist, obeying spoken commands, confusion chance).
INSERT INTO `help_topic` (`name`, `body`, `updated_by`) VALUES
('dismiss', 'Usage: dismiss\n\nReleases your charmed pet early, before its bond fades on its own.\nOnly does anything if you currently have a charmed pet -- see `pet`\nfor how to get one.\n\nRelated: pet', 'seed')
ON DUPLICATE KEY UPDATE `name` = `name`;

INSERT INTO `help_topic` (`name`, `body`, `updated_by`) VALUES
('pet', 'Usage: cast conjure elemental air/earth/fire/water   (Mage)\n       pray summon swarm                              (Cleric)\n       cast animal companion                           (Druid)\n\nSummons a charmed creature that follows you from room to room and\njoins any fight you''re in. You can only have one charmed pet at a\ntime -- summoning another while you already have one just fails.\n\nA pet obeys you if you speak in its presence:\n\n  say attack <target>   -- turns to attack that target\n  say kill <target>     -- same as attack\n  say stop               -- stops fighting and stands down\n  say stay                -- same as stop\n  say guard               -- same as stop\n\nAnything else you say is tried as a social -- \"say dance\" makes your\npet dance, and so on for any other emote it knows.\n\nA charmed pet doesn''t always listen -- every so often it just looks\nconfused and ignores what you said entirely.\n\n`dismiss` releases your pet early; otherwise its bond fades on its\nown after a while regardless.\n\nRelated: dismiss cast pray follow', 'seed')
ON DUPLICATE KEY UPDATE `name` = `name`;

-- Planting (Sneezy -> Tobin feature audit). Two unrelated mechanics share
-- the one `plant` command -- see cmd_plant.c's own doc comment.
INSERT INTO `help_topic` (`name`, `body`, `updated_by`) VALUES
('plant', 'Usage: plant <seeds>              -- sow a crop (anyone, outdoors)\n       plant <item> <victim>      -- secretly plant an item (Thief)\n\nSeed farming: find a sack of seeds and `plant` it outdoors, away from\nwater and out of any building. You''ll spend a few moments digging a\nhole, sowing the seeds, and covering it back up -- stay put and out of\na fight while you work, or the task is abandoned (your seeds are safe\nuntil the sowing step actually uses them). Watch it grow from a bare\nmound of dirt through a sprout to a mature plant, occasionally\nyielding fruit you can pick up once it''s grown. A room can only hold\nso many plants at once.\n\nThief plant: `plant <item> <victim>` secretly slips a carried item\ninto someone else''s inventory -- the reverse of pickpocketing. Needs\nboth hands free, and (against another player) needs `toggle pk` on\nboth sides, same as any other player-vs-player mischief.\n\nRelated: none', 'seed')
ON DUPLICATE KEY UPDATE `name` = `name`;

-- Crafting & extraction (Sneezy -> Tobin feature audit). Druid-only.
INSERT INTO `help_topic` (`name`, `body`, `updated_by`) VALUES
('skin', 'Usage: skin <corpse>\n\nDruid skill. Strips a hide from a slain animal''s corpse -- once per\ncorpse, and only works on an animal, not a person.\n\nRelated: butcher', 'seed'),
('butcher', 'Usage: butcher <corpse>\n\nDruid skill. Carves a raw steak from a slain animal''s corpse -- once\nper corpse, and only works on an animal, not a person.\n\nRelated: skin', 'seed'),
('forage', 'Usage: forage\n\nDruid skill. Gathers a bit of wild food from the terrain around you --\nneeds to be outdoors, away from water and out of any building, and\nnot too soon after your last attempt.\n\nRelated: none', 'seed')
ON DUPLICATE KEY UPDATE `name` = `name`;

-- Liquids (Sneezy -> Tobin feature audit, user 2026-07-26: "drinkable
-- liquids; pouring one out pools on the ground" + "fill a container from
-- a liquid pool"). `drink`/`sip` already existed for room puddles/
-- fountains -- their bodies are updated below to also mention a carried
-- container, now a valid target too.
UPDATE `help_topic` SET `body` = 'Usage: drink <puddle|fountain|container>\n\nDrinks from a puddle on the ground, a fountain, or a carried drink\ncontainer (waterskin, ale mug, ...). A puddle carries a chance of\ngetting poisoned or sick (a scare, not lethal on its own) -- a\nfountain or container never does. A container runs dry after enough\ndrinks; `fill` it again from a fountain or puddle, or `pour` it out.\n\nRelated: sip fill pour pee'
  WHERE `name` = 'drink' AND `body` NOT LIKE '%container%';
UPDATE `help_topic` SET `body` = 'Usage: sip <puddle|fountain|container>\n\nTastes a liquid -- a puddle on the ground, a fountain, or a carried\ndrink container -- without committing to a full `drink`. Much lower\nrisk than a full drink of whatever it might expose you to.\n\nRelated: drink fill pour'
  WHERE `name` = 'sip' AND `body` NOT LIKE '%container%';

INSERT INTO `help_topic` (`name`, `body`, `updated_by`) VALUES
('fill', 'Usage: fill <container>\n\nFills a carried drink container from a fountain or a ground puddle in\nthe room. A fountain never runs dry; a puddle is used up a little at\na time and can eventually disappear. Refuses if the container already\nholds a different liquid -- `pour` it out first to switch.\n\nRelated: pour drink sip', 'seed'),
('pour', 'Usage: pour <container>\n\nEmpties a carried drink container onto the ground as a puddle anyone\ncan see (and `fill` from later). Does nothing to an already-empty\ncontainer.\n\nRelated: fill drink sip', 'seed')
ON DUPLICATE KEY UPDATE `name` = `name`;

-- Cook profession (Sneezy -> Tobin feature audit, user 2026-07-26:
-- "professions" -- task_cook.h/.cc, real ingredient-matching recipes).
INSERT INTO `help_topic` (`name`, `body`, `updated_by`) VALUES
('cook', 'Usage: cook <recipe>\n\nCooks a known recipe from ingredients you''re carrying (and, for meat\nrecipes, a matching animal corpse on the ground). Type `cook` alone to\nsee the full recipe list. Nothing is consumed unless every ingredient\nfor the recipe is actually present.\n\nRelated: fill drink', 'seed')
ON DUPLICATE KEY UPDATE `name` = `name`;

-- Whittle profession (Sneezy -> Tobin feature audit, TODO.md "Deferred
-- decisions" -- task_whittle.h/.cc, scoped down: no bows/arrows/multi-
-- tick task, see whittle.h's own doc comment).
INSERT INTO `help_topic` (`name`, `body`, `updated_by`) VALUES
('whittle', 'Usage: whittle <item>\n\nCarves a known wooden item from wood logs you''re carrying, provided\nyou have a weapon wielded in your primary hand. Type `whittle` alone\nto see the full list of what you can make. Nothing is consumed unless\nyou have enough wood on hand for the item.\n\nRelated: cook craft', 'seed')
ON DUPLICATE KEY UPDATE `name` = `name`;

-- Full spell/skill/prayer roster import, Druid's 6 named Shaman
-- spells (user 2026-07-26). Sacrifice.
INSERT INTO `help_topic` (`name`, `body`, `updated_by`) VALUES
('sacrifice', 'Usage: sacrifice <corpse>\n\nDruid ritual (Basic, level 1). Ritually sacrifices a corpse in the\nroom to the loa -- consumed either way, but on success restores some\nof your Move (vitality). Scoped down from the original''s full multi-\nround ritual (no totem item required, one action instead of several).\n\nRelated: skin butcher', 'seed')
ON DUPLICATE KEY UPDATE `name` = `name`;

-- New topic: `uptime` (uptime command, TODO.md priority item, 2026-08-02).
INSERT INTO `help_topic` (`name`, `body`, `updated_by`) VALUES
('uptime', 'Usage: uptime\n\nShows when the server last booted (or last copyover-reset) and how\nlong it has been running since then.', 'seed')
ON DUPLICATE KEY UPDATE `name` = `name`;

-- Menu-driven loadsuit editor (`edit suit`, TODO.md priority item,
-- 2026-08-02).
UPDATE `help_topic` SET `body` = REPLACE(`body`,
  'edit rules <n> <title>    (59+) write a numbered game rule',
  'edit rules <n> <title>    (59+) write a numbered game rule\n  edit suit [name]          (56+) menu-driven newbie-suit editor --\n                                  see `help suit`')
WHERE `name` = 'edit';

INSERT INTO `help_topic` (`name`, `body`, `updated_by`) VALUES
('suit', 'Usage: edit suit [name]\n\nSenior immortal (56+) only: menu-driven editor for the named equipment\nsuits `loadsuit` grants (auto-issued to new characters, or loaded on\ndemand with `loadsuit <suit name> [target]`). With no name, lists every\nsuit defined. With a name, opens the menu on the first match -- or, if\nnothing matches, creates a brand-new empty suit under that exact name\nand opens it.\n\nFrom the menu: a number opens that item''s detail view (change its\nquantity, or delete it); A adds a new item by obj vnum, prompting for\na quantity (blank = 1) -- this is how a suit gives someone MORE than\none of the same item, e.g. two wrist bands or two boots for two feet;\nC sets which class the suit is restricted to (or clears it); D sets\nthe suit''s description. Every change here commits immediately, there\nis no separate Save step.\n\nRelated: loadsuit', 'seed')
ON DUPLICATE KEY UPDATE `name` = `name`;

-- `loadsuit` had no help topic at all -- added alongside `edit suit`
-- above since the new topic now cross-references it.
INSERT INTO `help_topic` (`name`, `body`, `updated_by`) VALUES
('loadsuit', 'Usage: loadsuit <suit name> [target]\n\nSenior immortal (56+) only: instantiates every item in a named\nequipment suit and gives them loose into inventory (not auto-equipped)\n-- to yourself with no target, or to a mob/PC in the room by name.\nSuit name matches by substring, same as most other named lookups.\nSuits are defined/edited with `edit suit`.\n\nRelated: edit suit', 'seed')
ON DUPLICATE KEY UPDATE `name` = `name`;

-- `loadsuit`/`edit suit` redesign (user, 2026-08-02: "loadsuit doesnt
-- work right... lets be number driven, and a way to delete a suit
-- needs to be implemented"). Both commands now take the numeric suit
-- id `edit suit` (no argument) lists, instead of a fuzzy name match --
-- rewriting the already-inserted rows in place, since the INSERTs
-- above are a no-op on conflict (`name` = `name`).
UPDATE `help_topic` SET `body` = REPLACE(`body`,
  'edit suit [name]          (56+) menu-driven newbie-suit editor --\n                                  see `help suit`',
  'edit suit [id]            (56+) menu-driven newbie-suit editor --\n                                  see `help suit`')
WHERE `name` = 'edit';

UPDATE `help_topic` SET `body` =
  'Usage: edit suit [id]\n      edit suit new <name>\n\nSenior immortal (56+) only: menu-driven editor for the named equipment\nsuits `loadsuit` grants (auto-issued to new characters, or loaded on\ndemand with `loadsuit <suit id> [target]`). With no argument, lists\nevery suit defined along with its id. With an id, opens the menu for\nthat suit. `edit suit new <name>` creates a brand-new empty suit under\nthat name and opens it.\n\nFrom the menu: a number opens that item''s detail view (change its\nquantity, or delete it); A adds a new item by obj vnum, prompting for\na quantity (blank = 1) -- this is how a suit gives someone MORE than\none of the same item, e.g. two wrist bands or two boots for two feet;\nC sets which class the suit is restricted to (or clears it); D sets\nthe suit''s description; X deletes the ENTIRE suit (asks for\nconfirmation first, and cannot be undone). Every change here commits\nimmediately, there is no separate Save step.\n\nRelated: loadsuit'
WHERE `name` = 'suit';

UPDATE `help_topic` SET `body` =
  'Usage: loadsuit <suit id> [target]\n\nSenior immortal (56+) only: instantiates every item in the given\nequipment suit and gives them loose into inventory (not auto-equipped)\n-- to yourself with no target, or to a mob/PC in the room by name\n(your own name is a valid target too). Suit ids are listed by `edit\nsuit` with no argument. Suits are defined/edited with `edit suit`.\n\nRelated: edit suit'
WHERE `name` = 'loadsuit';

-- Room vnum on bug/idea reports (TODO.md priority item, 2026-08-02, user:
-- "may help reproduce a bug for testing") -- rewriting the already-seeded
-- rows in place, same reason as the loadsuit rewrite above (the original
-- INSERTs are a no-op on conflict).
UPDATE `help_topic` SET `body` =
  'Usage: bug <description>\n\nReports a bug to the immortals -- your name, the date, and the room\nyou were standing in are all recorded with it (the room helps a\nbuilder reproduce it). Please be specific about what you did and what\nwent wrong. Immortals can type bug with no argument to list\noutstanding reports.\n\nRelated: delbug edbug'
WHERE `name` = 'bug';

UPDATE `help_topic` SET `body` =
  'Usage: idea <description>\n\nSuggests a new feature to the immortals -- your name, the date, and\nthe room you were standing in are all recorded with it. Immortals can\ntype idea with no argument to list outstanding suggestions.\n\nRelated: delidea'
WHERE `name` = 'idea';

-- New `typo`/`deltypo` (user, 2026-08-02: "add a typo command in the same
-- way" -- same shape as bug/idea, its own table).
INSERT INTO `help_topic` (`name`, `body`, `updated_by`) VALUES
('typo', 'Usage: typo <what''s misspelled/wrong, and where>\n\nReports a typo or other text problem to the immortals -- your name,\nthe date, and the room you were standing in are all recorded with it.\nImmortals can type typo with no argument to list outstanding reports.\n\nRelated: deltypo', 'seed'),
('deltypo', 'Usage: deltypo <id>\n\nRemoves a typo report once it has been handled. The id is the number\nshown beside each report in `typo`.\n\nRelated: typo', 'seed')
ON DUPLICATE KEY UPDATE `name` = `name`;

-- Territory/Homeland (Sneezy -> Tobin feature audit, a fresh, not-yet-
-- audited system this session -- see being.h's player_territory_t doc
-- comment for the scope-down from the real upstream's 6 race-specific
-- homeland tables to one shared 3-option set).
INSERT INTO `help_topic` (`name`, `body`, `updated_by`) VALUES
('territory', 'Homelands\n\nRight after choosing your race, you choose a homeland -- where your\ncharacter actually grew up. It leaves its own permanent mark on top of\nyour race''s, the same way class does:\n\nUrban  -- raised in a city: sharper mind, more charisma, but softer\n          and less hardy.\nRural  -- raised in a farming village: more practical and sure-\n          footed, at the cost of some charm.\nWilds  -- raised on the frontier: tougher and stronger, at the cost\n          of wit and charisma.\n\nHomeland is chosen once, at creation, and cannot be changed\nafterward. It''s shown in your `score` as your homeland. Related: score', 'seed')
ON DUPLICATE KEY UPDATE `name` = `name`;

INSERT INTO `help_topic` (`name`, `body`, `updated_by`) VALUES
('bank', 'Usage: bank [balance | deposit <amount> | withdraw <amount>]\n\nBanking is done through a bank keeper -- you must be standing in a\nroom with one to use this command at all (`bank` with no keeper\npresent just says so). Once you are:\n\n`bank` or `bank balance` -- shows how much gold you are carrying and\n                             how much is safely in the bank.\n`bank deposit <amount>`  -- moves gold from your wallet into the\n                             bank.\n`bank withdraw <amount>` -- moves gold from the bank back into your\n                             wallet.\n\nBanked gold is safe from anything that can take your carried gold --\nkeep only what you need on hand. Related: treasury', 'seed')
ON DUPLICATE KEY UPDATE `name` = `name`;

-- Command separator (user, 2026-08-08: "implement a command seperator in
-- the game"). Freed up `;` from its old leading-character wiznet
-- shorthand (dropped same session, user: "use ';' anyway, drop the
-- wiznet shorthand") -- rewrite wiznet's own body to stop advertising the
-- retired shortcut, and add the new `separator` topic.
UPDATE `help_topic` SET `body` = 'Usage: wiznet <message>   |   wiznet @<level> <message>\n\nImmortals only: a private broadcast channel among the immortals. Your\nmessage reaches every online immortal (and yourself), out of sight of\nmortals. Add `@<level>` to narrow delivery to only immortals at or\nabove that level -- `wiznet @59 <msg>` reaches Administrator+ only.\n\nThe old `;<message>` one-key shortcut was retired 2026-08-08 when `;`\nbecame the general command separator (see HELP SEPARATOR) -- define\nyour own alias if you want a quick one-key habit back.'
  WHERE `name` = 'wiznet' AND `updated_by` = 'seed';

INSERT INTO `help_topic` (`name`, `body`, `updated_by`) VALUES
('separator', '`;` chains several commands into one typed line, run in order:\n  north;look;inventory\nruns `north`, then `look`, then `inventory`.\n\nEach piece is trimmed and dispatched on its own, so a leading apostrophe\nstill works per-piece (say-shorthand: hi;bye said with a leading\napostrophe on each piece). There is no way to escape a literal `;`\ninside a message -- typing one into a say/tell/similar sentence will\nsplit it there, same as any other MUDs command separator. If a\nchained command ends your connection (like quit!), anything after it\nin the same line is not run.', 'seed')
ON DUPLICATE KEY UPDATE `name` = `name`;

-- egotrip expansion + new `force` command (user, 2026-08-08). See
-- combat.c/mob_ai.c/cmd_egotrip.c/cmd_force.c for the actual features.
UPDATE `help_topic` SET `body` = 'Usage: egotrip <subcommand>\n\nImmortal toy-box, expanded 2026-08-08 to cover every subcommand that\nmaps onto a real Tobin system:\n\n  blast <target>              -- halves a target''s current HP (never\n                                 below 1) with a bolt of lightning.\n  disease <target> <disease>  -- inflicts a named disease. Choices:\n                                 cold, dysentery, flu, pneumonia,\n                                 leprosy, gangrene, plague, scurvy.\n  cleanse                     -- cures every disease and poison on\n                                 every connected being, world-wide.\n  stupidity                   -- casts stupidity on every connected\n                                 mortal at once.\n  wander                      -- forces every eligible mob in your\n                                 own room to attempt a wander move\n                                 right now (charmed pets and\n                                 sentinels still won''t budge).\n  crit <target>               -- forces a random MINOR limb sever\n                                 (never a killing blow), no fight\n                                 required.\n\nNot ported -- no equivalent Tobin system exists to hang them on:\n  deity, bless, portal, hate, garble. `teleport` isn''t ported either,\n  but only because it''s already covered by the separate `transfer`\n  command. Related: force'
  WHERE `name` = 'egotrip' AND `updated_by` = 'seed';

INSERT INTO `help_topic` (`name`, `body`, `updated_by`) VALUES
('force', 'Usage: force <target> <command>\n\nLevel 55+ only: makes another player or a mob run a command as\nthemselves -- exactly as if they''d typed it. A player target''s own\ncommand feedback goes to their own screen, not yours (only your own\nconfirmation line does). A player target is found anywhere in the\ngame; a mob target must be in your own room. Can''t force yourself,\nand can''t force another immortal ranked equal to or above you.\n\nA forced mob (or lowbie player) still can''t reach a command above\ntheir own level -- forcing someone to `egotrip` or `shutdown` just\ngets them the same "Huh?!" typing it themselves would. Related: egotrip transfer', 'seed')
ON DUPLICATE KEY UPDATE `name` = `name`;

-- Trophy system (TODO.md, user: "implement trophy system from Sneezy").
-- See trophy.h/trophy.c/trophy_repo.c/cmd_trophy.c.
INSERT INTO `help_topic` (`name`, `body`, `updated_by`) VALUES
('trophy', 'Usage: trophy [name]\n\nShows how much XP you will earn the next time you kill each mob\nyou''ve already killed before -- repeat kills of the same mob are\nworth less and less (down to a floor, never zero), so grinding one\nspawn stops paying off after a while. Leave that mob alone and the\npenalty fades back out over time on its own.\n\nWith no argument, lists every mob you have a trophy count for. Add\na name to narrow the list to mobs whose name contains it, e.g.\n`trophy rat`. Related: score', 'seed')
ON DUPLICATE KEY UPDATE `name` = `name`;

-- docs/Spell Assignments.xlsx gap audit, 2026-08-08 (TODO.md, "Implement
-- missing skills from docs/Spell Assignments.xlsx"). See
-- skill.c/combat.c/cmd_pray.c/cmd_cast.c/vitals.c for the actual code.
INSERT INTO `help_topic` (`name`, `body`, `updated_by`) VALUES
('defense', 'A passive combat skill, known from level 1 by every class. Improves\nwith use (learn-by-doing) and makes you slightly harder to hit --\nyour first passive defensive skill, alongside `toughness` (damage\nreduction) and `focused avoidance` (also to-hit reduction, unlocked\nlater). No command to use it; it just works. Related: toughness focused avoidance', 'seed'),
('praying', 'A passive Cleric skill, known from level 25. Improves with use\n(learn-by-doing) every time you `pray` -- your general prayer\nproficiency, separate from any one specific prayer''s own skill.\nNo command to use it; it trains automatically alongside whatever\nyou pray. Related: pray wizardry', 'seed'),
('casting', 'A passive Mage skill, known from level 9. Improves with use\n(learn-by-doing) every time you `cast` -- a second, separate\nspellcasting-proficiency stat alongside `wizardry`, trained\nautomatically alongside whatever you cast. No command to use it.\nRelated: cast wizardry', 'seed'),
('swim', 'A passive combat skill, known from level 1 by every class. Improves\nwith use (learn-by-doing) and reduces the damage you take from\ndrowning while underwater without water-breathing -- up to half off\nat full proficiency. No command to use it; it just works when you\nneed it. Related: score', 'seed')
ON DUPLICATE KEY UPDATE `name` = `name`;

-- egotrip `damn` subcommand + mob targeting for blast/damn/disease/crit
-- (user follow-up, 2026-08-08: "bypass xp loss on an egotrip hit" /
-- "make egotrip usable on mobs too"). See combat.c/cmd_egotrip.c.
UPDATE `help_topic` SET `body` = 'Usage: egotrip <subcommand>\n\nImmortal toy-box, expanded 2026-08-08 (now also usable on mobs, room-only)\nto cover every subcommand that maps onto a real Tobin system:\n\n  blast <target>              -- halves a target''s current HP (never\n                                 below 1) with a bolt of lightning.\n  damn <target>                -- instantly kills the target for free --\n                                 no XP loss (an immortal''s own PK-neutral\n                                 status waives the usual death penalty).\n                                 Real corpse/respawn, just no cost.\n  disease <target> <disease>  -- inflicts a named disease. Choices:\n                                 cold, dysentery, flu, pneumonia,\n                                 leprosy, gangrene, plague, scurvy.\n  cleanse                     -- cures every disease and poison on\n                                 every connected being, world-wide.\n  stupidity                   -- casts stupidity on every connected\n                                 mortal at once.\n  wander                      -- forces every eligible mob in your\n                                 own room to attempt a wander move\n                                 right now (charmed pets and\n                                 sentinels still won''t budge).\n  crit <target>               -- forces a random MINOR limb sever\n                                 (never a killing blow), no fight\n                                 required.\n\nTargeting: blast/damn/disease/crit find any connected PLAYER anywhere\nin the game; a MOB target must be in your own room (no world-wide mob\nindex exists).\n\nNot ported -- no equivalent Tobin system exists to hang them on:\n  deity, bless, portal, hate, garble. `teleport` isn''t ported either,\n  but only because it''s already covered by the separate `transfer`\n  command. Related: force' WHERE `name` = 'egotrip';
-- docs/Spell Assignments.xlsx gap audit, batch B, 2026-08-08. See
-- being.h/combat.c/vitals.c/cmd_move.c/cmd_bandage.c for the actual code.
INSERT INTO `help_topic` (`name`, `body`, `updated_by`) VALUES
('bandage', 'Usage: bandage [target]\n\nA passive-gated active skill, known from level 1 by every class.\nTreats a bleeding limb -- yours, or another player''s in your room --\nusing a carried bandage item. A limb starts bleeding once it takes\nenough damage to be "hurt rather badly" or worse, and keeps chipping\nyour HP every tick until treated (or fully healed some other way).\nA successful bandage stops the bleeding, heals a little HP, and\nconsumes the bandage; a failed attempt keeps the bandage for another\ntry. Requires a carried bandage item and improves with use.\nRelated: score limbs', 'seed'),
('hiking', 'A passive combat skill, known from level 1 by every class. Improves\nwith use (learn-by-doing) and reduces the movement cost of walking\nbetween rooms -- up to half off at full proficiency, stacking with\nflying and mounted discounts. No command to use it; it just works\nevery time you move. Related: score move', 'seed')
ON DUPLICATE KEY UPDATE `name` = `name`;

-- kick can now start a fight (user, 2026-08-05/08). See cmd_kick.c.
UPDATE `help_topic` SET `body` = 'Usage: kick [target]\n\nAn unarmed kick for bonus damage, on top of the normal automatic\ncombat round. If you''re already fighting, `kick` alone hits your\ncurrent opponent. If you AREN''T fighting yet, `kick <target>` also\nstarts the fight -- same as `attack`, just with a free extra hit\nthe moment it lands.\nRelated: skills practice attack kill\nClasses: Warrior, Thief, Monk (level 1)' WHERE `name` = 'kick';

-- `cast` grew a multi-round delay (user 2026-08-09: "spell casting
-- should take 2-3 rounds before hitting with purple colored
-- messaging... druids should have modified messages... forest
-- flavor... druid messaging should be <y>") -- previously resolved
-- instantly; the seeded row above predates this, so update it
-- explicitly, same "no-op on the already-seeded row" precedent as the
-- offensive-spell-breadth update above. `pray` is unaffected (stays
-- instant), not touched here.
UPDATE `help_topic` SET `body` = 'Usage: cast <spell> [target]\n\nMages and Druids only: casts a spell from your class''s roster (see\n`skills`). You need a spell component -- any carried item keyworded\n"component" -- on hand; it is consumed on a successful cast. Also\nneeds enough practiced discipline in that spell''s tier -- see `help\npractice`. Once cast, the spell takes 2-3 rounds to complete -- you\nwill see a few lines of flavor text each round (purple for Mages,\nforest-flavored yellow for Druids) and cannot act again until it\nfinishes, then the spell''s real effect lands. An offensive spell can\ntarget someone else in the room (`cast gust <name>`) and will draw you\ninto a fight with them if you aren''t already fighting anyone; left\nblank, it keeps hitting whoever you''re already fighting. If your\ntarget is gone or you go down before the spell completes, it fizzles.\nClerics use `pray` instead, which resolves instantly.'
  WHERE `name` = 'cast' AND `updated_by` = 'seed';

UPDATE `help_topic` SET `body` = 'Usage: cast knot\n\nTears a gap in reality and steps you through it to safety -- an\nemergency escape, not a defensive ward. Self only, no target.\nWon''t work in an area whose defenses are too strong to tear.\nRequires: any item keyworded "component" (e.g., a pouch of spell components) (`cast`)\nRelated: skills practice cast pray affects\nApprox. Level: 50\nDiscipline: 85%\nClasses: Mage' WHERE `name` = 'knot';

-- `sharpen`/`smooth` -- missing-skill audit batch C, 2026-08-09. See
-- obj.h/cmd_sharpen.c/combat.c for the actual code.
INSERT INTO `help_topic` (`name`, `body`, `updated_by`) VALUES
('sharpen', 'Usage: sharpen\n\nAn active skill, known from level 1 by every class. Sharpens whatever\nedged or piercing weapon you are wielding using a carried whetstone,\nraising its condition toward a maximum -- a sharper weapon lands a\nslightly harder hit. Has no effect on blunt weapons (see `smooth`\ninstead) and refuses while fighting. Improves with use. Requires a\ncarried whetstone.\nRelated: smooth skills wield', 'seed'),
('smooth', 'Usage: smooth\n\nAn active skill, known from level 1 by every class. Files the nicks\nand dents out of whatever blunt weapon you are wielding using a\ncarried file, raising its condition toward a maximum -- a smoother\nweapon lands a slightly harder hit. Has no effect on edged or piercing\nweapons (see `sharpen` instead) and refuses while fighting. Improves\nwith use. Requires a carried file.\nRelated: sharpen skills wield', 'seed')
ON DUPLICATE KEY UPDATE `name` = `name`;

-- `alcoholism` -- missing-skill audit batch C, 2026-08-09. See
-- being.h/vitals.c/liquids.c/cmd_drink.c/cmd_sip.c/combat.c.
INSERT INTO `help_topic` (`name`, `body`, `updated_by`) VALUES
('alcoholism', 'A passive skill, known from level 1 by every class. Reduces how\nintoxicated you get from a single alcoholic drink -- an experienced\ndrinker can hold their liquor. Improves with use (every alcoholic\ndrink you finish is a chance to train it). Intoxication itself fades\non its own over time; a fighter who is too far gone swings less\naccurately, and drinking far too much can knock you out cold. Related:\ndrink sip score', 'seed')
ON DUPLICATE KEY UPDATE `name` = `name`;

-- Deikhan mounted-combat trio (`calm mount`/`charge`/`advanced riding`) --
-- missing-skill audit batch C, 2026-08-09. See cmd_charge.c/cmd_ride.c/
-- combat.c/skill.c.
INSERT INTO `help_topic` (`name`, `body`, `updated_by`) VALUES
('calm mount', 'A passive skill, known from level 1 by every class. When a real hit\nlands on you while mounted, your mount has a chance to panic and\nthrow you to the ground -- this skill (and, to a lesser extent,\n`advanced riding`) reduces that chance, down to none at full\nproficiency. Improves with use. Related: ride charge advanced riding', 'seed'),
('charge', 'Usage: charge <target>\n\nAn active skill, known from level 20 by every class. Requires being\nmounted (`ride`) and not already fighting -- a charge only works as an\nopening move. On a successful roll, delivers a heavy bonus-damage hit\nthat knocks the target down; a failed roll still starts the fight,\njust without the bonus. `advanced riding` adds extra damage. Related:\nride advanced riding calm mount', 'seed'),
('advanced riding', 'A passive skill, known from level 40 by every class. Improves with use.\nGrants a real bonus to your chance of successfully mounting a creature\n(`ride`), adds extra damage to `charge`, and helps `calm mount` keep you\nin the saddle. Related: ride charge calm mount', 'seed')
ON DUPLICATE KEY UPDATE `name` = `name`;

-- Ranger beast-charm pair (`beast charm`/`befriend beast`) -- missing-
-- skill audit batch C, 2026-08-09. See cmd_cast.c/skill.c.
INSERT INTO `help_topic` (`name`, `body`, `updated_by`) VALUES
('beast charm', 'Usage: cast beast charm\n\nDruid only. Calls a loyal gray wolf to your side, obedient to your\nwill for a while -- the same charmed-companion mechanic `animal\ncompanion` uses. Self only, no target. Requires: any item keyworded\n"component"\nRelated: skills practice cast animal companion befriend beast', 'seed'),
('befriend beast', 'Usage: cast befriend beast\n\nDruid only. The gentler half of the beast-charm pair -- calls a loyal\ngray wolf to your side through friendship rather than command,\nobedient to your will for a while. Self only, no target. Requires:\nany item keyworded "component"\nRelated: skills practice cast animal companion beast charm', 'seed')
ON DUPLICATE KEY UPDATE `name` = `name`;

-- 5 Shaman/Druid spells (flatulence/shield of mists/thornflesh/living
-- vines/raze) -- missing-skill audit batch C, 2026-08-09. See
-- cmd_cast.c/combat.c/cmd_move.c/affect.h/skill.c.
INSERT INTO `help_topic` (`name`, `body`, `updated_by`) VALUES
('flatulence', 'Usage: cast flatulence\n\nDruid only. A room-wide attack -- noxious fumes damage every occupant\nof your room except your own group and any immortal. No target needed.\nA rare mishap chokes you instead. Requires: any item keyworded\n"component"\nRelated: skills practice cast raze', 'seed'),
('shield of mists', 'Usage: cast shield of mists [target]\n\nDruid only. Wraps you (or a willing room-mate) in a thick green mist,\nmaking the recipient noticeably harder to hit for a while. Defaults to\nyourself if no target is given. Requires: any item keyworded\n"component"\nRelated: skills practice cast thornflesh living vines', 'seed'),
('thornflesh', 'Usage: cast thornflesh\n\nDruid only. Self only. Real thorns emerge from your body -- any melee\nattacker who lands a hit on you takes a bite of that same damage back.\nRefuses if already active. Requires: any item keyworded "component"\nRelated: skills practice cast shield of mists', 'seed'),
('living vines', 'Usage: cast living vines <target>\n\nDruid only. Outdoors only. Vines burst from the earth around the\ntarget, wrapping their legs (can''t move) and throwing off their\nfooting (easier to hit). Requires: any item keyworded "component"\nRelated: skills practice cast entangling roots raze', 'seed'),
('raze', 'Usage: cast raze <target>\n\nDruid only. The single most powerful attack spell on the Druid\nroster -- calls upon ancient spirits to erase the target''s very\nexistence, dealing severe damage with a real chance of doubling\nagain. Refuses against an immortal. Requires: any item keyworded\n"component"\nRelated: skills practice cast flatulence', 'seed')
ON DUPLICATE KEY UPDATE `name` = `name`;
