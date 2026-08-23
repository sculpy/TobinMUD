-- Data fix: obj vnum 604 ("moneypouch pouch") is the exact vnum SneezyMUD's
-- source hardcodes as Obj::GENERIC_MONEYPOUCH (misc/low.h). It was seeded
-- into Tobin's DB with type=27 (ITEM_BAG) instead of type=75
-- (ITEM_MONEYPOUCH) -- a data-import mismatch, not a deliberate
-- reclassification (no STATUS.md decision documents this). Harmless today
-- (nothing currently spawns type-75 objects), but wrong. Idempotent: only
-- touches the row if it still has the stale type.
UPDATE obj SET type = 75 WHERE vnum = 604 AND type = 27;
