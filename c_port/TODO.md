# Tobin — TODO

Last updated: 2026-08-21. Companion to STATUS.md, which holds the full
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

- Meditate doesnt use POSITION_MEDITATE, examine sneezy for the proper implementation that will gain HP, Mana/Lifeforce, and vitality.

- I wince as my wounds bleed but nothing reports a bleeding limb.

- Rework MSP into a toggle so players can turn sound on/off.

- Gametog and toggles should save over reboots.

- 
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
