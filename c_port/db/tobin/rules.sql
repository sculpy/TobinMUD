-- Game rules: numbered, player-facing conduct rules shown by the `rules`
-- command (cmd_rules.c / rules_repo.c) and edited by 59+ via `edrules`
-- (cmd_edrules.c). Each rule has a stable number the player references
-- ("rules 1"). Tobin-specific. CREATE ... IF NOT EXISTS keeps re-runs safe;
-- the seed rows use ON DUPLICATE KEY UPDATE num=num so they never clobber
-- in-game edits.

CREATE TABLE IF NOT EXISTS `rules` (
  `num` int NOT NULL,
  `title` varchar(120) NOT NULL DEFAULT '',
  `body` text NOT NULL,
  `updated_by` varchar(64) NOT NULL DEFAULT '',
  `updated_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`num`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT INTO `rules` (`num`, `title`, `body`, `updated_by`) VALUES
(1, 'Respect Other Players',
 'Treat everyone with courtesy. Harassment, slurs, and deliberate cruelty have no place here. Disagreements happen -- keep them civil.', 'seed'),
(2, 'No Cheating or Exploits',
 'Do not exploit bugs, duplicate items, or use third-party automation to gain an unfair advantage. Found a bug? Report it with the bug command.', 'seed'),
(3, 'One Character In Play',
 'Mortals play one character at a time. Multiplaying is not allowed unless an immortal has enabled it for an event.', 'seed')
ON DUPLICATE KEY UPDATE `num` = `num`;
