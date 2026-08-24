# Tobin — TODO

Last updated: 2026-08-24 (18). Companion to STATUS.md, which holds the full
session log, decisions, and history — **this file tracks only what's NEXT.**
Completed items are pruned from here as they land (find them in STATUS.md).

All in-game editors are menu-driven, like character creation — see the
[[editors-menu-driven]] memory. The user provides a wireframe for each.
Editor commands are unified under **`edit <noun> [args]`** (user
2026-07-11, superseding the old separate `ed<noun>` verbs from
2026-07-05): `edit room` (rooms), `edit zone` (zones), `edit help`
(help), `edit news` (news), `edit wiznews` (wiznews), `edit player`
(players); future `edit object`/`edit mob`/`edit account`. Read-only
viewers keep plain names (`news`, `wiznews`).


## Open follow-ups
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
have zero shoptype rows at all; a dedicated commodity-trader mob
(SneezyMUD's spec_mobs_commod_trader.cc) was not ported either.
Road-shrink initiative is CLOSED (Sessions 183-188). Phase A
complete: all 14 built road/connector zones shrunk -- 11, 2, 67,
16, 53, 22, 258, 18, 12, 49, 19, 259, 38, 146. Phase B (global
vnum cascade-renumber) was investigated in Session 188 and
ABANDONED: zone.bottom/top turns out not to reflect where a
zone's rooms actually live for most of the database (138/336
zones drifted, 81% of all rooms outside their own zone's declared
range -- confirmed benign for gameplay, only used by the `dig`
builder command and a manual zonefile-snapshot tool, not by zone
resets). This breaks the cascade-renumber's core premise, so it
was dropped rather than forced. See STATUS.md Session 188 for the
full writeup. A pre-Phase-B DB backup is on the droplet at
~/backups/tobin_pre_phaseB_20260822_194858.sql.gz as a general
safety net (nothing was applied against it -- Session 188 was
read-only investigation).
Known loose ends from Phase A: zone 2 rooms 104 and 167 have no
outgoing exit (legitimate one-way portal targets from zone 106,
not bugs) -- worth a manual redit pass sometime. A handful of
pre-existing zero-exit orphan rooms were noticed while verifying
(34770, 1734, 34034) -- unrelated, not touched, not urgent.
road_shrink.py has a minor rough edge: a zone with 0% cuttable
rooms writes an invalid `IN ()` SQL file instead of skipping --
harmless, just delete the file if it recurs.
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
