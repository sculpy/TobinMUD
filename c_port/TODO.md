# Tobin — TODO

Last updated: 2026-08-22 (14). Companion to STATUS.md, which holds the full
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
Road-shrink initiative (Session 183-184): zone 11 (49->26) and zone 2
(413->344) done -- use db/road_shrink.py for remaining zones, it's
fixed after a Session 184 incident (see STATUS.md). Two known
loose ends from zone 2: rooms 104 and 167 have no outgoing exit
(legitimate one-way portal targets from zone 106, not bugs) --
worth a manual redit pass sometime. Next: continue down the
road/connector zone list (12 more built zones) one per session,
same pattern -- see STATUS.md and the plan file (user's local
Claude Code plans dir, not in this repo). Phase B (global vnum
cascade-renumber to close gaps) waits until all 14 zones are done
and soaked.
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
