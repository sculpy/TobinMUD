-- Tips system (TODO.md: "tips command + periodic tip echoes, per-player
-- newbie toggle, tipedit (53+)"). Tobin-specific (not upstream seed).
--
-- `body` has no UNIQUE key (short tips can legitimately overlap in
-- wording more easily than a news headline) -- idempotent re-runs guard
-- with a `WHERE NOT EXISTS` on the exact body text instead.

CREATE TABLE IF NOT EXISTS `tip` (
  `id` int NOT NULL AUTO_INCREMENT,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `added_by` varchar(64) NOT NULL DEFAULT '',
  `body` varchar(255) NOT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT INTO `tip` (`added_by`, `body`)
SELECT 'The TobinMUD Team', t.body FROM (
  SELECT 'Drinking from a puddle carries real risk -- poison and disease are both possible. sip is safer than drink if you just want to check the water.' AS body
  UNION ALL SELECT 'A hospital can cure a damaged limb, a disease, or poison, all for gold -- goto hospital finds the nearest one.'
  UNION ALL SELECT 'toggle nospam hides "you miss"/"they miss" combat messages if you find them cluttering your screen.'
  UNION ALL SELECT 'toggle autoloot automatically grabs everything from a defeated opponent''s corpse.'
  UNION ALL SELECT 'consider a target before you fight it -- it sizes up the odds in plain English.'
  UNION ALL SELECT 'A closed door can be opened, but a locked one needs a key -- or someone who can pick it.'
  UNION ALL SELECT 'Reaching a new skill or spell''s level is not enough on its own -- most also need practicing at a guildmaster first.'
  UNION ALL SELECT 'goto guildmaster and goto rent both give you directions, not a teleport -- you still have to walk it yourself.'
  UNION ALL SELECT 'rent is the safe way to log off: your belongings stay with you and you heal while away. quit! drops everything you carry.'
  UNION ALL SELECT 'Resting heals faster than standing around; sleeping heals fastest of all, but you see nothing while asleep.'
  UNION ALL SELECT 'kill 2.mob (or look 2.board, get 2.sword, ...) reaches the second match when more than one thing shares a name.'
  UNION ALL SELECT 'affects shows any buff, debuff, disease, or poison currently active on you, and how long it has left.'
  UNION ALL SELECT 'Set a wimpy level and you will flee automatically once your health drops below it -- the cheapest life insurance there is.'
  UNION ALL SELECT 'limbs shows the condition of each part of your body; a badly hurt limb costs you in a fight until it is treated.'
  UNION ALL SELECT 'who -l lists everyone by level, which makes it easy to find someone close enough to yours to group with.'
  UNION ALL SELECT 'help rules is worth reading once -- it covers what is and is not allowed here, and ignorance is not much of a defence.'  UNION ALL SELECT 'Stuck on something these tips do not cover? Type newbie <message> to ask on the newbie help channel -- experienced players and staff watch it.'
  UNION ALL SELECT 'New gear does nothing sitting in your pack. wear all puts on everything you can equip at once; wear <item> handles just one piece.'
  UNION ALL SELECT 'equipment -- or just eq -- shows everything you are wearing and wielding, and what condition each piece is in.'
  UNION ALL SELECT 'inventory (inv for short) lists what you are carrying but have not yet put on.'
  UNION ALL SELECT 'remove <item> takes a piece of worn gear back off -- you often need to before you can wear something new in that slot.'
  UNION ALL SELECT 'say <message> speaks aloud to everyone in your room; tell <name> <message> reaches someone anywhere in the world.'
  UNION ALL SELECT 'Etiquette: if another player reached a room before you, the mobs there are theirs to pull first -- do not jump their fight.'
  UNION ALL SELECT 'Not sure where to train a discipline? goto <discipline> gives directions -- for example goto combat walks you toward the combat trainer.'
  UNION ALL SELECT 'goto surplus points you to a heap of cast-off equipment -- always worth a rummage for a lucky find.'
  UNION ALL SELECT 'Seen enough of these? toggle tips turns them off. To read one on demand any time, just type tips.'
) t
WHERE NOT EXISTS (SELECT 1 FROM `tip` WHERE `tip`.`body` = t.body);
