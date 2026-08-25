# Tobin — TODO

Last updated: 2026-08-24 (Session 199). Companion to STATUS.md, which holds the full
session log, decisions, and history — **this file tracks only what's NEXT.**
Completed items are pruned from here as they land (find them in STATUS.md).

All in-game editors are menu-driven, like character creation — see the
[[editors-menu-driven]] memory. The user provides a wireframe for each.
Editor commands are unified under **`edit <noun> [args]`** (user
2026-07-11, superseding the old separate `ed<noun>` verbs from
2026-07-05): `edit room` (rooms), `edit zone` (zones), `edit help`
(help), `edit news` (news), `edit wiznews` (wiznews), `edit player`
(players), `edit shop` / `sedit` (shops, 58+, Session 198). Read-only
viewers keep plain names (`news`, `wiznews`).


## Open follow-ups
sell-all/component category bug FIXED Session 199: OBJ_CAT_COMPONENT
split out of OBJ_CAT_OTHER (obj.h/obj.c) so a commodity shop's `sell
all` no longer sweeps up carried spell/prayer components (they used to
share a category with raw materials/organics). See STATUS.md.

smoke_test_component_charges.py FAILING (found Session 199, unrelated
to the fix above -- neither the test nor spell_component.c's charge
logic reference category/OBJ_CAT_* at all): expects a component to
survive exactly 10 mortal casts before being used up, got None instead.
Needs investigation.
Fixed 3 stale/broken smoke tests along the way (Session 190, see
STATUS.md): smoke_test_animal_no_gold.py (missing loot step),
smoke_test_corpse.py (stale load-obj-drops-on-floor assumption),
smoke_test_autoloot.py (stale message-text assertion). None were
caused by anything recent -- just never caught since the suite runs
each file individually. (Session 190 also suspected a get-all-
container partial-sweep bug here; Session 195 investigated directly
and could not reproduce it -- see STATUS.md.)
Mob wealth items (Session 189): investigated "moneypouches/
commodities not loading on mobs." Moneypouches: NOT a bug -- a
2026-07-28 decision (see combat.c) deliberately replaced
SneezyMUD's pre-spawn pickpocket-able pouch with gold going into
the corpse as a lootable coin pile at death instead; user
confirmed keep it that way. Did fix a small real bug found along
the way: obj vnum 604 ("moneypouch pouch", the exact vnum
SneezyMUD hardcodes as Obj::GENERIC_MONEYPOUCH) was seeded with
type=27 (BAG) instead of type=75 (MONEYPOUCH) -- fixed via
db/tobin/fix_obj604_moneypouch_type.sql.
**Commodities -- SHIPPED 2026-08-24.** Mob loot now sometimes skims
part of a mob's corpse_gold (combat.c combat_defeat()) and spends it on
the priciest already-seeded commodity prototype (obj type 42/43/50)
that skim can afford, dropping it into the corpse alongside the coin
pile -- new commodity.c/commodity.h, cached at boot. Deliberately does
NOT port SneezyMUD's live demand-curve pricing (`commodLoader`/
`TCommodity::demandCurvePrice`, ~200 materials) -- reuses the already-
seeded fixed price column on those 182 prototypes and the existing
5-tier material value system instead, same simplification precedent as
material.h. Verified with tests/smoke_test_commodity_loot.py.
Shoptype coverage EXTENDED 2026-08-24: 12 more shops added, chosen by
theme (forges/smithies now buy 42, jewelers/curio now buy 43, a
tannery/alchemists now buy 50) -- see
db/tobin/commodity_shoptype_expansion.sql. Still not done: many shops
have zero shoptype rows at all.
Commodity-trader mob DECLINED 2026-08-24: read upstream's
spec_mobs_commod_trader.cc directly -- it opens with a hard-coded
`return FALSE;` right after its local variable declarations, before
any pulse/trading logic runs, so it is dead code in SneezyMUD itself.
The logic that does exist below that line calls straight into
TCommodity::sellPrice/shopPrice/buyMe/sellMe -- the live per-material
demand-curve pricing engine Session 197 already decided not to port.
Porting a disabled upstream feature onto infrastructure that was
deliberately left unported isn't worth doing; not building it.
Road-shrink initiative: fully reverted world-wide (Session 197). All 14 shrunk zones (11, 2, 67, 16, 53, 22, 258, 18, 12, 49, 19, 259, 38, 146) are back to their pre-shrink room/exit counts; road_shrink.py and every db/tobin/road_shrink_zone*.sql migration are removed. See STATUS.md Sessions 183-188 (the original shrink) and 192/194/197 (the revert) for the full history if this is ever attempted again -- differently, per the user.
## Unimplemented skills/spells backlog (audited Session 158)

Grep-verified against `SKILLS[]` (skill.c) vs real handlers in cmd_cast.c /
cmd_pray.c / combat.c / cmd_*.c. Each entry below has ZERO real-handler refs
(falls through to the "nothing happens yet" placeholder, or a passive with no
wiring). Help-body "does nothing yet" text is STALE/unreliable — the code is
authoritative. Work top-down within a class; each ships with a smoke test.

**Druid spells:** all cleared (sunscald/feral wrath/wave crash/withering
touch/tree walk/leeching vine done 2026-08-18; cure blindness + word of
recall dropped — Cleric prayers).

**Warrior skills:** all cleared 2026-08-22 (advanced berserking done).

**Thief skills:** all cleared 2026-08-22 (spy, cudgel done).

**Monk skills:** all cleared 2026-08-18 (iron flesh/skin/bones/muscles/will + defenestrate + bonebreak; iron fist/iron legs were already wired).

**Common skills (all 6 classes):** all cleared 2026-08-22 (ranged
proficiency/specialization, set trap arrow).
**Spot-check flags:** `cure blindness` / `word of recall` are cleric only skills.
`two-handed specialization` flag is STALE (checked 2026-08-22): it's fully wired
in combat.c (2026-08-18, damage bonus while wielding a WEAR_PAIRED weapon,
same passive learn-by-doing shape as the other Warrior specializations) and
has real two-handed weapons to trigger on since the 2026-08-21 seed.

## Standing rules (learned)

- No new full sweeps. Full sweep takes 5+ hours to complete. Targeted testing only.
- Every player-facing change gets a `news.sql` entry (no numbers). See CLAUDE.md.
- Every new `db/tobin/*.sql` file MUST use `CREATE TABLE IF NOT EXISTS`,
  never an unconditional `DROP TABLE IF EXISTS` + `CREATE TABLE` — the
  latter silently wipes live data every time `apply-tobin-schema.sh` re-runs
  it (which it always does; that script re-applies every file, every time).
  Burned us once for real (Session 36: `player_attrs.sql`/
  `player_progress.sql` wiped ~1338 players' progress this way).
- Always use SneezyMUD code as implementation guidance where available.
- Complete the current task fully before moving to the next.
- Continue progressing rapidly through the backlog without waiting for additional instructions unless blocked by missing requirements.
- Document all completed changes, database updates, VNUM allocations, and implementation notes.
- Help me conserve tokens. No narration, just complete silence. Be absolutely quiet.
- As each item gets built and tested clean, remove that entry in TODO.md. Keep TODO.md and STATUS.md as trimmed as possible.
