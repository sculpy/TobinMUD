# Tobin — TODO

Last updated: 2026-08-21 (4). Companion to STATUS.md, which holds the full
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

- **Sneezy-DB decoupling — optional cleanup** (main task DONE 2026-08-16;
  orphaned `sneezy` + `sneezy_scratch` DBs dropped 2026-08-17;
  `c_port/db/fix-workbox.sh` deleted 2026-08-17; stale
  `sneezymud-master/db/init-db.sh` references fixed in all 4 docs and the
  script itself deleted, 2026-08-21): the `c_port/db/seed` snapshot is a
  Jul-27 baseline, ~250-900 world rows behind live — refresh via
  `sneezymud-master/db/update-seed-data.sh` if a current-world
  from-scratch build is ever wanted. Purely optional, no rush.

- **Seed WEAR_PAIRED onto in-world armor** (mechanic DONE 2026-08-18, see
  smoke_test_wear_paired.py; two-handed WEAPONS seeded 2026-08-21 -- 55 real
  vnums bit-512'd via a keyword pass: battle axes/claymores/flamberges/
  greatswords/greataxes/halberds/mauls/pikes/two-handed swords & spears/
  warhammers/zweihander/naginata/quarterstaffs/scythes/longbows; live-
  verified with a real seeded vnum + smoke_test_wear_paired.py). Still open:
  both-limb ARMOR (leggings/gauntlets/boots/vambraces/etc.) -- deliberately
  NOT swept this round: unlike weapons (bounded list, unambiguous by type),
  armor spans a much larger and fuzzier set (thousands of leg/hand/foot/arm
  rows) where "should this one be paired" is a real per-item balance call,
  not a keyword match. Needs its own scoped pass.

- **Client: mapping support.** Enable GMCP/MXP so the server feeds clean
  room + coordinate data to a mapping interface; build that interface in
  the client; add a client-side toggle to enable/disable mapping; store
  maps in a rereadable file so players can save them across sessions.

- **Client: enable cut/copy/paste** in the client.

- **Client: launch/update notice.** Show a visible "updating..." status
  when the client is applying an update on launch, so players don't
  assume it failed to start.

- **Pre-existing test bug found 2026-08-21** (unrelated to that session's
  work, not yet fixed): `smoke_test_bleeding.py` fails at "a fresh (size-1)
  blood pool reads as 'a puddle of blood'" -- a freshly-spawned blood pool's
  room description doesn't read as expected. Room object pool-growth text,
  obj.c's `obj_grow_pool()`/pool-size-tier naming -- separate code path from
  the limb-bleeding tick/display fix that session shipped.

## Unimplemented skills/spells backlog (audited Session 158)

Grep-verified against `SKILLS[]` (skill.c) vs real handlers in cmd_cast.c /
cmd_pray.c / combat.c / cmd_*.c. Each entry below has ZERO real-handler refs
(falls through to the "nothing happens yet" placeholder, or a passive with no
wiring). Help-body "does nothing yet" text is STALE/unreliable — the code is
authoritative. Work top-down within a class; each ships with a smoke test.

**Druid spells:** all cleared (sunscald/feral wrath/wave crash/withering
touch/tree walk/leeching vine done 2026-08-18; cure blindness + word of
recall dropped — Cleric prayers).

**Cleric spell (pray):** relive (49, corpse resurrection).

**Warrior skills:** advanced berserking (35).

**Thief skills:** spy (38), cudgel (41).

**Monk skills:** all cleared 2026-08-18 (iron flesh/skin/bones/muscles/will + defenestrate + bonebreak; iron fist/iron legs were already wired).

**Common skills (all 6 classes):**
ranged proficiency (25, all 6 classes) + ranged specialization (35, Warrior) —
no ranged-weapon combat-bonus path (though cmd_shoot.c now exists);
set trap arrow (26) / mine (37) / grenade (50) — cmd_trap.c (only container/door traps currently).

**Spot-check flags:** `cure blindness` / `word of recall` are cleric only skills.
`two-handed specialization` is a silent no-op — it should fold into another category.

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
