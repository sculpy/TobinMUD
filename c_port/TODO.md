# Tobin — TODO

Last updated: 2026-08-22 (12). Companion to STATUS.md, which holds the full
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

- **Client: mapping doesn't actually DRAW a map.** DONE 2026-08-22
  (STATUS.md Session 174, client v0.4.34). Server now sends x/y/z on
  Room.Info and in mapexport's file format; Map > View Map... is a real
  GDI-drawn graph (nodes/lines, pan/zoom, per-z-level, current room
  highlighted) instead of a text list. Known, accepted limitation: a room
  with no known position, or an exit to a different z-level, isn't drawn.

- **Sneezy-DB decoupling — optional cleanup** (main task DONE 2026-08-16;
  orphaned `sneezy` + `sneezy_scratch` DBs dropped 2026-08-17;
  `c_port/db/fix-workbox.sh` deleted 2026-08-17; stale
  `sneezymud-master/db/init-db.sh` references fixed in all 4 docs and the
  script itself deleted, 2026-08-21): the `c_port/db/seed` snapshot is a
  Jul-27 baseline, ~250-900 world rows behind live — refresh via
  `sneezymud-master/db/update-seed-data.sh` if a current-world
  from-scratch build is ever wanted. Purely optional, no rush.

- **Seed WEAR_PAIRED onto in-world armor.** DONE 2026-08-22 (mechanic
  DONE 2026-08-18, see smoke_test_wear_paired.py; two-handed WEAPONS
  seeded 2026-08-21 -- 55 real vnums bit-512'd via a keyword pass:
  battle axes/claymores/flamberges/greatswords/greataxes/halberds/mauls/
  pikes/two-handed swords & spears/warhammers/zweihander/naginata/
  quarterstaffs/scythes/longbows). Both-limb ARMOR (legs/feet/hands/arms,
  204 real vnums) done via db/tobin/wear_paired_armor_seed.sql: material
  tier (Rare+Legendary, user-approved), not a keyword match -- every
  candidate item is named/described singular ("a boot") with no
  textual pairing signal. smoke_test_wear_paired_armor_seed.py.

- **Client: mapping support.** DONE 2026-08-21/22 (STATUS.md Sessions
  165-168). Server: `Room.Info` GMCP sends `{num,name,exits}`, fires on
  every real room display. Client: learns a graph-walked map from
  Room.Info as a player moves; Map > Enable Mapping toggle; saved to
  `map.dat` (exe_dir); Map > View Map... browses it (client/README.md's
  "Mapping" section). `mapexport [filename]` (59+, cmd_mapexport.c)
  dumps the WHOLE `room`/`roomexit` DB tables to a map.dat-format file
  in `map_exports/`. `maprecalc` (60+, cmd_maprecalc.c) BFS-derives x/y/z
  for every room from the roomexit graph and saves them to the `room`
  table's x/y/z columns (world_map_repo.c backs both). Known, accepted
  limitation (not solved further): per-connected-component flood fill,
  first-visit-wins on any room reached twice by different paths (a real
  cycle, a one-way/teleport link, or non-planar layout) -- geometrically
  imperfect there, but always well-defined and rerunnable. A real
  graphical graph-drawing client view is a possible future follow-up,
  not built.

- **Client: enable cut/copy/paste** in the client. DONE 2026-08-21: the
  input box already had this for free (native Win32 Edit control
  behavior); added a menu-bar Edit menu (Cut/Copy/Paste/Select All) and a
  right-click Copy/Select All menu on the read-only scrollback (RichEdit
  has no built-in one). Shipped in client v0.4.32.

- **Client: launch/update notice.** DONE 2026-08-21: the silent
  auto-update path ran entirely before any window existed, so a real
  update (MSI download + up to 60s waited-on msiexec install) looked
  identical to the client failing to launch. Added a small always-on-top
  "Updating TobinMUD Client..." splash shown for that whole window.
  Shipped in client v0.4.32.

## Unimplemented skills/spells backlog (audited Session 158)

Grep-verified against `SKILLS[]` (skill.c) vs real handlers in cmd_cast.c /
cmd_pray.c / combat.c / cmd_*.c. Each entry below has ZERO real-handler refs
(falls through to the "nothing happens yet" placeholder, or a passive with no
wiring). Help-body "does nothing yet" text is STALE/unreliable — the code is
authoritative. Work top-down within a class; each ships with a smoke test.

**Druid spells:** all cleared (sunscald/feral wrath/wave crash/withering
touch/tree walk/leeching vine done 2026-08-18; cure blindness + word of
recall dropped — Cleric prayers).

**Cleric spell (pray):** relive (49, corpse resurrection) -- likely a
dead end, not just unimplemented: Tobin's PC death already has no
corpse (no permadeath; "resurrection" is already covered by soft-
respawn/relog, see combat.c's XP-loss block comment). Needs a design
conversation before picking back up, not a straight port.

**Warrior skills:** all cleared 2026-08-22 (advanced berserking done).

**Thief skills:** all cleared 2026-08-22 (spy, cudgel done).

**Monk skills:** all cleared 2026-08-18 (iron flesh/skin/bones/muscles/will + defenestrate + bonebreak; iron fist/iron legs were already wired).

**Common skills (all 6 classes):**
ranged proficiency + ranged specialization done 2026-08-22 (wired into
cmd_shoot.c's damage roll, same damage-only shape combat_strike() gives
kubo/voplat/melee specializations).
set trap arrow (26) DONE 2026-08-22 (obj.h's ARROW_TRAPPED, cmd_trap.c's `settrap arrow`, sprung by cmd_shoot.c on a landed hit -- smoke_test_set_trap_arrow.py). mine (37) / grenade (50) still blocked -- no room-floor trap object type, no thrown-weapon command.

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
