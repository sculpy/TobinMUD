-- Real, usable example drug items (Sneezy -> Tobin feature audit, "drug
-- tracking") -- Tobin-specific (not upstream seed). New vnums 90010-90013,
-- confirmed free of any real upstream obj/mob/room content, following the
-- same "new vnum range for a brand-new Tobin-owned feature" precedent
-- obj_magic.sql already established at 90000-90002.
--
-- Identified purely by the keyword "drug" (same generic-by-keyword
-- convention as spell components/holy symbols) -- `smoke <item>` reads
-- val0 as the real drug_type_t (drug.h: 0=pipeweed, 1=opium, 2=pot,
-- 3=frogslime), val1/val2 as current/max charges.
INSERT INTO `obj` (vnum,name,short_desc,long_desc,type,wear_flag,val0,val1,val2,val3,weight,volume,price,can_be_seen,max_struct,cur_struct,material,decay,max_exist) VALUES
(90010,'pouch pipeweed drug','a pouch of pipeweed','A pouch of pipeweed sits here, fragrant and inviting.',56,1,0,10,10,0,1,1,2,1,0,0,0,-1,20),
(90011,'ball opium drug','a ball of raw opium','A ball of raw opium lies here.',56,1,1,10,10,0,1,1,5,1,0,0,0,-1,20),
(90012,'bag pot leaves drug','a bag of dried pot leaves','A bag of dried pot leaves is here.',56,1,2,10,10,0,1,1,3,1,0,0,0,-1,20),
(90013,'vial frogslime drug','a vial of frogslime','A vial of frogslime sits here, faintly glowing.',56,1,3,10,10,0,1,1,4,1,0,0,0,-1,20)
ON DUPLICATE KEY UPDATE `vnum` = `vnum`;
