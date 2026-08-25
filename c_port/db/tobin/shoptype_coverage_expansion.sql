-- Audit of the 119 shops with zero shoptype rows (TODO.md, Session 199
-- follow-up). Cross-checked each against shopproducing (what it actually
-- stocks) and shopowned (corp_id) -- only 6 of the 119 stock anything at
-- all via shopproducing; the other 113 are genuine service NPCs (banks,
-- postmasters, tax collectors, innkeepers, doctors/medics, receptionists,
-- repairmen/sharpeners/barbers, butlers) or player/corp-market stalls
-- (shop_nr 85-103, corp_id 21, a separate marketplace mechanic) that
-- correctly buy nothing from players -- left untouched.
--
-- The 6 real general stores, matched to the raw obj.type values they
-- stock in shopproducing:
--   109 A Fishing Shop (Keirk) -- poles/waders/hats/bait/pails: WORN
--       (11), TRASH (13, upstream fishing-pole seed quirk, not touched
--       here), BAG (27), TOOL (35).
--   218-221, 224 (Trolley zone bazaar: Brotherhood of the Knot, Cult of
--       Tailoring, Gnucci, Furtive Florimel's) -- race-themed cloth/worn
--       gear and a few starter weapons: WORN (11) on all five, WEAPON (5)
--       added where the shop's own stock includes one (219/221/224).
-- Idempotent: safe to re-run.
INSERT INTO shoptype (shop_nr, type) VALUES
  (109, 11), (109, 13), (109, 27), (109, 35),
  (218, 11),
  (219, 5), (219, 11),
  (220, 11),
  (221, 5), (221, 11),
  (224, 5)
ON DUPLICATE KEY UPDATE shop_nr = shop_nr;
