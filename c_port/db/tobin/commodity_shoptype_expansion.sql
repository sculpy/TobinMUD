-- Follow-up to Session 197 (commodities loot): TODO.md noted only 6-9
-- shops accept raw type 42 (RAW_MATERIAL), 43 (GEMSTONE), or 50
-- (RAW_ORGANIC), so most dropped commodities had nowhere to sell.
-- Thematically extends coverage to more shops of the matching trade,
-- using existing shopmaster/room names to justify each choice: forges
-- and smithies for raw materials, jewelers and curio shops for
-- gemstones, and a tannery/alchemists for raw organics. No new shops,
-- no schema change -- just shoptype rows. Idempotent: safe to re-run.
INSERT INTO shoptype (shop_nr, type) VALUES
  (133, 42), (134, 42), (138, 42), (175, 42),
  (44, 43), (61, 43), (73, 43), (236, 43),
  (29, 50), (110, 50), (174, 50), (244, 50)
ON DUPLICATE KEY UPDATE shop_nr = shop_nr;
