-- Global game settings that persist across reboots/copyovers -- simple
-- name/value rows. Tobin-specific.

CREATE TABLE IF NOT EXISTS `game_config` (
  `name` varchar(32) NOT NULL,
  `value` varchar(64) NOT NULL DEFAULT '',
  PRIMARY KEY (`name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Multiplay: whether mortals may run more than one character at once.
-- Default off (mortals get one connected character; immortals are exempt).
INSERT INTO `game_config` (`name`, `value`) VALUES ('multiplay', 'off')
ON DUPLICATE KEY UPDATE `name` = `name`;
