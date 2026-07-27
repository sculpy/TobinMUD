-- New training-tier weapons for the newbie-suit feature (user 2026-07-26:
-- "6 sets of newbie equipment... include a shield and a weapon based
-- upon class choice"). Tobin-specific, not upstream seed.
--
-- Three of the six classes already have a perfect existing "training"-
-- tier weapon in the real upstream data (same 327-price/12-max_struct
-- profile): staff wooden training (177, Mage), dagger training small
-- (325, Thief), sword wooden small training (329, Warrior) -- reused
-- directly in suit.sql rather than reinventing them. No training-tier
-- mace, sickle, or nunchaku exists anywhere in the seed data (checked
-- live against the real `obj` table first), so the three missing ones
-- are added here, at the SAME stat profile as the three reused ones,
-- so all six suits feel equivalent in quality.
--
-- New vnums 90003-90005 -- reclaiming the gap between the two existing
-- Tobin-owned blocks (obj_magic.sql's 90000-90002, drug_items.sql's
-- 90010-90013) rather than opening a new range.
INSERT INTO `obj` (vnum,name,short_desc,long_desc,type,wear_flag,val0,val1,val2,val3,weight,volume,price,can_be_seen,max_struct,cur_struct,material,decay,max_exist) VALUES
(90003,'mace wooden training','a wooden training mace','A wooden training mace lies here.',5,16385,3084,2304,0,0,4,3,327,1,12,12,5,-1,9999),
(90004,'sickle wooden training','a wooden training sickle','A wooden training sickle lies here.',5,16385,3084,2304,0,0,3,3,327,1,12,12,5,-1,9999),
(90005,'nunchaku wooden training','a pair of wooden training nunchaku','A pair of wooden training nunchaku lies here.',5,16385,3084,2304,0,0,2,3,327,1,12,12,5,-1,9999)
ON DUPLICATE KEY UPDATE `vnum` = `vnum`;
