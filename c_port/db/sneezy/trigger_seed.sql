-- Starter trigger content (user, 2026-07-11: "and convert what sneezy has
-- into a starter set of db data for tobin"), demonstrating `edit trigger`
-- with real content rather than synthetic test fixtures. Reinterprets two
-- of SneezyMUD's hardcoded spec procs (code/code/spec/spec_{mobs,objs}.cc)
-- as data-driven Tobin triggers -- not a port (the fixed action vocabulary
-- can't replicate their exact mechanics), just the same flavor:
--
--   insulter (spec_mobs.cc)    -> speech + random triggers on the real
--                                 seeded "dirty refuse hauler" (vnum
--                                 33271): mutters when spoken to nearby,
--                                 occasionally grumbles unprompted.
--   stickerBush (spec_objs.cc) -> a new takeable "tangle of thorny
--                                 brambles" (vnum 1000001) with a `get`
--                                 trigger: a scratch and minor damage,
--                                 exactly like the original's
--                                 CMD_OBJ_MOVEMENT reaction.
--
-- Room-damage-trap procs (blazingroom, BankVault) and portal-gate procs
-- (SecretPortalDoors, dayGateRoom) were deliberately left out of this
-- starter set -- attaching real damage or teleports to an EXISTING,
-- already-traveled room risks disrupting live players/zones, which a
-- lightweight demo shouldn't do. corpseMuncher was also left out: it
-- reacts to corpses specifically, which Tobin's `get`/`wear`/`death`
-- trigger types don't have a matching hook for yet.
--
-- vnum 1000001 is a deliberately new namespace, well clear of both real
-- seeded content (max real vnum ~989310) and the ephemeral 900000-970000
-- range smoke tests use for throwaway SQL-bootstrapped fixtures.

INSERT INTO `obj` (`vnum`, `name`, `short_desc`, `long_desc`, `type`, `wear_flag`, `can_be_seen`)
SELECT 1000001, 'tangle brambles thorny sticker bush',
       'a tangle of thorny brambles',
       'A tangle of thorny brambles lies here, snagged with bits of fur and cloth.',
       13, 1, 1
WHERE NOT EXISTS (SELECT 1 FROM `obj` WHERE `vnum` = 1000001);

INSERT INTO `trigger` (`created_by`, `target_type`, `target_vnum`, `trigger_type`, `match_text`, `chance_pct`, `script`)
SELECT 'seed', 'obj', 1000001, 'get', NULL, 100,
       'echo Ouch! The thorns prick your fingers as you pull it free.\ndamage 2'
WHERE NOT EXISTS (
  SELECT 1 FROM `trigger` WHERE `target_type` = 'obj' AND `target_vnum` = 1000001 AND `trigger_type` = 'get'
);

INSERT INTO `trigger` (`created_by`, `target_type`, `target_vnum`, `trigger_type`, `match_text`, `chance_pct`, `script`)
SELECT 'seed', 'mob', 33271, 'speech', 'hello', 100,
       'echo The hauler mutters something rude under his breath and keeps working.'
WHERE NOT EXISTS (
  SELECT 1 FROM `trigger` WHERE `target_type` = 'mob' AND `target_vnum` = 33271 AND `trigger_type` = 'speech'
    AND `match_text` = 'hello'
);

INSERT INTO `trigger` (`created_by`, `target_type`, `target_vnum`, `trigger_type`, `match_text`, `chance_pct`, `script`)
SELECT 'seed', 'mob', 33271, 'random', NULL, 10,
       'emote grumbles about the state of the streets these days.'
WHERE NOT EXISTS (
  SELECT 1 FROM `trigger` WHERE `target_type` = 'mob' AND `target_vnum` = 33271 AND `trigger_type` = 'random'
);
