-- Magic items (Sneezy -> Tobin feature audit, "Magic items"): pairs a
-- scroll/wand/staff object VNUM with the Tobin spell it invokes and how
-- many charges it holds. Tobin-specific (not upstream seed) -- checked the
-- real upstream `obj`/`objaffect` data first: raw val[] fields on real
-- scroll/wand/staff rows turned out to be unreliable import noise (a
-- scroll with 25650 "charges", confirmed via `stat`/cmd_identify.c), and
-- none of the real named items ("Staff of Disruption", "Staff of Magii",
-- ...) hint at which spell they'd cast without inventing lore -- so this
-- is a clean, Tobin-authored table instead of trying to reinterpret messy
-- upstream numbers, same reasoning as social.sql's `point` addition.
--
-- Scrolls (raw obj.type=2/ITEM_SCROLL) are always single-use regardless of
-- `max_charges` -- `use` (cmd_use.c) destroys them after one recitation,
-- matching the original's TScroll::reciteMe() always returning
-- DELETE_THIS. Wands (type=3) are player-targeted; staves (type=4) hit
-- every other being in the room (reusing the same real area-effect logic
-- "offensive spell breadth" already built for `cast`/`pray`). Both track
-- current/max charges on the live obj_t's own val[0]/val[1] (obj.c
-- initializes from `max_charges` here at creation), same MAGIC_DEVICE
-- charge convention obj.h already documents -- reset to full on every
-- reconnect, same known limitation player_inventory's flat vnum+slot
-- schema already has for spell-component/holy-symbol charges (no per-
-- instance state persists across a relog yet).
--
-- Three example items seeded below (new vnums, 90000-90002 -- confirmed
-- free of any real upstream obj/mob/room content in that range) so the
-- feature is immediately testable/usable: a wand, a staff, and a scroll,
-- each tied to a real spell already in Tobin's own roster.

CREATE TABLE IF NOT EXISTS `obj_magic` (
  `vnum` int NOT NULL,
  `spell_name` varchar(64) NOT NULL,
  `max_charges` int NOT NULL DEFAULT 0,
  PRIMARY KEY (`vnum`),
  CONSTRAINT `fk_obj_magic_vnum` FOREIGN KEY (`vnum`) REFERENCES `obj` (`vnum`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT INTO `obj` (vnum, name, short_desc, long_desc, type, wear_flag, val0, val1, weight, price, can_be_seen)
VALUES
(90000, 'wand gusts wind', 'a wand of gusts', 'A wand of gusts lies here.', 3, 16385, 0, 0, 1, 200, 1),
(90001, 'staff fireball flame', 'a staff of fireball', 'A staff of fireball leans here.', 4, 16385, 0, 0, 4, 500, 1),
(90002, 'scroll healing minor', 'a scroll of minor healing', 'A scroll of minor healing lies here.', 2, 16385, 0, 0, 0.1, 50, 1)
ON DUPLICATE KEY UPDATE `name` = `name`;

INSERT INTO `obj_magic` (vnum, spell_name, max_charges) VALUES
(90000, 'gust', 5),
(90001, 'fireball', 3),
(90002, 'heal light', 0)
ON DUPLICATE KEY UPDATE `spell_name` = `spell_name`;
