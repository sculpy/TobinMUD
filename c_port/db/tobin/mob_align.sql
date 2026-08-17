-- mob_align.sql -- seed cosmic good/evil alignment onto mob prototypes.
--
-- Background: every one of the 5685 mob protos shipped with align=0, so the
-- alignment-driven branches of mob combat/AI were all inert:
--   * combat.c combat_recruit_assist(): aligned allies (same nonzero
--     mob_align) join a fight the player picks against one of them.
--   * mob_ai.c mob_try_aggress()/mob_try_align_flavor(): an ALIGNED
--     aggressive mob fights only the OPPOSITE alignment and taunts/supports
--     neutral PCs instead of attacking them (user design, 2026-07-11).
--
-- The runtime only ever reads the SIGN of mob_align (good vs evil) and tests
-- two mobs for EXACT EQUALITY (assist banding). Magnitude is otherwise
-- unused, so one shared value per side maximizes banding: every evil mob in
-- a room assists every other evil mob, likewise good. Values are small
-- (the column is tinyint): -100 evil, +100 good.
--
-- SCOPE (deliberately conservative -- disclosed): only the cosmically
-- aligned supernatural races are seeded. Mortal humanoid raiders (orc,
-- goblin, troll, drow, gnoll, kobold, hobgoblin, ratmen, ...) and all
-- wildlife are LEFT NEUTRAL (align=0) on purpose: an aligned aggressive mob
-- stops attacking the neutral majority of players (by design above), so
-- marking every goblin "evil" would gut low-level danger. Keeping them at 0
-- preserves their attack-everyone aggression. Faithful to Diku/Sneezy
-- convention that undead/fiends are evil and celestials are good, without a
-- broad aggression change.
--
-- Idempotent: pure UPDATEs by race, safe to re-run every apply-tobin-schema.
-- Race numbers are Sneezy race_t (sneezymud-master/code/code/misc/race.h).

-- Evil: undead(10), demon(21), devil(28), mind flayer(33), banshee(38),
-- vampire(49), vampire bat(52), lycanthrope(8).
UPDATE mob SET align = -100
 WHERE race IN (8, 10, 21, 28, 33, 38, 49, 52);

-- Good: pegasus(7), angel(32), shedu(78), lammasu(79), phoenix(81),
-- coatl(122).
UPDATE mob SET align = 100
 WHERE race IN (7, 32, 78, 79, 81, 122);
