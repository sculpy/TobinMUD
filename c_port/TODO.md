# Tobin — TODO

Last updated: 2026-08-17. Companion to STATUS.md, which holds the full
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

- **Sneezy-DB decoupling — optional cleanups** (main task DONE 2026-08-16;
  orphaned `sneezy` + `sneezy_scratch` DBs dropped 2026-08-17;
  `c_port/db/fix-workbox.sh` deleted 2026-08-17): (1) the `c_port/db/seed`
  snapshot is a Jul-27 baseline, ~250-900 world rows behind live — refresh
  via `sneezymud-master/db/update-seed-data.sh` if a current-world
  from-scratch build is ever wanted. (2) `sneezymud-master/db/init-db.sh` is
  superseded by `c_port/db/init-db.sh` but is still named as the DB-seed step
  in 4 docs (README.md, doc/systems/README.md, ENVIRONMENT.md, CLAUDE.md) —
  update those to the new script before deleting it.

- **Seed WEAR_PAIRED onto in-world items** (mechanic DONE 2026-08-18, see
  smoke_test_wear_paired.py): no seeded weapon/armor carries the flag yet, so
  players won't meet a two-handed weapon until greatswords/mauls/halberds/
  claymores/etc. (and any both-limb armor) get bit 9 (512) set on their
  wear_flag. A keyword-driven data pass, with a balance eye (making a weapon
  two-handed frees/consumes the off-hand).

## Unimplemented skills/spells backlog (audited Session 158)

Grep-verified against `SKILLS[]` (skill.c) vs real handlers in cmd_cast.c /
cmd_pray.c / combat.c / cmd_*.c. Each entry below has ZERO real-handler refs
(falls through to the "nothing happens yet" placeholder, or a passive with no
wiring). Help-body "does nothing yet" text is STALE/unreliable — the code is
authoritative. Work top-down within a class; each ships with a smoke test.

**Druid spells (cast):** sunscald (16), feral wrath (28), wave crash (32),
withering touch (32), tree walk (41), leeching vine (48). (cure blindness +
word of recall dropped from the Druid roster 2026-08-18 — they are Cleric
prayers.)

**Cleric spell (pray):** relive (49, corpse resurrection).

**Warrior skills:** fortify (1), doorbash (1), advanced berserking (35).

**Thief skills:** search (1, hidden exits/items), dodge (1, passive avoidance),
poison weapon (25), skulk (25), track (25), concealment (30, passive), spy (38),
cudgel (41).

**Monk skills (defensive "iron" family + two actives):** iron flesh (31), iron
skin (35), iron bones (38), defenestrate (42), iron muscles (42), iron will
(48), bonebreak (50). [iron fist (25) + iron legs (45) already wired.]

**Deferred-by-design (source comments say NOT ported — revisit only if wanted):**
ranged proficiency (25, all 6 classes) + ranged specialization (25, Warrior) —
no ranged-weapon combat-bonus path (though cmd_shoot.c now exists, so revisitable);
set trap arrow (26) / mine (37) / grenade (50) — cmd_trap.c deliberately ports
only container/door traps.

**Spot-check flags:** `cure blindness` / `word of recall` are class-routing gaps
(handled for one class, not the Druid cast path) — confirm intended before
building. `two-handed specialization` is a silent no-op — confirm whether it
should fold into another weapon category.

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
- Help me conserve tokens. No narration, just complete silence. Only exception to this rule is when finished with a task, you can give a brief report. Otherwise, be absolutely quiet.
- As each item gets built and tested clean, remove that entry in TODO.md. Keep TODO.md and STATUS.md as trimmed as possible.
