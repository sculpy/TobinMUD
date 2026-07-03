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
('look', 'Usage: look\n\nShows the room you are in: its name, description, and everyone\nstanding there with you. You also look automatically whenever you\nenter the world.', 'seed'),
('who', 'Usage: who\n\nLists everyone currently playing, with their level (or immortal rank\ntitle) shown in brackets before their name.', 'seed'),
('score', 'Usage: score\n\nShows your character sheet: attributes, level, experience, and hit\npoints. Limbs appear here only once they are hurt -- see `help limbs`\nfor the full breakdown any time.', 'seed'),
('color', 'Usage: color [on|off]\n\nToggles ANSI color rendering for your connection. With no argument,\nshows the current setting. Color tags in the world (like <r>this<z>)\nrender as real colors when on and are stripped when off.', 'seed'),
('attack', 'Usage: attack <player>   (alias: kill -- identical)\n\nMortals: starts a fight with another player in your room; combat\nresolves in rounds, every hit lands on a specific limb, and you can\nabbreviate the target''s name (`attack clau` reaches Claudius).\nImmortals: an instant slay -- no rounds, no wait, the target dies.', 'seed'),
('kill', 'Usage: kill <player>   (alias: attack -- identical)\n\nMortals: starts a fight with another player in your room; combat\nresolves in rounds, every hit lands on a specific limb, and you can\nabbreviate the target''s name (`kill clau` reaches Claudius).\nImmortals: an instant slay -- no rounds, no wait, the target dies.', 'seed'),
('say', 'Usage: say <message>   (shorthand: ''<message>)\n\nSays something to everyone in your room. The apostrophe shorthand\nneeds no space: ''hello says "hello".', 'seed'),
('limbs', 'Usage: limbs\n\nShows the health of all thirteen of your limbs as percentages, with\nan injury note on any limb below 20%. A destroyed limb (0%) makes\nyour own attacks less accurate until you are made whole again.', 'seed'),
('help', 'Usage: help [topic]\n\nWith no argument, lists every command available to you. With a topic\n(any command name, abbreviations welcome), shows its full help text.', 'seed'),
('wizhelp', 'Usage: wizhelp\n\nImmortals only: lists the immortal-only commands, with the minimum\nlevel each one requires.', 'seed'),
('goto', 'Usage: goto <room vnum>\n\nImmortals only: teleport directly to any room by its vnum. Useful\nvnums: 0 (The Void), 1 (Imperia).', 'seed'),
('promote', 'Usage: promote <name> [level]\n\nImmortals only: set another player''s level (default 51, the first\nimmortal rank). You cannot set anyone above your own level, and the\nname must be typed in full. Works on offline players too; an online\ntarget changes immediately and is told. Also demotes.', 'seed'),
('hedit', 'Usage: hedit <topic>\n\nLevel 56+ only: edit (or create) a help topic in a line editor. Any\nexisting text is shown first; lines you type are appended. Finish\nwith a single `.` on its own line to save, or `~` to abort without\nsaving. Topics are stored in the database and shown by `help <topic>`.', 'seed'),
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
('exits', 'Usage: exits\n\nLists this room''s exits and the name of the place each one leads to.\n(`look` shows the same directions as a one-line summary.)', 'seed'),
('loadroom', 'Usage: loadroom [vnum]\n\nImmortals only: sets the room your character enters the game in at\nlogin (e.g. `loadroom 43`). Bare `loadroom` shows the current setting.\nThe room must exist.', 'seed'),
('prompt', 'Usage: prompt [hp]\n\n`prompt hp` toggles your hit points into the prompt line ("HP: 25 >").\nBare `prompt` shows the current setting. Your choice is saved with\nyour character. More stats will join the prompt as they exist.', 'seed'),
('mortal', 'Usage: mortal   (and later: immort)\n\nImmortals only: set your divinity aside and walk the world as a level\n50 mortal -- wait-states apply, you can be killed, and your immortal\ncommands are out of reach. Your true rank is kept safe (even through\ndeath or logout); type `immort` at any time to reclaim it.', 'seed'),
('redit', 'Usage: redit [field] [args]   (level 51+ builders)\n\nEdits the room you are standing in; every change saves to the\ndatabase immediately. Bare `redit` shows the room summary.\n\n  redit name <text>          -- set the room title\n  redit description          -- line editor (`.` saves, `~` aborts)\n  redit sector_type [n]      -- show or set the sector number\n  redit exit <dir> <toroom>  -- link an exit; creates the target room\n                                if needed and fixes the reverse exit\n  redit exit <dir> -1        -- delete an exit', 'seed')
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

-- Migration: immortal look header ([vnum] name [sector] [flags]).
UPDATE `help_topic` SET `body` = 'Usage: look\n\nShows the room you are in: its name, description, obvious exits, and\neveryone standing there with you. You also look automatically whenever\nyou enter the world. Immortals additionally see the room''s vnum,\nsector type, and flags in the header line.'
  WHERE `name` = 'look' AND `updated_by` = 'seed';

-- Migration: attack/kill are full aliases (immortals instakill on both).
UPDATE `help_topic` SET `body` = 'Usage: attack <player>   (alias: kill -- identical)\n\nMortals: starts a fight with another player in your room; combat\nresolves in rounds, every hit lands on a specific limb, and you can\nabbreviate the target''s name (`attack clau` reaches Claudius).\nImmortals: an instant slay -- no rounds, no wait, the target dies.'
  WHERE `name` = 'attack' AND `updated_by` = 'seed';
UPDATE `help_topic` SET `body` = 'Usage: kill <player>   (alias: attack -- identical)\n\nMortals: starts a fight with another player in your room; combat\nresolves in rounds, every hit lands on a specific limb, and you can\nabbreviate the target''s name (`kill clau` reaches Claudius).\nImmortals: an instant slay -- no rounds, no wait, the target dies.'
  WHERE `name` = 'kill' AND `updated_by` = 'seed';

-- Migration: redit dropped to 51+ (every immortal builds).
UPDATE `help_topic` SET `body` = 'Usage: redit [field] [args]   (level 51+ builders)\n\nEdits the room you are standing in; every change saves to the\ndatabase immediately. Bare `redit` shows the room summary.\n\n  redit name <text>          -- set the room title\n  redit description          -- line editor (`.` saves, `~` aborts)\n  redit sector_type [n]      -- show or set the sector number\n  redit exit <dir> <toroom>  -- link an exit; creates the target room\n                                if needed and fixes the reverse exit\n  redit exit <dir> -1        -- delete an exit'
  WHERE `name` = 'redit' AND `updated_by` = 'seed';
