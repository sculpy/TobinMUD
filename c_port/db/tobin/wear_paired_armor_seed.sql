-- WEAR_PAIRED both-limb armor seed (TODO.md follow-up to Session 159's
-- WEAR_PAIRED mechanic + the two-handed WEAPONS seed pass). Unlike
-- weapons, armor has no reliable keyword signal for "should this be
-- paired" -- every candidate item across all 4 paired-eligible slots
-- (legs/feet/hands/arms) is named/described singular ("a boot", "a
-- leather boot") even where a real pair would be expected, so a
-- keyword pass would be a coin flip per item, not a real rule.
--
-- Criterion used instead (user-approved, 2026-08-22): material tier.
-- Tobin's own 5-tier material system (material.h/material.c) already
-- scales damage/AC/durability/value by tier; using it to also grant
-- WEAR_PAIRED is a bounded, non-arbitrary rule -- Rare and Legendary
-- armor covers both limbs from one item as a top-tier convenience
-- perk, Common/Fine/Superior gear does not. ~204 items across the 4
-- slots (53 legs, 48 feet, 53 hands, 50 arms) at the time this was
-- written; re-running this file only affects newly-added items that
-- now match (idempotent: the WHERE clause already excludes anything
-- with the bit set).
--
-- wear_flag bits (obj.h): LEGS=32, FEET=64, HANDS=128, ARMS=256,
-- PAIRED=512. Material IDs (material.c's material_tier_for_id()):
-- RARE = 53,67,102,103,106,110,111,112,119,121,162,163,164,165,169,172
-- LEGENDARY = 57,58,59,60,61,62,63,72,104,160,161,170,173,174,177

UPDATE obj
SET wear_flag = wear_flag | 512
WHERE (wear_flag & (32 | 64 | 128 | 256)) <> 0
  AND (wear_flag & 512) = 0
  AND material IN (
    53,67,102,103,106,110,111,112,119,121,162,163,164,165,169,172,
    57,58,59,60,61,62,63,72,104,160,161,170,173,174,177
  );
