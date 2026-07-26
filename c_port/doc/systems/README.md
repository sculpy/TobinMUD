# TobinMUD systems — orientation

This is a map of what exists in `c_port/` and where to find it, for someone
picking this project up fresh. It is deliberately an **orientation** doc, not
a deep mechanics reference — for that, see:

- **[STATUS.md](../../STATUS.md)**'s "Architecture decisions" table — the
  *why* behind every deliberate deviation from the original SneezyMUD.
  **Note:** that table (and the "Module port status" table next to it)
  was last comprehensively updated early in the project (around the
  point PvP combat and the first objects/mobs landed) and has not kept
  pace with the session log below it — treat it as a snapshot of early
  decisions, not a current inventory. The session log itself (newest
  entries at the top) is the accurate, up-to-date record of what's been
  built since.
- **[TODO.md](../../TODO.md)** — the current backlog, what's deliberately
  out of scope, and standing rules.
- **`../../../sneezymud-master/docs/systems/`** — the *original* SneezyMUD's
  own 61-file systems reference (critical/important/informational tiers).
  Useful for checking real upstream behavior before porting or "fixing"
  something (house rule in CLAUDE.md) — Tobin is a from-scratch C rewrite,
  not a line-by-line port, so don't assume Tobin's version matches without
  checking.

As of this writing: 69 headers in `include/`, 128 files in `src/cmd/`
(one command handler per file, dispatched through `cmd_table.c`), and 180
`tests/smoke_test_*.py` files (one or more per feature — run individually
during development, `tests/sweep.sh` for the full ~85-minute regression
suite before a push).

## Directory layout

```
c_port/
  include/        one .h per subsystem — start here to find a struct/API
  src/core/        game logic: being/room/combat/skill/weather/etc (no I/O)
  src/db/          one *_repo.c per DB-backed table/feature (mariadb C API)
  src/cmd/         one cmd_*.c per player-facing command (128 files)
  src/net/         sockets, telnet, the <X> color-tag translator
  src/*.c          main.c, game_loop.c, config.c, log.c, world.c, crash_handler.c
  db/tobin/        Tobin-owned schema/migrations (see db/README.md)
  tests/           smoke tests, one file per feature, raw-socket driven
  doc/systems/     this file
```

The upstream SneezyMUD world seed (rooms/mobs/objects/shops, ~19,000 rows)
lives in the sibling `sneezymud-master/db/tobin/` directory and is loaded
once via `sneezymud-master/db/init-db.sh` — Tobin never touches that data's
shape, only adds its own tables alongside it (`c_port/db/apply-tobin-schema.sh`).

## Core data model

- **`thing.h`/`thing.c`** — the base "anything that can exist in the world"
  type (`thing_t`), first-member-embedded into `being_t`/`room_t`/`obj_t`
  (the inheritance replacement — see STATUS.md's "Inheritance replacement"
  decision). Containment (room floor / carried / worn / held) is one
  generic linked-list chain shared by every kind of thing.
- **`being.h`/`being.c`** — players AND mobs are both `being_t` (a mob is
  just `kind = THING_MOB`, `player_id = 0`, `desc = NULL` — no separate
  struct, mirroring the original's `TMonster : TBeing`). Carries
  attributes, level/XP/HP (`progress_t`), limbs, combat state, wait-state,
  affects, and every per-character feature flag added since (drug
  tracking, planting progress, mount, group, gametime offset, ...).
- **`room.h`/`room.c`**, **`obj.h`/`obj.c`** — rooms and objects, same
  `thing_t`-embedding pattern. Object prototypes are the upstream-seeded
  `obj` table read directly; `obj_category_t` collapses the original's 60
  item types into 16 buckets.
- **`world.h`/`world.c`** — lazy per-vnum room/mob/obj registry; no
  boot-time bulk load.
- **`zone.h`/`zone.c`**, **`zone_repo.c`** — zone resets (repopulating a
  zone's rooms/mobs/objects on a timer) and zone ownership/identity.

## Networking & session lifecycle

- **`net/main_socket.c`, `net/socket.c`** — raw POSIX sockets + `select()`,
  non-blocking fds. Direct descendant of the original's `TMainSocket`/`TSocket`.
- **`net/descriptor.c`** (the biggest file in the project) — the entire
  connection state machine: telnet IAC handling, login, the account/
  character menus, character creation, and every in-game **editor**
  (`edroom`/`edzone`/`edplayer`/`edobject`/`edmobile`/`edaccount`/`edsocial`/
  `edtrigger`/`hedit`/`balance` menu screens) — editors are modeled as
  `CONN_*` states here, not one-shot commands, matching the character-
  creation flow (see the `editors-menu-driven` project convention).
- **`net/colorstring.c`** — translates `<X>`-tag markup (e.g. `<c>`, `<C>`)
  into real ANSI escapes, or strips it if color is off. Single choke point,
  also does CRLF normalization for outbound text.
- **`descriptor_send()`** is the one function everything routes through for
  outbound text — if you need to change how text reaches the wire, this is
  where.

## Persistence

- **`db.h`/`db.c`** — thin C wrapper over `libmariadb`, a `%s`/`%r`/`%i`/`%f`
  query mini-language mirroring the original's `TDatabase`. No ORM.
- **`src/db/*_repo.c`** (27 files) — one file per DB-backed feature:
  accounts/players/rooms/mobs/objects (upstream-seeded tables, read-only
  prototypes) plus every Tobin-owned table (aliases, balance, board,
  bugs, drugs, help topics, ideas, ignore lists, magic items, quests,
  repairs, rules, shops, skills, socials, tips, treasury, triggers).
- **`db/tobin/*.sql`** — Tobin's own schema + idempotent migrations,
  applied by `db/apply-tobin-schema.sh` (safe to re-run after a pull).
  See [db/README.md](../README.md).

## Combat & the body

- **`combat.h`/`combat.c`**, **`cmd_attack.c`/`cmd_kill.c`/`cmd_hit.c`/
  `cmd_flee.c`/`cmd_bash.c`/`cmd_kick.c`/`cmd_disarm.c`** — round-based
  combat (PC-vs-PC and PC-vs-mob), skill-based special attacks (bash/kick/
  disarm/parry), an immortal-only instant `kill`, and the drowning-death
  path.
- **`limb_t`** (in `being.h`) — the real 18-slot limb enum (plus 4
  mob-only `EX_*` extra leg/foot slots for non-humanoid creatures),
  replacing an earlier flat placeholder set. **`body.h`/`body.c`** — 60
  creature body TYPES (`BODY_HUMANOID`/`BODY_SPIDER`/`BODY_DRAGON`/...)
  each with their own weighted per-limb hit-chance table, so a snake mob
  doesn't get hit "in the arm". `cmd_limbs.c`/`cmd_hurtlimb.c` are the
  player/immortal-facing commands.
- **`damage.h`** — `describe_dam()`, the 11-tier "pathetically" → "into a
  bloody pulp" damage-to-prose ladder (no raw numbers shown to mortals).
- **`material.h`/`material.c`** — 5-tier material quality (Common through
  Legendary) affecting damage/AC/durability/value.
- **`regen.h`/`regen.c`**, **`vitals.h`/`vitals.c`** — passive HP/limb
  regen and hunger/thirst/age drain, both pulse-driven.

## Magic, skills & classes

- **`skill.h`/`skill.c`** — the 3-tier skill/discipline framework
  (universal / class-core / secondary-discipline), proficiency-by-doing.
  6 classes (`CLASS_MAGE`/`CLERIC`/`WARRIOR`/`THIEF`/`DRUID`/`MONK`, see
  `being.h`) — the original's Ranger and 6 named Shaman spells were folded
  into Druid rather than kept as separate classes — each with its own
  spell/skill/prayer roster imported from the original.
- **`cmd_cast.c`/`cmd_pray.c`** — spellcasting and prayer, gated by class
  and known-skill state.
- **`affect.h`/`affect.c`** — the stat-modifying/timed-effect subsystem
  (buffs/debuffs/disease/poison/charm/polymorph/etc), the mechanism most
  spells and a few commands (e.g. `cmd_sacrifice.c`) apply their effects
  through.
- **`practice.h`/`practice.c`**, **`cmd_practice.c`** — skill training.
- **`trigger.h`/`trigger_script.h`/`trigger_script.c`** — a full DG
  Scripts-style scripting language (`if/while/switch`, `%var%`
  substitution, `set`/`eval`/`global`) for builder-authored mob/object/
  room triggers, authored through `edit trigger`.

## Classes' non-combat abilities & professions

- **`cook.h`/`cook.c`**, **`cmd_cook.c`** — the Cook profession (real
  ingredient-matching recipes).
- **`obj_plant.h`/`planting.h`**, **`obj_plant.c`/`planting.c`**,
  **`cmd_plant.c`** — seed farming (15 real crop types) and Thief's
  reverse-pickpocket `plant`.
- **`extraction.h`**, **`cmd_skin.c`/`cmd_butcher.c`** (corpse-based
  material gathering) **/`cmd_forage.c`** (wild foraging) — crafting &
  extraction.
- **`liquids.h`/`liquids.c`**, **`cmd_drink.c`/`cmd_pour.c`/`cmd_fill.c`/
  `cmd_sip.c`** — carried-liquid tracking.
- **`drug.h`/`drug.c`**, **`drug_repo.c`** — drug tracking (use/addiction
  state per being).
- **`repair_repo.h`**, **`cmd_repair.c`** — the repair-shop economy,
  feeding off material tiers' durability.

## Economy

- **`bank.h`/`bank.c`**, **`cmd_bank.c`** — Money system v2 (deposits/
  withdrawals, bank-flagged shops).
- **`balance.h`/`balance.c`**, **`balance_repo.c`**, **`cmd_balance.c`** —
  per-class/race combat balance modifiers, immortal-tunable.
- **`shop_repo.h`**, **`cmd_shop.c`** — NPC shops (264 real upstream
  shops).
- **`treasury_repo.h`** — accumulated sales-tax revenue.

## World content & building

- **Editors** (`edit <noun>`, dispatched from `cmd_edit.c`, state machines
  live in `descriptor.c`): `edit room`/`zone`/`player`/`object`/`mobile`/
  `account`/`social`/`trigger`/`help`(`hedit`)/`news`/`wiznews`/`rules`.
  All menu-driven like character creation — see the `editors-menu-driven`
  convention (a wireframe is supplied per editor, layouts aren't invented).
- **`cmd_goto.c`/`cmd_loadroom.c`/`cmd_load.c`/`cmd_dig.c`** — immortal
  building/navigation tools.
- **Mob AI**: **`mob_ai.h`/`mob_ai.c`** — pursuit and basic autonomous
  behavior, driven off the pulse scheduler.
- **`weather.h`/`weather.c`**, **`gametime.h`/`gametime.c`** — weather
  simulation and the in-game clock/calendar (real day/night, matches the
  original's weekday formula).

## Social & communication

- **`cmd_say.c`/`cmd_shout.c`** (and the `'` shorthand), `cmd_tell`-style
  whispers, **`socials.h`/`socials.c`** (155 emote verbs, imported from
  the original's `lib/actions`), **`board_repo.h`**/`cmd_board.c` (bulletin
  boards), **`ignore_repo.h`**/`cmd_ignore.c`, **`cmd_group.c`** (party/
  group system).

## Admin & immortal tools

- **`cmd_promote.c`/`cmd_set.c`/`cmd_stat.c`/`cmd_snoop.c`/`cmd_possess.c`/
  `cmd_poof.c`/`cmd_bamf.c`/`cmd_mortal.c`(mortal-toggle)/`cmd_egotrip.c`/
  `cmd_wipe.c`/`cmd_wiznet.c`** — the usual immortal toolbox. `log.h`/`log.c`
  is the typed game-log system (`cmd_log.c` reads it); every log bucket is
  immortal-visible only.
- **`cmd_copyover.c`**, **`shutdown.h`/`shutdown.c`** — hot-reload (exec-
  based copyover, connections survive) and graceful shutdown.
- **`multiplay.h`/`multiplay.c`** — the multiplay (multiple characters
  from one account, simultaneously) on/off gate, DB-persisted.

## Scheduling

- **`pulse.h`/`pulse.c`** — the fixed-table pulse scheduler (a pulse =
  100ms), everything recurring (combat rounds, regen, weather, mob AI,
  triggers, planting growth, drug decay, ...) registers a tick function
  here. **`wait_tick.h`/`wait_tick.c`** — the per-command wait-state clock
  (immortals bypass it).
- **`heartbeat.h`/`heartbeat.c`** — top-of-loop bookkeeping separate from
  the pulse table itself.

## Where this doc stops

This is a map, not a spec. For the actual mechanics of any one system —
exact formulas, edge cases, what's a deliberate simplification vs. a real
port of the original — read the file's own header comment first (most
non-trivial ones cite the real upstream source they were checked against),
then STATUS.md's session log for the change that introduced it.
