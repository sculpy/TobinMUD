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
