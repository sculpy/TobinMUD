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
('limbs', 'Usage: limbs\n\nShows the health of all thirteen of your limbs as percentages, with\nan injury note on any limb below 20%. A destroyed limb (0%) makes\nyour own attacks less accurate until you are made whole again.', 'seed'),
('flee', 'Usage: flee\n\nWhile fighting, makes a desperate attempt to escape through a random\nexit. You do not choose the direction and it does not always work -- a\nfailed flee leaves you in the fight. On success both sides stop\nfighting and you bolt to a neighbouring room.', 'seed'),
('bug', 'Usage: bug <description>\n\nReports a bug to the immortals -- your name and the date are recorded\nwith it. Please be specific about what you did and what went wrong.\nImmortals can type bug with no argument to list outstanding reports.', 'seed'),
('delbug', 'Usage: delbug <id>\n\nAdministrator (59+) only: removes a bug report once it has been\nhandled. The id is the number shown beside each report in `bug`.', 'seed'),
('newbie', 'Usage: newbie <message>\n\nA help channel for new players. Everyone starts on it, so newcomers can\nask questions and veterans can answer. Turn it off (or back on) with\n`toggle newbie`; you must be on the channel to speak on it.', 'seed'),
('rules', 'Usage: rules [number]\n\nWith no argument, lists the numbered game rules. `rules <number>` shows\nthat rule in full. Please read them -- ignorance is no excuse.', 'seed'),
('edrules', 'Usage: edrules <number> <title>\n\nAdministrator (59+) only: writes or rewrites a numbered game rule. Give\nthe rule number and a title, then type the rule text into the line\neditor (/s saves, /a aborts, /b blanks, /f reflows to width). Players\nread rules with the rules command.', 'seed'),
('help', 'Usage: help [topic]\n\nWith no argument, lists every command available to you. With a topic\n(any command name, abbreviations welcome), shows its full help text.', 'seed'),
('wizhelp', 'Usage: wizhelp\n\nImmortals only: lists the immortal-only commands, with the minimum\nlevel each one requires.', 'seed'),
('exec', 'Usage: exec <shell command>\n\nImplementor-only (level 60): runs a command on the host box and shows\nits output. Fenced for safety -- a blocklist refuses dangerous commands\n(process kills, disk wipes, reboots, privilege escalation, touching the\nmud), every command runs under a timeout so it cannot freeze the game,\nand each use is logged. Not a root shell.', 'seed'),
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
('open', 'Usage: open <direction>\n\nOpens a door blocking that exit, if there is one. A locked door can''t\nbe opened this way -- that needs a key, which isn''t built yet. Once\nopen, you (and everyone else) can walk through; closing it again with\n`close` blocks movement until it''s reopened.', 'seed'),
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
('wear', 'Usage: wear <item>\n\nPuts on a carried item into its body slot (head, body, legs, and so\non). Refuses if you''re already wearing something there, or if the\nitem isn''t wearable there at all -- a holdable item (weapon or\notherwise) isn''t worn this way; see `hold`/`wield` instead.', 'seed'),
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
UPDATE `help_topic` SET `body` = 'Usage: vnum <room|obj|mob> <pattern>\n\nBuilder tool (level 51+): lists the vnums and names of rooms, objects,\nor mobiles whose name contains <pattern> (case-insensitive). Handy for\nfinding a prototype''s number to oload/mload or goto. Results are listed\nlowest vnum first, a page at a time.'
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
UPDATE `help_topic` SET `body` = 'Usage: open <direction|container>\n\nOpens a closed door blocking that exit, or a closeable container (a\nbag, chest, and the like) that you''re carrying or that''s on the floor.\nA locked door or container can''t be opened this way -- that needs a\nkey, which isn''t built yet. Close it again with `close`.'
  WHERE `name` = 'open' AND `updated_by` = 'seed';
UPDATE `help_topic` SET `body` = 'Usage: close <direction|container>\n\nCloses a door blocking that exit, or a closeable container you''re\ncarrying or that''s on the floor. A closed door blocks movement; a\nclosed container keeps its contents sealed until someone opens it\nagain with `open`.'
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
('load', 'Usage: load <mob|obj> <vnum|name>\n\nBuilder tool (level 51+): spawns a copy of a mob or object prototype\ninto the room you''re standing in -- replaces the old separate oload/\nmload commands. Give the category (mob or obj -- a single letter M/O\nworks too) then an exact vnum, or a name/keyword (`load obj sword`\nloads the first object whose name contains "sword"; `load mob demon`\nthe first mobile whose name contains "demon"). There''s no automatic\nworld respawn yet, so anything placed this way is gone if the server\nrestarts -- only what players are actually carrying survives that.', 'seed')
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
('hit', 'Usage: hit <player>\n\nStarts a real fight through the normal multi-round combat process --\nfor anyone, including immortals. Unlike kill/attack (an instant slay for\nan immortal), hit always resolves in rounds, every hit landing on a\nspecific limb, so an immortal can use it to actually fight something\ninstead of instakilling it.', 'seed')
ON DUPLICATE KEY UPDATE `name` = `name`;

-- `zonereset`/`zoneassign` merged into `zone`/`edzone` (2026-07-10) --
-- remove the superseded topics from any earlier deploy.
DELETE FROM `help_topic` WHERE `name` IN ('zonereset', 'zoneassign');

-- New topics: `zone` and `edzone` (2026-07-10) -- Zones Part 2 + identity.
INSERT INTO `help_topic` (`name`, `body`, `updated_by`) VALUES
('zone', 'Usage: zone reset <zone number>\n       zone list\n\nBuilder tool (level 51+). `zone reset` force-runs a zone''s reset right\nnow, exactly like its periodic timer would -- loads any mobs/objects\nstill under their per-room cap, equips/gives them items, sets doors.\nEvery zone also resets automatically: once (in full) whenever the\nserver starts, then periodically after that on its own lifespan.\n`zone list` shows every zone with its name, enabled state, lifespan,\nand assigned builders. For editing a zone''s own properties or\nassigning builders, see `edzone`.', 'seed'),
('edzone', 'Usage: edzone <zone number>\n\nMenu-driven zone editor (level 51+, but see below): change a zone''s\nname, enabled state, lifespan, and vnum range; assign or un-assign\nbuilders (selecting a builder already assigned un-assigns them); or\nforce a reset right now. A level 51-54 builder can only edzone (or\nedroom) a zone they are assigned to -- editing any other zone is\nrefused. 55+ can always edit anything, and content outside any zone is\nunrestricted for every builder.', 'seed')
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
('delidea', 'Usage: delidea <id>\n\nAdministrator (59+) only: removes an idea once it has been handled.\nThe id is the number shown beside each idea in `idea`.', 'seed')
ON DUPLICATE KEY UPDATE `name` = `name`;
