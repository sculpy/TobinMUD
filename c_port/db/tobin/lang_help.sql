-- Help topics for the Tier-4 speak-a-language subsystem (2026-08-16).
-- Bodies follow the same Usage/Related/Approx. Level/Classes field
-- convention cmd_help.c parses into its colorized footer (see the encamp
-- topic). Command topics (`speak`) get their Minimum Level from cmd_table
-- automatically, so it is not repeated in the body.

INSERT INTO help_topic (name, body, updated_by) VALUES
('speak',
 'Usage: speak [language]\n\nChoose the tongue you speak in. With no argument, shows your current tongue and every language you can speak. Everyone starts out speaking Common, which is never garbled. Other tongues must be learned before you can speak them -- and anyone who has not learned a tongue hears it come out garbled, less and less so as their own skill in it grows.\nRelated: languages say tell whisper',
 'system'),
('languages',
 'The tongues of the world. Speak one with `speak <language>`; your fluency (and how much you understand when others use it) improves the more you are exposed to it. The eight tongues:\n\n  Common             -- the trade tongue; understood by all, never garbled\n  Trollish           -- the guttural speech of trolls\n  Avian              -- the clipped, squawking speech of birdfolk\n  Fish Burble        -- the watery, gurgling speech of fishfolk\n  Bullycroak         -- the croaking speech of frogmen\n  Gutter Cant        -- streetwise back-alley slang\n  Gnoll Jargon       -- the broken, mangled tongue of gnolls\n  Troglodyte Pidgin  -- the halting, syllable-chopped speech of trogs\nRelated: speak say tell whisper',
 'system'),
('common',
 'Usage: speak common\n\nThe common trade tongue, spoken and understood everywhere. Speech in Common is never garbled -- it is the language everyone falls back on. A fluent Common speaker is also easier to follow even when speaking a foreign tongue. Known by every class and improves with use.\nRelated: languages speak\nApprox. Level: 1\nClasses: all',
 'system'),
('trollish',
 'Usage: speak trollish\n\nThe guttural, growling speech of trolls, thick with dropped consonants and glottal stops. Listeners who have not learned it hear only a mangled snarl. Known by every class and improves with use.\nRelated: languages speak\nApprox. Level: 20\nClasses: all',
 'system'),
('avian',
 'Usage: speak avian\n\nThe clipped, squawking speech of the birdfolk, broken up by sudden squawks and whistles. Listeners who have not learned it hear little but birdsong. Known by every class and improves with use.\nRelated: languages speak\nApprox. Level: 20\nClasses: all',
 'system'),
('fish burble',
 'Usage: speak fish burble\n\nThe watery, gurgling speech of fishfolk, full of bubbles and drawn-out gurgles. Listeners who have not learned it hear only glugs and bloops. Known by every class and improves with use.\nRelated: languages speak\nApprox. Level: 20\nClasses: all',
 'system'),
('bullycroak',
 'Usage: speak bullycroak\n\nThe croaking, soft-palated speech of frogmen, where hard sounds slur into wet croaks. Listeners who have not learned it hear only croaking. Known by every class and improves with use.\nRelated: languages speak\nApprox. Level: 20\nClasses: all',
 'system'),
('gutter cant',
 'Usage: speak gutter cant\n\nThe streetwise back-alley slang of thieves and rogues, clipped and slurred so outsiders cannot follow. Listeners who have not learned it hear only muddled slang. Known by every class and improves with use.\nRelated: languages speak\nApprox. Level: 10\nClasses: all',
 'system'),
('gnoll jargon',
 'Usage: speak gnoll jargon\n\nThe broken, mangled tongue of gnolls, words chewed up and spat back half-formed. Listeners who have not learned it hear only garbled barking. Known by every class and improves with use.\nRelated: languages speak\nApprox. Level: 10\nClasses: all',
 'system'),
('troglodyte pidgin',
 'Usage: speak troglodyte pidgin\n\nThe halting speech of troglodytes, every word chopped into broken, hyphenated syllables. Listeners who have not learned it hear only stuttering fragments. Known by every class and improves with use.\nRelated: languages speak\nApprox. Level: 10\nClasses: all',
 'system')
ON DUPLICATE KEY UPDATE body=VALUES(body), updated_by=VALUES(updated_by);
