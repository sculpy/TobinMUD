-- Newbie equipment system expansion (TODO.md priority item, 2026-08-02,
-- user-supplied vnum ranges): six per-RACE starting-gear suits, granted
-- alongside (not instead of) the existing per-CLASS suits (suit.sql).
-- Each race already has a full 11-piece cloth armor set + its own
-- racial weapon + the shared training shield pre-seeded at vnums
-- 36930-37002 -- this just wires them into the suit system so
-- `suit_grant()` (suit.c) actually hands them out, the same mechanism
-- the class suits already use.
--
-- Vnum ranges (user, 2026-08-02):
--   human:  36930-36940 (armor) + 36996 (sword)      + 37002 (shield)
--   ogre:   36941-36951 (armor) + 37001 (club)        + 37002 (shield)
--   dwarf:  36952-36962 (armor) + 36997 (hand axe)    + 37002 (shield)
--   gnome:  36963-36973 (armor) + 36999 (dagger)      + 37002 (shield)
--   elf:    36974-36984 (armor) + 36998 (longknife)   + 37002 (shield)
--            (36974-37984 as originally given was a typo -- 36974-36984
--             is the only range that doesn't overlap gnome/hobbit and
--             matches every other race's own "11 armor pieces" pattern)
--   hobbit: 36985-36995 (armor) + 37000 (walking stick)+ 37002 (shield)
--
-- Race values match player_race_t (being.h): HUMAN=0, ELF=1, OGRE=2,
-- DWARF=3, HOBBIT=4, GNOME=5.
INSERT INTO `suit` (name, race, description) VALUES
('human_race', 0, 'Starting armor and weapon for a Human'),
('elf_race', 1, 'Starting armor and weapon for an Elf'),
('ogre_race', 2, 'Starting armor and weapon for an Ogre'),
('dwarf_race', 3, 'Starting armor and weapon for a Dwarf'),
('hobbit_race', 4, 'Starting armor and weapon for a Hobbit'),
('gnome_race', 5, 'Starting armor and weapon for a Gnome')
ON DUPLICATE KEY UPDATE `description` = VALUES(`description`);

INSERT INTO `suit_item` (suit_id, obj_vnum, quantity)
SELECT s.id, v.vnum, 1 FROM `suit` s
JOIN (
  SELECT 'human_race' AS suit_name, vnum FROM `obj` WHERE vnum BETWEEN 36930 AND 36940
  UNION ALL SELECT 'human_race', 36996
  UNION ALL SELECT 'human_race', 37002
  UNION ALL SELECT 'ogre_race', vnum FROM `obj` WHERE vnum BETWEEN 36941 AND 36951
  UNION ALL SELECT 'ogre_race', 37001
  UNION ALL SELECT 'ogre_race', 37002
  UNION ALL SELECT 'dwarf_race', vnum FROM `obj` WHERE vnum BETWEEN 36952 AND 36962
  UNION ALL SELECT 'dwarf_race', 36997
  UNION ALL SELECT 'dwarf_race', 37002
  UNION ALL SELECT 'gnome_race', vnum FROM `obj` WHERE vnum BETWEEN 36963 AND 36973
  UNION ALL SELECT 'gnome_race', 36999
  UNION ALL SELECT 'gnome_race', 37002
  UNION ALL SELECT 'elf_race', vnum FROM `obj` WHERE vnum BETWEEN 36974 AND 36984
  UNION ALL SELECT 'elf_race', 36998
  UNION ALL SELECT 'elf_race', 37002
  UNION ALL SELECT 'hobbit_race', vnum FROM `obj` WHERE vnum BETWEEN 36985 AND 36995
  UNION ALL SELECT 'hobbit_race', 37000
  UNION ALL SELECT 'hobbit_race', 37002
) v ON v.suit_name = s.name
ON DUPLICATE KEY UPDATE `quantity` = VALUES(`quantity`);

-- Rations + a small drink container for EVERY class suit (user,
-- 2026-08-02: "load newbies with a few rations of food and a small
-- drink container"). Reused real seeded items: 403 "ration standard
-- food" (val0=18 hunger units) x3, 410 "waterskin skin regular" (a
-- small water-filled skin, val0=val1=70, val2=0/water) x1. Uses the
-- new per-item `quantity` column (Menu-driven loadsuit editor,
-- 2026-08-02) rather than 3 separate identical rows.
INSERT INTO `suit_item` (suit_id, obj_vnum, quantity)
SELECT s.id, v.vnum, v.qty FROM `suit` s
JOIN (
  SELECT 'mage_newbie' AS suit_name, 403 AS vnum, 3 AS qty UNION ALL
  SELECT 'mage_newbie', 410, 1 UNION ALL
  SELECT 'cleric_newbie', 403, 3 UNION ALL
  SELECT 'cleric_newbie', 410, 1 UNION ALL
  SELECT 'warrior_newbie', 403, 3 UNION ALL
  SELECT 'warrior_newbie', 410, 1 UNION ALL
  SELECT 'thief_newbie', 403, 3 UNION ALL
  SELECT 'thief_newbie', 410, 1 UNION ALL
  SELECT 'druid_newbie', 403, 3 UNION ALL
  SELECT 'druid_newbie', 410, 1 UNION ALL
  SELECT 'monk_newbie', 403, 3 UNION ALL
  SELECT 'monk_newbie', 410, 1
) v ON v.suit_name = s.name
ON DUPLICATE KEY UPDATE `quantity` = VALUES(`quantity`);

-- Mage/Druid: a small spellpouch (321, "spellbag small") plus three
-- spell components (user, 2026-08-02: "mages and druids... small
-- spellpouch containing appropriate spell components to get to level
-- 5"). Reused real seeded mage-component items (200/201/202, each
-- val0=10 charges -- consume_component()/cmd_cast.c only checks for
-- the keyword "component" in ANY carried item, not a class-specific
-- match, so the same three work for both classes despite their
-- "component mage" name tag -- verified against cmd_cast.c's
-- find_keyword_item(ch, "component") before reusing them here). Not
-- physically nested inside the pouch -- suit_grant() (suit.c) only
-- ever creates items loose in inventory, same "they can arrange it
-- themselves" precedent the whole suit system already uses.
INSERT INTO `suit_item` (suit_id, obj_vnum, quantity)
SELECT s.id, v.vnum, v.qty FROM `suit` s
JOIN (
  SELECT 'mage_newbie' AS suit_name, 321 AS vnum, 1 AS qty UNION ALL
  SELECT 'mage_newbie', 200, 1 UNION ALL
  SELECT 'mage_newbie', 201, 1 UNION ALL
  SELECT 'mage_newbie', 202, 1 UNION ALL
  SELECT 'druid_newbie', 321, 1 UNION ALL
  SELECT 'druid_newbie', 200, 1 UNION ALL
  SELECT 'druid_newbie', 201, 1 UNION ALL
  SELECT 'druid_newbie', 202, 1
) v ON v.suit_name = s.name
ON DUPLICATE KEY UPDATE `quantity` = VALUES(`quantity`);

-- Cleric: a few wooden holy symbols (user, 2026-08-02). Reused real
-- seeded item 500 "symbol holy wooden" (val0=10 -- consume_symbol()
-- degrades it by a random 1-2 strength per prayer, cmd_pray.c/
-- cmd_continue.c), granted at quantity 3 via the new per-item
-- quantity column rather than three separate rows.
INSERT INTO `suit_item` (suit_id, obj_vnum, quantity)
SELECT s.id, 500, 3 FROM `suit` s WHERE s.name = 'cleric_newbie'
ON DUPLICATE KEY UPDATE `quantity` = VALUES(`quantity`);

-- Two-slot limb items only granted ONE apiece above -- a sleeve (ARMS),
-- bracelet (WRISTS), glove (HANDS), legging (LEGS), boot/shoe/slipper
-- (FEET), or ring (FINGERS) covers only ONE of the pair (LIMB_RIGHT_ARM/
-- LIMB_LEFT_ARM, etc, being.h), leaving the other arm/wrist/hand/leg/
-- foot/finger bare (user, 2026-08-03: "include two of the limbs that
-- require two eq slots like arms hands wrists legs feet etc"). Fixed by
-- bumping quantity to 2 for exactly those rows -- wear_slot_for_flag()
-- (obj.c) already fills the SECOND matching limb (prefers right, then
-- left) once a second item with the same wear_flag exists to wear;
-- nothing else needed. Every race armor set (36930-36995) uses the same
-- 11-piece pattern, so one bitwise match against wear_flag covers all
-- six races at once: WEAR_FINGERS(2)|WEAR_LEGS(32)|WEAR_FEET(64)|
-- WEAR_HANDS(128)|WEAR_ARMS(256)|WEAR_WRISTS(4096) (src/core/obj.c).
UPDATE `suit_item` si
JOIN `obj` o ON o.vnum = si.obj_vnum
SET si.quantity = 2
WHERE o.vnum BETWEEN 36930 AND 36995
  AND (o.wear_flag & (2 | 32 | 64 | 128 | 256 | 4096)) != 0;
