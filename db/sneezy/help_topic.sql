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
('attack', 'Usage: attack <player>\n\nStarts a fight with another player in your room. Combat resolves in\nrounds of about a second each; every hit lands on a specific limb.\nYou can abbreviate the target''s name: `attack clau` reaches Claudius.', 'seed'),
('kill', 'Usage: kill <player>\n\nFor mortals this is identical to `attack`. For immortals it is an\ninstant slay -- no rounds, no wait, the target simply dies.', 'seed'),
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
('edit', 'Usage: edit [field] [args]   (level 56+ builders)\n\nEdits the room you are standing in; every change saves to the\ndatabase immediately. Bare `edit` shows the room summary.\n\n  edit name <text>          -- set the room title\n  edit description          -- line editor (`.` saves, `~` aborts)\n  edit sector_type [n]      -- show or set the sector number\n  edit exit <dir> <toroom>  -- link an exit; creates the target room\n                               if needed and fixes the reverse exit\n  edit exit <dir> -1        -- delete an exit', 'seed')
ON DUPLICATE KEY UPDATE `name` = `name`;
