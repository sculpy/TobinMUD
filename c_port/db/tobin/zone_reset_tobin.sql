-- Tobin-added zone_reset rows, kept SEPARATE from zone_reset.sql (that
-- file is machine-generated from the upstream zonefiles -- see its own
-- "Do not edit by hand" header -- and does an unconditional `DELETE FROM
-- zone_reset` before re-inserting, which would silently wipe any custom
-- row added directly to it). Sorts after zone_reset.sql under
-- apply-tobin-schema.sh's C-locale filename sort ('.' < '_'), so this
-- always re-applies AFTER that wipe-and-reload, not before it.
--
-- cmd_no values here start at 9000 (zone 2 "Tobin City Roads" has zero
-- generated reset rows today, but a future zonefile regen could add
-- real low cmd_no rows for it -- a high, clearly Tobin-reserved range
-- avoids any future collision).

-- Combat trainer mob (vnum 239, "trainer combat fencing expert instructor" --
-- the expert fencer) on Perimeter Road near town (user, 2026-08-03: "make
-- the combat trainer load somewhere on perimeter road"). Room 111 is a
-- real seeded Perimeter Road room whose own description already says it
-- "meets the East King's Road... along the outer edge of the town" --
-- genuinely close to town, not just picked at random.
INSERT INTO `zone_reset` (`zone_nr`,`cmd_no`,`command`,`if_flag`,`arg1`,`arg2`,`arg3`,`arg4`,`comment`)
VALUES (2, 9000, 'M', 0, 239, 1, 111, 0, 'Combat trainer, Perimeter Road near town')
ON DUPLICATE KEY UPDATE `arg1` = VALUES(`arg1`), `arg3` = VALUES(`arg3`);

-- Dragon-keeper flavor mobs for the new dragon roost rooms (dragon_ride.sql,
-- cmd_fly.c). Zone 63 ("Permanent General Purpose") had zero zone_reset
-- rows before this, so cmd_no starts at 1.
INSERT INTO `zone_reset` (`zone_nr`,`cmd_no`,`command`,`if_flag`,`arg1`,`arg2`,`arg3`,`arg4`,`comment`)
VALUES (63, 1, 'M', 0, 7900, 1, 7900, 0, 'Dragon-keeper Keirath, roost above Market Square')
ON DUPLICATE KEY UPDATE `arg1` = VALUES(`arg1`), `arg3` = VALUES(`arg3`);
INSERT INTO `zone_reset` (`zone_nr`,`cmd_no`,`command`,`if_flag`,`arg1`,`arg2`,`arg3`,`arg4`,`comment`)
VALUES (63, 2, 'M', 0, 7901, 1, 7901, 0, 'Dragon-keeper Sorha, roost above the Araxus walls')
ON DUPLICATE KEY UPDATE `arg1` = VALUES(`arg1`), `arg3` = VALUES(`arg3`);

-- Dragon-keeper flavor mobs for the 2026-08-25 destination expansion
-- (dragon_ride_destinations.sql, cmd_travel.c). Continuing zone 63's
-- cmd_no sequence from the original two roosts above.
INSERT INTO `zone_reset` (`zone_nr`,`cmd_no`,`command`,`if_flag`,`arg1`,`arg2`,`arg3`,`arg4`,`comment`)
VALUES (63, 3, 'M', 0, 7907, 1, 7907, 0, 'Dragon-keeper Wenna, roost above Amber Castle')
ON DUPLICATE KEY UPDATE `arg1` = VALUES(`arg1`), `arg3` = VALUES(`arg3`);
INSERT INTO `zone_reset` (`zone_nr`,`cmd_no`,`command`,`if_flag`,`arg1`,`arg2`,`arg3`,`arg4`,`comment`)
VALUES (63, 4, 'M', 0, 7908, 1, 7908, 0, 'Dragon-keeper Torvald, roost above Logrus')
ON DUPLICATE KEY UPDATE `arg1` = VALUES(`arg1`), `arg3` = VALUES(`arg3`);
INSERT INTO `zone_reset` (`zone_nr`,`cmd_no`,`command`,`if_flag`,`arg1`,`arg2`,`arg3`,`arg4`,`comment`)
VALUES (63, 5, 'M', 0, 7909, 1, 7909, 0, 'Dragon-keeper Marisol, roost above the Xanesla coast')
ON DUPLICATE KEY UPDATE `arg1` = VALUES(`arg1`), `arg3` = VALUES(`arg3`);
INSERT INTO `zone_reset` (`zone_nr`,`cmd_no`,`command`,`if_flag`,`arg1`,`arg2`,`arg3`,`arg4`,`comment`)
VALUES (63, 6, 'M', 0, 7910, 1, 7910, 0, 'Dragon-keeper Ghurn, roost above the Obsidian Citadel')
ON DUPLICATE KEY UPDATE `arg1` = VALUES(`arg1`), `arg3` = VALUES(`arg3`);
