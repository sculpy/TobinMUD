# Tobin C Port — Status

Last updated: 2026-07-26 — Session 80 (home): **Newbie equipment
suits: 6 class-based starter kits, `loadsuit`, and the Welfare
Department social worker's gear reissue.**
- User: "we need 6 sets of newbie equipment to load on the character
  when connecting for the first time... a shield and a weapon based
  upon class choice", "or in room 570 (welfare) they could ask the
  social worker to receive a new set of newbie gear", "report on the
  vnums used for the new loadsuit immortal 56+ command... each suit is
  defined in the database as a suitset... we will require all builders
  to create at least one suit."
- Confirmed a fresh character previously got NO starting inventory at
  all (`being_create_pc()`/`player_create()` only ever set up stats).
  New `suit`/`suit_item` tables (`db/tobin/suit.sql`) -- a suit is just
  a named, optionally class-restricted bundle of obj vnums, builder-
  extensible with two INSERTs and no code change. `suit_repo.h/.c`
  (lookup layer) + `suit.h/.c`'s `suit_grant()` (create + drop loose
  into inventory -- deliberately NOT auto-equipped, user: "they can
  hold the items themselves, just load into inventory") back all three
  real features from one shared implementation.
- Seeded all 6 class suits: shared training shield (vnum 1010, real
  upstream item) + a class weapon + shared torch (105) + shared
  backpack (600). Three weapons reused real existing "training"-tier
  items (177 staff/Mage, 325 dagger/Thief, 329 sword/Warrior); no
  training-tier mace/sickle/nunchaku existed anywhere in the seed data,
  so three new ones were added at the identical stat profile (90003
  mace/Cleric, 90004 sickle/Druid, 90005 nunchaku/Monk) -- **reclaiming
  the numeric gap between the two existing Tobin-owned vnum blocks**
  (obj_magic.sql's 90000-90002, drug_items.sql's 90010-90013) rather
  than opening a new range, per the user's own "we can reclaim some
  vnums."
- **Auto-issue caught a real transaction/mutual-recursion trap early**:
  the first attempt tried to auto-equip suit items into hand slots
  (weapon/shield/torch all HOLD-flagged, only 2 hands) -- live-tested
  before the user simplified the ask ("just load into inventory"), and
  along the way a suspected DB corruption during testing (character
  creation intermittently landing at an empty account menu, some FK
  violations in the log) turned out to be self-inflicted test-script
  timing/pacing issues plus a known, already-documented "player_save on
  copyover races a throwaway test account's own cleanup DELETE" pattern
  -- not a real bug, confirmed by careful step-by-step re-verification.
- `loadsuit <suit name> [target]` (new `cmd_loadsuit.c`,
  `LOADSUIT_MIN_LEVEL` 56 -- same senior tier as `addnews`/help-edit,
  per the user's own "immortal 56+") -- abbreviation-matched suit name,
  defaults to self, or a named target found via the same room-lookup
  `transfer`'s siblings already use.
- Welfare Department social worker (mob vnum 90 "the ... social
  worker", room 570 -- BOTH already present in the real upstream seed
  data untouched, nothing new to add there) now actually does
  something: keyword speech ("gear"/"equipment"/"newbie"/"supplies")
  reissues the speaker's own class suit. Dispatch keyed on
  `SPEC_PROC_NEWBIE_EQUIPPER` (147) -- the real original engine's own
  spec-proc id for this exact NPC, already seeded on that mob vnum;
  same data-driven lookup-key precedent as `SPEC_PROC_DOCTOR`/
  `SPEC_PROC_LAMPLIGHTER` (no spec-proc EXECUTION ported, just the id).
  No anti-farming limit -- nothing asked for one, "lost your gear, get
  a replacement" is the whole point.
- `vnum <room|obj|mob> <range>` (cmd_vnum.c) now also lists the FREE/
  unused vnums in that range, not just what's taken -- user, hunting
  vnums for this same feature: "we can reclaim some vnums" then "i need
  a way to list them." Skipped if the existing-vnum listing itself got
  truncated, so a reported gap is never a guess.
- New `tests/smoke_test_newbie_gear.py` (16 checks, two consecutive
  clean runs): fresh-character suit issue (items present, loose not
  equipped), `loadsuit` on self and a named target (each gets their own
  notice), a bogus suit name refused cleanly, and the social worker
  reissuing on request while staying silent for unrelated speech.
  Regression-checked `smoke_test_accounts.py` and `smoke_test_vnum.py`
  (both still pass clean). Deployed via copyover, zero build warnings
  across the whole 8-file feature. Test debris (dropped items,
  throwaway characters) cleaned up from Center Square/room 570
  afterward.

### Session 79 (home): **Fixed
`tests/smoke_test_affects.py`, flagged as its own follow-up two
sessions ago.**
- Two separate, unrelated, pre-existing issues, both confirmed via live
  repro before touching the test: (1) `load obj` lands in the loading
  immortal's own inventory, not the room floor (documented gap,
  2026-07-22) -- the test never dropped it, so the Cleric's later `get
  symbol` always failed. Fixed with the same load-then-drop workaround
  `smoke_test_drugs.py`/`smoke_test_affect_persistence.py` already use.
  (2) `damages_from()`'s `for (\d+) damage` regex can never match
  anymore -- `combat.c`'s `describe_dam()` replaced every raw damage
  number with a qualitative word ("pathetically", "very lightly", ...)
  for mortals AND immortals alike (a later, deliberate change,
  postdating whenever this test last actually passed). Replaced the
  whole damage-number-measurement approach with the live-HP-loss-via-
  `score` pattern `smoke_test_affect_persistence.py` introduced last
  session (`hp_of()`/`hp_loss_over()`) -- reads the real in-memory HP
  directly rather than parsing combat spam.
- Verified live on Home: two consecutive full runs, both clean passes
  (Sanctuary's damage-rate reduction showed as 3.50->2.25 and
  4.60->2.38 HP/round respectively -- comfortably past the 20% cutoff
  both times, not a borderline result). Test-only change, no game
  source touched, no rebuild needed.

### Session 78 (home): **`get all`, `get
all.<item>`, and `drop all` (user requests, mid-session).**
- User: "drop all should drop all items in inventory", then "and
  all.items should also work for get", then "get all all.corpse too"
  (clarifying both the bare and dot forms should work for get).
  Previously Tobin only had the two-word `get all <container>`
  (emptying a named container like a corpse) -- a bare `get all` fell
  through to a literal "find an item named all" lookup that always
  failed, and `drop` had no multi-item form at all.
- `cmd_object.c`: new shared `get_all_from_room()` helper backs both
  bare `get all` (every takeable item on the floor) and `get all.<name>`
  (dot syntax, classic Diku convention -- every item on the floor
  matching `<name>`, reusing find_obj()'s own per-keyword-prefix
  matching just without stopping at the first hit). `drop all` drops
  every LOOSE carried item (worn/held items untouched, same as a single
  `drop` already requires removing first). All three reuse the existing
  per-item message/log/trigger/save plumbing, so behavior matches using
  the singular commands one at a time.
- Verified live: `get all.torch` on a 3-item floor (torch, torch, rock)
  picked up only the 2 torches; a follow-up bare `get all` picked up
  the rock; `drop all` correctly emptied the resulting inventory back
  onto the floor. Confirmed the existing two-word `get all <container>`
  path (emptying a chest) still works unchanged -- untouched code path,
  double-checked live anyway. Deployed via copyover, zero build
  warnings.

### Session 77 (home): **Active affects
(buffs/debuffs) now persist across a quit!/reconnect, closing a real
gap a docs/systems audit found.**
- The audit (spawned earlier this session) compared sneezymud-master's
  own persistence docs against Tobin's `src/db/*_repo.c` layer looking
  for "should be DB-backed but isn't" gaps. Nearly everything checked
  out as already-disclosed, deliberate simplifications (mount/rider,
  drug effect ticks, planting progress) -- except one: `being.h`'s
  `active_affect_t affects[]` was commented "session-scoped, meaningless
  across a reconnect, same as a real MUD", but the original's own
  `08-persistence-storage.md` shows SneezyMUD's charFile actually DOES
  round-trip active affects through every login/logout. User: "make it
  persistent."
- New `player_active_affect` table (db/tobin/) + `affect_repo.h`/`.c`
  (load/save-all, matching drug_repo.h's shape), wired into the single
  `player_save()`/`player_load()` choke point (`player_repo.c`) every
  other per-player save/load already shares -- covers quit!, combat
  defeat, the 5-minute linkdead auto-purge, and the explicit `save`
  command for free from one place, no new call sites needed. Load is
  bookkeeping-only (doesn't re-apply a stat-affect's `modifier` to
  attrs -- `player_attrs_save()` already snapshots the LIVE, already-
  modified attrs blob, so re-applying on top would double it).
  AFFECT_CHARMED/AFFECT_POLYMORPH are skipped defensively -- both live
  on a mob (a summoned pet, a temporary transformed body), never a
  real player's own being_t, so they were never reaching this path
  anyway.
- **Named `player_active_affect`, not the shorter `player_affect`** --
  caught live via added debug logging (`log_info`, temporarily) after
  the first end-to-end test showed the save silently failing:
  `player_affect` was ALREADY a table name, an unrelated, unused
  (0-row) leftover from the upstream SneezyMUD seed schema with a
  completely different, incompatible column set (`type`/`level`/
  `duration`/`renew`/`modifier2`/`location`/`bitvector`...) --
  `CREATE TABLE IF NOT EXISTS` silently no-op'd against it instead of
  creating the right schema. Confirmed nothing in Tobin's own code
  references the old table (same "inert upstream leftover" status as
  the `drug_use` precedent from the `player_drug` fix two sessions
  ago) -- renamed Tobin's own table rather than touching the old one.
  Debug logging removed once root-caused.
- New `tests/smoke_test_affect_persistence.py`: casts Sanctuary, does a
  real `quit!` (not a raw socket close), reconnects as the same
  character, and checks THREE things, not just the display -- Sanctuary
  is still listed with a sane (not reset, not grown) remaining
  duration, it's still actually reducing HP loss under sustained
  attack (not just cosmetic), and it still wears off cleanly afterward
  with no double-reversal bug introduced by the reload.
- **Found along the way, NOT fixed here** (flagged as its own follow-
  up task instead): the existing `tests/smoke_test_affects.py` has been
  broken by two separate, unrelated, pre-existing issues -- the
  documented `load obj`-lands-in-inventory-not-room gap (2026-07-22),
  and a newer one: `combat.c`'s `describe_dam()` replaced ALL raw
  damage numbers with qualitative words ("pathetically", "very
  lightly", ...) for mortals AND immortals alike, so that test's
  `damages_from()` regex can never collect a single sample anymore.
  This session's new persistence test sidesteps both (works around the
  load/drop gap, measures via live `score` HP polling instead of
  message parsing) rather than fixing the older test, which needs its
  own separate pass.
- Verified live on Home: zero-warning clean rebuild (two header
  changes), deployed via copyover with a player connected, full pass
  on the new persistence test plus regression passes on
  `smoke_test_drugs.py` and `smoke_test_accounts.py` (both exercise
  the same shared `player_save()`/`player_load()` path this change
  touched).

### Session 76 (home): **Two small closeouts:
`db/fix-workbox.sh` (Work box's still-pending DB rename + player_drug
schema catchup, one script, unattended-safe) and a resolved design
decision on the destroyed-limb hit penalty.**
- `db/fix-workbox.sh`: combines the two things Work (db.kullit.com)
  still needs from Sessions 73/75 -- the sneezy->tobin rename and the
  player_drug schema catchup -- into one idempotent script (backup,
  atomic RENAME TABLE, apply-tobin-schema.sh, clean rebuild, restart
  both preview/production instances, smoke-test both). Refuses to run
  with a real player connected. Committed and pushed; Work box itself
  is still unreachable from this environment, so it's staged for
  whoever can actually SSH in to run `git pull && bash
  db/fix-workbox.sh`.
- **Destroyed-limb hit penalty scaling** (TODO.md, open since the
  Hospital-mechanic work): asked the user directly rather than guessing
  -- flat -15 (current behavior, doesn't get worse with more destroyed
  limbs) vs. scaling per limb, capped or uncapped. No upstream SneezyMUD
  precedent exists for this mechanic at all (`combat.c:44`'s own comment
  already flagged it as Tobin-original), so there was no "port it
  correctly" answer available, only a genuine design call. **Decided:
  stays flat.** No code behavior change; updated `combat.c`'s comment to
  record the decision (no longer a "placeholder") and closed the TODO
  item.

### Session 75 (home): **Fixed the missing
`player_drug` table flagged at the end of Session 74, plus a real
`score` display bug caught while verifying it.**
- User-supplied task: figure out why production logged
  `Table 'tobin.player_drug' doesn't exist` on every login, fix it on
  both boxes without touching real player data, and decide what to do
  with the orphaned `drug_use` table.
- Root cause: `db/tobin/player_drug.sql` was correct but had never
  actually been run against Home's live DB — predates the sneezy→tobin
  rename entirely (confirmed absent from the old `sneezy` DB too via
  `information_schema.TABLES`, so not caused by that rename). Checked
  `src/db/drug_repo.c`/`src/core/drug.c` for both `player_drug` and
  `drug_use`: only `player_drug` is referenced anywhere in Tobin's code
  — `drug_use` is inert leftover upstream seed schema (0 rows), left
  alone rather than dropped.
- Fix: re-ran `db/apply-tobin-schema.sh` on Home — safe/idempotent since
  every file in `db/tobin/` uses `CREATE TABLE IF NOT EXISTS`/`ON
  DUPLICATE KEY UPDATE`. Applied all 23 files clean; `player_drug` now
  exists with the right schema, and the 4 missing drug-item vnums
  (90010-90013) came back too. **Work box (db.kullit.com) still needs
  this same catchup** — unreachable from this session, same as the
  DB-rename item it's now bundled with in TODO.md.
- While verifying via `tests/smoke_test_drugs.py`, hit a `TypeError`
  crash in the test's own `stats_from_score()` helper — its regex only
  ever matched full-word stat labels (`Strength:`, `Dexterity:`, ...),
  which never matched `score`'s real abbreviated output at all. Went to
  fix the test and found the real story was the opposite: `score`
  itself (`cmd_score.c`) abbreviates five of six stats but spells out
  `Wisdom:` in full — a genuine inconsistency with no upstream Sneezy
  precedent (this exact score layout is Tobin's own revamp from Session
  ~71, not a port), confirmed by checking `sneezymud-master` for a
  matching `do_score`-style stat block and finding none. Fixed
  `cmd_score.c` to say `Wis:` like its siblings, and updated the test's
  stale regex to match the now-consistent format.
- Verified live on Home: full 10-check `smoke_test_drugs.py` run passes
  clean (dose apply/consolidate/expiry, Hobbit bonus, item destruction
  on last charge, and — the one that actually exercises Str/Con parsing
  for the first time — withdrawal's real STR/CON penalty). One
  transient false-negative along the way (failed once right after a
  copyover, passed cleanly on immediate re-run and via an isolated
  standalone repro) — treated as a copyover-timing fluke per the sweep-
  failure-triage habit, not chased further since it didn't reproduce.
  Deployed via copyover, zero build warnings.

### Session 74 (home): **OLC editor menus
colorized (ported from real SneezyMUD cyan/purple, no boxes), deployed
live via copyover with a player connected.**
- TODO.md's "Boxed ASCII-art menu rework, remaining editors" item,
  unblocked when the user clarified it doesn't need an invented box
  wireframe: "port directly from sneezy and i will correct adhoc". A
  research pass over `sneezymud-master/code/code/misc/create_{rooms,
  mobs,objs}.cc` (`update_room_menu`/`update_obj_menu`/`update_mob_menu`)
  found the original's real OLC editors have **no box borders at all** --
  plain numbered `N) Label` lists, colorized cyan-for-numbers/purple-for-
  labels via `ch->cyan()`/`ch->purple()`/`ch->norm()` (non-bold
  `\033[36m`/`\033[35m`/`\033[0m`), raw VT100 cursor-addressed, no shared
  "print in a box" helper anywhere in that codebase. Also confirmed no
  zedit/pedit exist upstream -- both are Tobin-original inventions. Mid-
  task the user extended the ask: "and use colorization from sneezy for
  all menus".
- Applied `<c>N)<z> <p>Label<z>` (Tobin's own `<c>`/`<p>` tags are an
  exact match for the original's non-bold cyan/purple, and match the
  project's own "colorize with lowercase tags" habit) across every still-
  plain menu in `descriptor.c`: `edit room` (main + flags/terrain/exits/
  exit-submenu/doortype/conditions/extra-desc list+item), `edit player`,
  `edit zone`, `edit object` (main + action-flags + wear-flags), `edit
  mobile`, `balance`, `edit account`, `edit social` (item view), `edit
  trigger` (list + item). One alignment nuance: `show_edsocial_item()`'s
  two-column `%-22s` layout had to widen to `%-34s` to absorb the 12
  invisible tag bytes each colorized label now carries, since `%-Ns`
  pads on byte length, not the tag-aware visible width `visible_len()`
  computes for the (deliberately untouched) account-menu box style.
  Those two styles -- real Unicode box vs. plain colorized list -- are
  now both legitimate, just for different screens: the account menu got
  a user-supplied box1.txt wireframe; every OLC editor gets sneezy's
  actual un-boxed style instead.
- Verified: zero-warning build, `tests/smoke_test_redit.py` full pass
  (32 checks) live against Home production, plus a raw-socket capture of
  the real `edit room` output confirming the actual ANSI escape bytes
  (`\x1b[0;36m`.../`\x1b[0;35m`...) land correctly. A real player was
  connected during deployment -- used `copyover` (not a hard restart) via
  a throwaway level-59 test account scripted over raw socket; both
  connections (the real player and the throwaway) survived, confirmed via
  the copyover log lines and the connection staying ESTABLISHED
  throughout.
- **Found, flagged, NOT fixed** (out of scope, spawned as its own follow-
  up task): production is missing the `player_drug` table entirely
  (`ERROR: query failed: Table 'tobin.player_drug' doesn't exist` on
  every login) -- confirmed pre-existing (absent from the old `sneezy` DB
  before this session's rename too, not something the rename caused). A
  differently-named `drug_use` table exists live instead; needs its own
  investigation into whether that's dead leftover data or something code
  still reads.
- Reconciled the Home VM's git working tree afterward (`git fetch && git
  reset --hard origin/main`) -- it had been running on a tar-synced,
  pre-commit working copy since Session 73's rename work; now a clean
  checkout of the pushed commits again.

### Session 73 (home): **TobinMUD identity + DB
rename: underlying MariaDB database `sneezy` -> `tobin` (Home VM done and
live-verified; Work box pending, unreachable this session).**
- TODO.md's long-open "TobinMUD identity + DB rename" item, picking up
  where the 2026-07-01 partial rename (Session 1 part 3) deliberately
  left off -- that session renamed every code identifier
  (`DB_TOBIN`/`TOBIN_DB_*`/`tobin_c`) but explicitly kept the literal
  MariaDB database name `sneezy` and `db/sneezy/*.sql` paths unchanged,
  since `db/` itself wasn't renamed at the time. This session finishes it.
- **Code side** (`c_port/`): `TOBIN_DB_NAME` default (`config.c`/`.h`),
  `db.h`'s `DB_TOBIN` comment, `c_port/db/sneezy/` -> `c_port/db/tobin/`
  (23 files, `git mv`), `apply-tobin-schema.sh`'s default arg + dir
  lookup, every `db/sneezy/...sql` path mentioned in an `include/*.h` or
  `src/**/*.c` comment (28 files), and all 151 `tests/smoke_test_*.py`
  files' hardcoded `mariadb sneezy ...` CLI calls (187 occurrences,
  mechanical `"sneezy"` -> `"tobin"` string swap -- verified none of the
  non-quoted prose "Sneezy" mentions in the same files, which credit the
  original project by name, got touched). Top-level docs (`README.md`,
  `db/README.md`, `CLAUDE.md`, `SYNC.md`, `ENVIRONMENT.md`) updated to
  match, since those describe current operational procedure. Historical
  STATUS.md/TODO.md session-log prose describing what was true AT THE
  TIME was deliberately left alone (same precedent this file's own
  Session 1 part 3 entry set) -- only TODO.md's one *standing rule* about
  future `db/tobin/*.sql` files was updated, since that's forward-looking.
  Also hand-patched the local, gitignored `sneezymud-master/` reference
  clone's `db/init-db.sh` + `db/README.md` (text) and renamed its own
  `db/sneezy/` seed-dump dir to `db/tobin/` for consistency -- this is
  LOCAL-ONLY (that tree is "re-clone per location", never git-synced) and
  not load-bearing for the actual live rename below, since `init-db.sh`
  is a destructive one-time bootstrap script never re-run against a live
  box.
- **Home VM (192.168.254.200) live rename**: `mysqldump --single-transaction
  sneezy` backed up first (21 MB, kept at `~/db_backups/` on the box, NOT
  deleted). Server stopped, `tobin` DB created (needed `sudo mariadb` --
  `mud` has no `CREATE DATABASE` grant, only per-DB `ALL PRIVILEGES` on
  `sneezy`/`immortal`), all 108 tables moved with one atomic multi-table
  `RENAME TABLE sneezy.x TO tobin.x, ...` statement (metadata-only,
  zero data copy, zero data loss -- confirmed `tobin` has 108 tables,
  `sneezy` has 0 afterward, `tobin.player` still has all 1438 real rows).
  Granted `mud` privileges on the new `tobin.*`. `.env.local` (per-box,
  gitignored, not in git) updated to `TOBIN_DB_NAME=tobin`. Clean rebuild
  (`make -j4`, zero warnings), restarted, re-attached gdb per the standing
  habit. Verified two ways: raw-socket connect shows the real login
  banner, and `smoke_test_accounts.py` passes clean end-to-end (account
  creation, point-buy, character connect into a real seeded room, delete)
  against the renamed DB. No players were connected at rename time
  (checked first). Old `sneezy` DB deliberately NOT dropped -- kept as a
  rollback safety net; user's call when to actually drop it.
- **Work box (db.kullit.com) -- NOT done**: unreachable from this session
  (SSH times out / "Network is unreachable" from Home's network for the
  public IPs `db.kullit.com` resolves to; the Work-specific deploy key
  `~/.ssh/id_ed25519_kullit` also isn't present on this machine). Code is
  already pushed to `main` (commit `2178986`), so Work's side is just:
  `git pull`, `rm -rf build/obj && make -j4`, then the identical
  backup -> stop -> `CREATE DATABASE tobin` (sudo) -> one atomic
  `RENAME TABLE` covering every `sneezy.*` table -> grant -> update
  `.env.local` -> restart -> reattach gdb -> smoke-test sequence used on
  Home above. Do this from a session that can actually reach
  `db.kullit.com` (or forward the connection some other way).
- Next: finish the Work box rename per the runbook above, then this TODO
  item is fully done.
- **Object stacking** (user: "object stacking needs to work on
  inventory"): `cmd_inventory()` (cmd_object.c) now groups identical
  rendered lines together with a "(xN)" suffix, reusing the exact
  "group by the rendered string itself" technique `cmd_look.c`'s
  `group_room_items()` already established for room-floor items/mobs
  (new `render_inventory_item()` + the same grouping loop) rather than a
  separate vnum-equality check -- naturally handles both real prototype
  items (same vnum, same condition tier render identically) and
  ephemeral items like Planting's fruit/hide/meat (vnum 0, grouped by
  label instead) with one mechanism. New
  `tests/smoke_test_inventory_stacking.py` (3 checks). Regression-tested
  against several other object-heavy tests; two unrelated pre-existing
  failures surfaced (`smoke_test_containers.py`, `smoke_test_corpse.py`)
  -- both the same already-documented "`load obj` puts the item in the
  loading immortal's own inventory, not the room" gap from 2026-07-22,
  never retrofitted into these older test files, confirmed unrelated by
  checking their own git history (last real edit predates that fix).
- **`smoke_test_limbs_cmd.py` flake, finally root-caused**: not infra
  flakiness or anything wrong in `hurtlimb`/`limbs`/
  `being_limbs_full_heal()` (all confirmed deterministic, as the
  original TODO.md note suspected) -- `regen_tick_run()` (regen.c) heals
  EVERY limb a small amount every `REGEN_PULSES` (~5s real time) for any
  connected, non-fighting character (`being_heal()`'s per-limb
  spillover, itself correct behavior). If that tick lands in the gap
  between the test's `hurtlimb` call and its immediate `limbs` read --
  usually sub-second, but not guaranteed -- the injured limb picks up
  +1 HP (2->3 out of the 15-floor max, 13% -> 20%), a genuine race
  against wall-clock time. Fixed by accepting either outcome instead of
  requiring one exact figure; verified with 4 consecutive clean runs.
- Also logged this session (not started): user, "when planting a money
  tree, you see 'A single talen is here. (x4)' -- that should be gold."
  See TODO.md.

### Session 71 (home): **Three follow-ups from
Session 70's account-menu batch, all confirmed and closed.**
- **`smoke_test.py` fixed**: its character-creation sequence predated
  both the "confirm new account" y/n step and the race/class-before-
  attrs reorder, plus never accounted for the color/timezone prompts a
  brand-new account gets -- silently out of sync with the real server
  flow since before this session. Rewritten to match
  `smoke_test_accounts.py`'s current sequence; passes cleanly end to end.
- **Real bug fixed: `player_delete()` orphaned linkdead beings.** A
  character disconnected via a raw socket close (never `quit!`) leaves a
  linkdead body standing in its room (`descriptor_destroy()`'s own
  documented behavior); deleting that character from the account menu
  only ran a DB DELETE (`player_delete()`, player_repo.c), leaving the
  in-memory being_t orphaned forever, now pointing at a player_id that
  no longer exists. Fixed at the call site
  (`CONN_CHAR_DELETE_PASSWORD`, descriptor.c): looks up
  `player_id_for_name()` BEFORE the DB row goes away, then
  `world_find_linkdead_pc()` + `being_destroy()` after a successful
  delete -- the same lookup a real reconnect already uses in
  `enter_world()`. New `tests/smoke_test_delete_linkdead.py` (5 checks)
  reproduces the exact scenario end to end.
- **Not a bug: `smoke_test_multiplay.py`'s "off by default" assumption.**
  Root-caused via a live gdb breakpoint on the multiplay gate
  (`enter_world()`): `multiplay_allowed()` was genuinely returning true,
  not because of anything wrong in the gate logic itself, but because
  the multiplay flag PERSISTS in the `game_config` DB table across a
  server restart (`multiplay.c`'s `multiplay_load()`/`multiplay_set()`
  -- not just an in-memory default like previously assumed) -- an
  earlier crashed test run had left it stuck "on" in the DB, and every
  subsequent restart faithfully reloaded that stuck value. Reset the DB
  row directly, and made the test itself defensive against this exact
  scenario going forward (resets the flag to off at its own start,
  same "prior crashed run" precedent already established elsewhere in
  this codebase's tests). Confirmed clean with a fresh restart + rerun.
- All three verified with the relevant test(s) passing; test/leftover DB
  rows cleaned up. No product code changed except the one real
  `player_delete()` fix -- zero build warnings.

### Session 70 (home): **Account-menu UX batch --
immortal quit! now purges instead of drops, character creation's attribute
screen redesigned into a numbered grid per user wireframe, handedness/
gender/alignment/appearance moved into their own second boxed menu, and
deleting a character now shows a numbered pick list.**
- **Immortal purge on quit!** (`cmd_quit.c`): a mortal's `quit!` still
  drops everything on the floor (existing behavior); an immortal's now
  gets `obj_destroy()`ed outright instead -- an immortal's inventory is
  almost always `load`ed test/debug props, and scattering those on every
  quit was clutter a mortal's real belongings shouldn't share the same
  treatment as.
- **Character creation redesign** (user-supplied wireframe): the
  attribute point-buy screen (`show_attr_screen()`, descriptor.c) is now
  a numbered 2x3 grid (`1) Str: 120  2) Dex: 120  3) Con: 120` / `4) Int
  ... 6) Cha`) instead of one-stat-per-line -- a bare number 1-6 opens a
  small "how much?" sub-prompt (new `CONN_CHAR_CREATE_ATTR_AMOUNT` state,
  `apply_attr_delta()` shared with the still-working direct "str 30"
  syntax). Handedness/gender/alignment/appearance moved OUT of that
  screen entirely into a NEW second boxed menu (`show_options_screen()`,
  `CONN_CHAR_CREATE_OPTIONS`), each its own numbered sub-menu
  (`CONN_CHAR_CREATE_OPT_HAND`/`_GENDER`/`_ALIGN`/`_APPEARANCE`) --
  replaces the old standalone always-shown alignment screen; alignment
  now defaults to neutral if the sub-menu is never visited, matching the
  "optional, sensible default" precedent hand/gender/appearance already
  had. Both screens' footer is the wireframe's `D)one R)eset A)bort or
  Quit`.
- **Mechanical test-suite fix required**: 140+ existing smoke test files
  drive character creation through a raw socket, and every one of them
  sent a bare "2" as the final step (picking Neutral alignment, which
  used to ALSO finish creation in one shot). Since alignment moved into
  the options sub-menu, that same input now means something else
  entirely -- every occurrence became "done" (the options menu's own
  finish command, landing on the same neutral-default result). Two
  files needed real logic changes beyond the mechanical swap
  (`smoke_test_gender.py`'s inline `gender`/`appearance` commands moved
  into the sub-menu flow; `smoke_test_accounts.py`'s literal
  "Strength:      120"-style assertions updated to the new grid text,
  plus its own account-menu-hidden-list interactions adjusted).
- **Delete-character picker** (user: "when deleting a character, the
  player should be presented a list of his characters so he could
  choose properly"): bare `D`/`delete` (no name) at the account menu now
  shows the same numbered box `C` reveals, then accepts a number OR a
  name (new `CONN_CHAR_DELETE_PICK` state, since a bare number at the
  PLAIN account menu already means "connect" -- a dedicated state was
  needed so the exact same input means something different in this
  context, rather than colliding with that existing shortcut, which is
  the bug the first version of this fix actually shipped with before
  being caught in testing: a bare "2" briefly connected to character #2
  instead of deleting it).
- **Two real bugs caught in testing**: (1) the delete-picker collision
  just described. (2) a genuine, unrelated latent bug surfaced by a
  test's own linkdead-body leftover: `player_delete()` (player_repo.c)
  is DB-only -- deleting a character that has a linkdead body still
  resident in the world (disconnected via raw socket close, never
  `quit!`) leaves that being_t orphaned forever, now pointing at a
  deleted player_id. Flagged as its own follow-up task, not fixed here
  (out of scope for this UX batch).
- Also flagged, not caused by this session's changes: `tests/
  smoke_test_multiplay.py`'s "second character refused while multiplay
  is off" check fails even on a fresh restart -- confirmed the affected
  code path (`enter_world()`'s multiplay gate, descriptor.c) wasn't
  touched by anything in this batch; needs its own investigation.
- Regression-tested via 8 diverse smoke tests (accounts, quit-menu,
  quit-during-creation, trade-attrs, menu-letters, gender/appearance,
  plus manual verification of the purge and delete-picker fixes) rather
  than a full sweep, given the sheer number of touched files -- a
  targeted but broad sample across every creation-flow-adjacent test
  category. Deployed to production, zero build warnings.
- Also logged this session, not yet started: user, "when connecting
  during a reboot, we should accept the connection and give some
  booting information, see the peel sneezy for inspiration" + "and also
  for the logs" -- needs a closer look at `peel-sneezymud/`'s real
  reboot/copyover behavior and Tobin's own main.c/game_loop.c startup
  sequence before scoping (see TODO.md).

### Session 69 (home): **Crafting & extraction --
`skin`/`butcher` (mob-corpse material gathering) + `forage` (wild food),
the last open item from the 2026-07-19 Sneezy → Tobin feature audit.**
- User said "you pick" on what to build next -- picked the last remaining
  audit-list item (everything else in that ~26-item list was already
  done). Checked the real upstream's own doc first
  (`docs/systems/informational/crafting-extraction.md`): a much bigger
  system spanning Ranger (skin/butcher/forage), Shaman (brew/dissect),
  Mage (scribe), and Warrior (blacksmithing/sharpen). Scoped down to a
  Tobin-scale slice per this whole audit's own ground rule ("large
  economy/material systems get a Tobin-scale slice, not the full
  original depth"): skin+butcher+forage only, all landing on Druid
  (Tobin's established Ranger-flavor analog). Brewing/scribing need a
  Shaman-style component/charge system Tobin doesn't have; dissection
  needs per-race quest-item data outside this bundled source; material-
  category repair would duplicate the simpler repair system Object
  maintenance already shipped -- all disclosed cuts, not silent gaps.
- **Mechanism**: corpses (already lootable containers, combat.c) gained
  a `raw_type` marker (mob vs PC -- `CORPSE_KIND_MOB`/`_PC`, new
  `extraction.h`) and a `val[3]` flag bitmask (`CORPSE_SKINNED`/
  `_BUTCHERED`) for once-only extraction -- no half-yield partial tier,
  unlike the original. `skin`/`butcher` (new `cmd_skin.c`/`cmd_butcher.c`)
  resolve instantly against a mob corpse in the room (no multi-pulse
  task -- no general task engine exists in Tobin outside Planting's own
  one-off), yielding ONE generic item each (a hide, weight-scaled off
  the corpse; a steak, using the FOOD category's existing val[0]/val[1]
  max/current-units convention) rather than porting the original's full
  per-race item-mapping table. `forage` (new `cmd_forage.c`) gathers
  wild food outdoors (reuses Planting's `room_can_plant()` gate) with a
  cooldown via a new `AFFECT_FORAGE_COOLDOWN` -- reusing the EXISTING
  buff/debuff expiry machinery instead of a dedicated timestamp field.
- New `tests/smoke_test_craft.py` (13 checks). Deployed to production,
  zero build warnings.
- Also logged this session (not started): user, "object stacking needs
  to work on inventory" -- TODO.md's newest "Buildable now" batch item.

### Session 68 (home): **Planting -- seed farming
(all 15 real crop types) + Thief reverse-pickpocket, plus two real bugs
found and fixed live (a full pulse-registration table silently dropping
a tick function, and a use-after-clear bug in the sowing task's own
safety check).**
- **Scoped via AskUserQuestion**: both unrelated mechanics the real
  `plant` command covers, in one pass ("Both, same pass"), and the full
  15-plant-type depth rather than a trimmed slice ("Full 15-type
  system"). Found while investigating a user question in
  `peel-sneezymud/`, a fuller reference clone than `sneezymud-master/`.
- **Seed farming**: new `obj_plant.h`/`obj_plant.c` ports TPlant
  (`obj_plant.cc`) as an ordinary ephemeral obj_t (category
  `OBJ_CAT_OTHER`, a new `raw_type==OBJ_PLANT_RAW_TYPE` marker) rather
  than a new struct -- reuses the existing generic `val[4]` payload for
  TPlant's own four fields (type/age/yield/verminated). All 15 real
  seed vnums and fruit pairings ported verbatim from `seed_to_plant()`/
  `plantfruits[]`; type 11 displays as "pot" (Tobin's own established
  drug-naming convention, `drug.c`'s `DRUG_POT`) rather than the
  original's literal "marijuana". Disclosed simplification: the
  original's real lifespans run 6 months to 15 real YEARS -- compressed
  into tens/hundreds of ~1-real-minute growth ticks so a plant is
  actually observable in a session, preserving the original's fast/slow
  ordering (candy heart trees still wither far faster than rose bushes).
- **`plant <seeds>`** (anyone, outdoors -- new `room_can_plant()`,
  room.c, same fall/water/indoor/underwater refusal set as the
  original) starts a new 3-step dig/sow/cover task. No general task
  engine exists in Tobin yet, so this is a deliberately scoped one-off
  (new `being_t` fields `planting_seed`/`planting_ticks_left`/
  `planting_type`/`planting_room`, new `planting.c`'s
  `planting_tick_run()`) rather than porting the original's task-system
  integration. Growth (`obj_plant_growth_tick()`) ages every planted
  crop, occasionally yields fruit (dropped onto the room floor next to
  the plant, not into it as a container -- Tobin's flatter object model
  has no "look in <plant>" step) or loses it to vermin (a flat 10%
  roll), and hands a withered plant off to the EXISTING
  `obj_decay_tick()` machinery to actually disappear.
- **Thief `plant <item> <victim>`**: reverse pickpocket, gated on the
  "plant" skill itself (both `plant` and `steal` already existed as
  unwired skill.c placeholders) rather than "steal" the way the
  original oddly does. PC-vs-PC targeting reuses combat.c's own mutual
  `toggle pk` consent gate (now exported as `combat_pk_allowed()`) since
  the original's peaceful-room gate has no Tobin equivalent yet -- mobs
  are always fair game.
- **Two real bugs caught live, not just guessed at**: (1)
  `pulse_register()`'s `MAX_PULSE_PROCESSES` cap (24) was already fully
  used -- registering the two new tick functions silently dropped one
  (the same failure mode a past session already flagged once for
  `trigger_pending_tick`), so the sowing task never advanced until the
  cap was bumped to 32 with real headroom this time. (2)
  `planting_tick_run()`'s own "do you still have the seeds" safety check
  ran on every tick including the one right after the sow step
  legitimately consumed them, misreading its own successful consumption
  as "you lost your seeds" and aborting one step before completion --
  fixed by only checking while there's still a real seed pointer to
  check.
- **Live mishap, disclosed rather than hidden**: `aitick` (the existing
  debug tool that force-advances world ticks) was run repeatedly against
  PRODUCTION while a real user was connected, fast-forwarding weather
  and, unintentionally, aging the user's own live-planted crop to
  withering before they could harvest it. Apologized live. A reminder to
  be more careful running world-tick-forcing debug commands on the live
  server while players are online.
- New `tests/smoke_test_plant.py` (23 checks). Deployed to production,
  zero build warnings.

### Session 67 (home): **Transformation --
Polymorph (Mage) + Disguise (Thief) -- plus two real bugs found and
fixed along the way (a missing SIGPIPE handler, and a memory-corruption
path in combat death-handling for a possessed/polymorphed mob). Shipped
with a disclosed, NOT fully eliminated residual crash risk -- see below.**
- **Scoped via AskUserQuestion**: fixed form per spell (no player choice
  of target shape), covering Polymorph (Mage) + Disguise (Thief) only --
  user explicitly declined a Druid Shapeshift this pass.
- **Polymorph**: reuses the exact `possess`/`return` puppet-swap pattern
  (`descriptor_t.possess_original`) that immortal `possess` already used,
  now made mortal-accessible and timed. `being_start_polymorph()` swaps
  the descriptor onto a real seeded mob body (brown bear, vnum 585) for
  `TRANSFORM_DURATION_ROUNDS` (~5 min), tracked via new `AFFECT_POLYMORPH`;
  expiry (or an explicit early `return`) reverts the swap and destroys the
  temporary mob body. Disclosed simplification vs. Sneezy's real
  `Room::POLY_STORAGE`: the player's original body stays visible in the
  room, tagged "(linkdead)", instead of being pulled into hidden storage.
- **Disguise**: much lighter-weight, purely cosmetic -- toggles
  `short_descr` between empty and "a hooded stranger", reusing
  `cmd_look.c`'s existing short_descr-over-name preference. No descriptor
  swap, no stat transfer (Sneezy's real `DisguiseStuff()` does full
  stat/equipment transfer; disclosed as a deliberately smaller feature).
- **Bug #1 found and fixed (confirmed solid)**: the codebase had NO
  SIGPIPE handler anywhere -- default disposition terminates the whole
  process on any write to an already-closed client socket. Added
  `signal(SIGPIPE, SIG_IGN)` to `crash_handler_install()`. Verified live
  (attaching gdb intercepted a real SIGPIPE that would otherwise have
  killed the server; process survived under gdb, proving the mechanism).
- **Bug #2 found and partially fixed**: a `combat_defeat()` loss for a
  possessed/polymorphed mob, when it fell through to the full PC-death
  pipeline (`descriptor_leave_to_menu()`), corrupted memory -- traced via
  `coredumpctl`/gdb to a SIGSEGV in `descriptor_room_echo()` with a freed/
  reused `d->character`. Fixed by having `combat_defeat()` revert the
  swap, heal the player to half HP, and return immediately for this case,
  skipping the death pipeline entirely (no XP loss/corpse/menu-kick).
  This reduced crash frequency substantially (several consecutive clean
  full runs) but **a later regression run hit the identical crash
  signature again, during what looked like an ordinary, non-polymorphed
  PC disconnect** -- meaning the true root cause may be a more general,
  possibly pre-existing bug in the raw-disconnect path, not fully
  isolated to Transformation. Not fully root-caused before this session's
  effort budget ran out. **Known residual risk, disclosed rather than
  hidden.** Production is currently up and stable on this build (pid
  checked, watchdog cron active as always); Pet/charm and Group
  regression tests pass clean on the same build.
- Also: `disguise` needed `combat_disc_pct` (it's `SKILL_TIER_COMBAT`,
  gated the same as a combat skill) -- test helper `set_caster()` reused
  from the Pet/charm session rather than the immortal-instant-slay
  bypass, since that bypass would prevent testing real pulse-driven
  combat/death paths.
- New `tests/smoke_test_transformation.py`: transform/look/return/
  death-safety for Polymorph, toggle/class-gating for Disguise. One
  assertion ("own body present, marked linkdead") is a known occasional
  timing flake (~2/10 runs) even on non-crashing runs -- matches this
  codebase's existing "rotating sweep flakes -- re-run standalone"
  precedent, documented in the test's own docstring rather than chased
  further.
- Next: keep an eye on production for the disclosed residual crash
  signature (raw-disconnect path); if it recurs, treat it as a
  general/pre-existing bug hunt, not Transformation-specific. Then:
  Crafting & extraction (task 23), Planting (task 34, needs its own
  scoping), the full spell/skill roster import (task 35).

### Session 66 (home): **Pet/charm -- the last
remaining builder-tools-adjacent Sneezy → Tobin audit item to get a full
build in this window -- plus a small "there's new wiznews" login notice,
mirroring the existing news one.**
- **Pet/charm**: scoped via AskUserQuestion -- full pet behavior (follows
  its master room-to-room, assists in combat) across all three fitting
  classes in one pass. Mage's four pre-existing "conjure elemental air/
  earth/fire/water" skill.c placeholders (real Sneezy names/flavor text,
  never wired to anything before this) got a real implementation instead
  of new entries; Cleric's new "summon swarm" and Druid's new "animal
  companion" round out the set. All three reuse real seeded world mobs
  (fire/water/earth/air elementals vnums 16/17/18/19, wolf 570, locust
  swarm 7852) via `being_create_mob()`, same non-new-row pattern
  `cmd_load.c`'s `load mob` already uses.
- **Mechanism**: new `AFFECT_CHARMED` (affect.h) marks a summoned pet and
  times its lifespan (`PET_CHARM_DURATION_ROUNDS`, ~5 real minutes);
  expiry is special-cased in `tick_being_affects()` to actually dissolve
  and `being_destroy()` the pet, not just print a generic "wears off".
  New `being_summon_charmed_pet()`/`being_find_charmed_pet()` (being.c)
  attach it via the EXISTING `master`/`followers[]` fields the Group/
  party system already has -- one pet at a time (refused with a clear
  message otherwise), no new relationship storage needed. New `dismiss`
  command releases a pet early.
- **A real cadence bug found and fixed live**: the first version set the
  pet's "join the fight" pointer from `mob_ai_tick` (~60s wander/scavenge
  cadence) -- live testing showed a pet could sit out nearly a full
  minute of combat before ever engaging, since `combat_process_run()`
  actually resolves every ~1.2s. Fixed by moving both the join AND the
  strike into a new pass appended to `combat_process_run()` itself.
  Deliberately one-sided (the target's own retaliation stays with
  whoever it's really paired with; the pet never draws aggro) -- a
  disclosed simplification of Sneezy's real 3-way combat. A kill the pet
  lands is credited to the MASTER, not the pet, so XP/gold/kill messages
  make sense.
- **A second design gap found while testing the same-session follow-up
  requests** (see below): the pet's fighting pointer used to be cleared
  the instant `master->fighting` didn't match it exactly -- fine for
  auto-assist, but it meant an EXPLICITLY ordered attack (a target the
  master wasn't personally fighting) got silently cancelled the very
  next tick. Relaxed so a pet keeps its own target -- however it was
  set, auto-assist or ordered -- until that target dies/leaves or the
  master says "stop".
- **Same-session follow-up requests, both implemented**: (1) "add a
  trigger for the pet ... react and do whatever [the master] says to
  do, like Master says, 'attack guard' then pet attacks guard" -- new
  `try_pet_command()` (cmd_say.c) listens to whoever's in the same room
  as a charmed pet; "attack"/"kill <target>" sets its fighting pointer
  (reusing `combat_find_room_target()`, same target-resolution `attack`
  itself uses), "stop"/"stay"/"guard" disengages. (2) "master says dance
  pet dances etc" -- anything else is tried as a real social verb via a
  new `social_perform_for()` (socials.c), so the pet can perform any of
  the ~155 real ported social emotes too, not just combat orders. (3)
  "add a chance of failure, confused pet" -- `PET_CONFUSION_CHANCE_PCT`
  (20%) rolled once per spoken command, before interpreting it, so a
  confused pet visibly does nothing rather than doing the wrong thing.
- New `tests/smoke_test_pet.py` (27 checks): summon/cap-refusal/follow-
  through-a-move/combat-assist-visible-message/dismiss-then-resummon for
  Mage, spot-checks for Cleric/Druid, and the full say-command suite
  (attack/stop/dance) with confusion-tolerant retries. A real test-design
  bug caught along the way: `attack`/`kill` gives an IMMORTAL an instant
  slay (cmd_table.c) -- the first draft's mortal-bypass trick (level 51)
  killed the sandbox dummy in one blow before pulse-driven combat (and
  the pet's own strike) ever got a chance to run; fixed by granting
  `player_skill` proficiency directly instead of relying on the
  immortal shortcut, keeping the test character genuinely mortal.
- **wiznews login notice** (user, same session: "add a message like this
  for wiznews as well", pointing at the existing "there is new news!"
  mortal notice): new `wiznews_last_seen_id` player column, `player_get_
  wiznews_last_seen()`/`player_set_wiznews_last_seen()` (mirroring the
  news pair exactly), bumped by `cmd_wiznews.c` on read, checked at login
  gated on `being_is_immortal()` (a mortal can't reach `wiznews` at all).
  Deployed to production, zero build warnings.

Previous update: 2026-07-25 — Session 65 (home): **`edit mob` (medit) --
the last builder-tools-OLC gap -- plus same-session follow-ups: all
three prototype editors auto-create a blank row on a missing vnum,
medit's menu was rebuilt to the user's own wireframe with auto-computed
characteristics, and oedit gained inline value hints plus an `obj`
abbreviation.**
- **`edit mob <vnum>` (medit)**: new CONN_MEDIT_* menu (descriptor.c),
  same snapshot-working-copy shape as `edit object`/oedit. Widened
  `mob_proto_t`/`mob_repo.c` to cover every column the real upstream's
  `send_mob_menu()` (misc/create_mobs.cc) exposes AND Tobin's `mob`
  table actually has. The FIRST implementation (renumbered sequentially,
  paired fields combined onto one prompt each, all 12 upstream stat
  columns shown as "Characteristics") was superseded by a user-supplied
  wireframe paste: final menu is a fixed 23 fields (Name through
  Alignment), prompt is `Mob Editor> ` (not `[medit] `), Sex displays as
  a word ("neuter"/"male"/"female", also accepted as input alongside
  0/1/2), and Faction, Special proc, Local/Adjacent sound, and manually-
  edited Characteristics were dropped from the menu entirely.
  Characteristics (str/con/wis/intel/dex/cha) are now auto-computed on
  Save, not editable at all: `medit_apply_characteristics()` reuses the
  exact same formula `being_create_mob()` uses for live-spawned mobs
  (`ATTR_BASE + level`, capped `ATTR_MAX`, then `class_stat_bonus()`'s
  per-class deltas via a newly-exposed `mob_class_mask_to_tobin()`).
  User asked for race to factor in too ("according to race and class")
  but no race-to-attrs mapping exists yet, so only class contributes
  beyond the level base -- a disclosed gap, not silently dropped. The
  other 6 upstream-only stats (bra/agi/foc/per/kar/spe) still round-trip
  through load/save completely unedited.
- **Auto-create on a missing vnum**: user tested `edit mob 43` live
  (a vnum that didn't exist) and got the expected-at-the-time refusal,
  then said "if one doesn't exist a blank one should be created", then
  "objects and rooms should behave the same" -- widened to all three:
  new `mob_proto_create_blank()`/`obj_proto_create_blank()`, and
  `descriptor_redit_begin()` now falls back to the exact same
  `room_create()`+`room_repo_save()` call redit's own exit-auto-create
  already used for a missing exit target. All three `_begin()` functions
  call the appropriate create-blank helper and re-load before opening
  the editor, rather than returning false.
- **oedit polish (same session)**: `edit obj <vnum>` now works as an
  abbreviation for `edit object <vnum>` (cmd_edit.c dispatch). The
  "Four values" line now shows an inline, type-aware hint of what each
  of the 4 numbers means for the object's current type (weapon dice,
  armor AC, light fuel state, container cap/flags/key, drink/food
  units, etc.) via new `oedit_val_hint()`, reusing the existing
  `category_for_item_type()` classifier with FUEL/BOARD special-cased
  by raw type first.
- **Two real bugs found and fixed within minutes, live**: the mob blank-
  INSERT's column list (43 columns) and its value list were off by one
  (11 `120`s where 12 were needed for the original all-12-stat design) --
  `Column count doesn't match value count`, caught by testing the raw SQL
  directly before it ever reached a rebuild. The exact same miscount
  existed independently in `tests/smoke_test_edmobile.py`'s own fixture
  INSERT (an 11-`120` slip, not copy-pasted from the same bug) --
  caught when the test itself failed with the identical MariaDB error.
  Both fixed by counting columns and values programmatically instead of
  by eye before re-testing.
- New `tests/smoke_test_edmobile.py` (18 checks, rewritten twice to
  track the wireframe correction: menu display sans Faction/manual-
  Characteristics, dirty-before-save, Save auto-computing the 6 real
  characteristics from level+class while leaving the un-edited
  upstream-only stats untouched, auto-create-on-missing-vnum, Discard).
  updated `smoke_test_edobject.py`'s stale "nonexistent vnum is refused"
  check to the new auto-create behavior; confirmed `edit room`/`edit obj`
  on a missing/abbreviated vnum live (no existing test covered those
  paths). Deployed to production, zero build warnings.

Previous update: 2026-07-25 — Session 64 (home): **Score screen revamp
(wireframe layout, class-aware resource label) and `edit trigger` redesign
into a full menu-driven manager, both same-session follow-ups after the
DG Scripts trigger language revamp below.**
- **Score revamp**: user pasted a wireframe directly ("First a revamped
  score ... Colorize tastefully") -- new compact grid (Name/Level/
  Experience, Race/Class/Gold, HP/resource-pool/Move, six stats two-per-
  line, AC/Hand/Sex, Align/Hunger/Thirst, Age, Position), replacing the
  old single-column layout. Age is now real elapsed time converted
  through gametime.h's established mud-year ratio (starts at 17, +1 year
  per 336 mud-days). The HP-row resource-pool field started as a static
  "Mana/Piety: 0" per the wireframe, then two follow-up requests refined
  it: "should either display mana or piety according to class ... maybe
  we should call druid mana Lifeforce (LF)" then "default to mana in non
  magic classes" -- `resource_pool_label()` now shows Piety (Cleric),
  Lifeforce (LF) (Druid), or Mana (everyone else); the underlying VALUE
  stays 0 regardless (no mana/piety/lifeforce pool exists in Tobin at
  all, a disclosed simplification). Colorization ended up deliberately
  minimal -- an earlier draft also tinted labels and HP/Move/Hunger/
  Thirst by state, but colorstring.c's `<tag>` markup becomes real ANSI
  escape bytes in the wire output, and most smoke tests parse `score`
  with plain substring/regex checks that never call `color off` -- an
  escape sitting between a label and its value broke even `"Level: 1" in
  out`. Reverted to just Level's existing immortal-rank tint (always
  empty for the common mortal case, so it never breaks a plain-text
  check). Updated every test asserting score's old field spacing/labels
  (~20 files) -- caught and fixed 4 unrelated stale casualties of the
  2026-07-22 load-to-inventory change along the way (armor/drink/vitals/
  water_drowning_flight tests never got the `drop` call a different-
  character `get` needs), and undid two lines mistakenly touched during
  that pass that actually belonged to the unrelated attribute-allocation
  screen, not `score` (smoke_test_accounts.py/smoke_test_trade_attrs.py).
- **`edit trigger` menu-driven redesign**: user: "edit trigger <room|mob|
  obj> vnum should go into a menu driven editor where you choose type
  with an option to delete the trigger inside the menu -- edit trigger
  list <vnum> should display all three types -- edit trigger delete
  <id|vnum> should work as is." Replaces the old one-shot `edit trigger
  <type> <vnum> <trigger_type> [match|chance]` entirely with a new
  CONN_TRIGEDIT_* menu (descriptor.c), mirroring edsocial's "commits
  immediately, no working copy" shape: a list view (numbered existing
  triggers + "A" to add), a detail view per trigger (match text/chance
  percent edit immediately; "3" opens the script in the shared line
  editor; "D" deletes with a yes/no confirm). New `trigger_repo_get()`/
  `_update_script()`/`_update_match()`/`_update_chance()`. `edit trigger
  list <vnum>` (no target type) now checks room AND mob AND obj at that
  vnum and merges the results; `edit trigger delete <id>` unchanged.
  **Real bug found live** (user reproduced it immediately after this
  shipped: typing a script line like "wait 20" got misparsed as an
  answer to an unrelated chance-percent prompt and the whole trigger got
  silently cancelled) -- root cause: the shared line editor's active-
  editing interception only lives inside `case CONN_PLAYING`, but the
  menu never sets `d->state` back to CONN_PLAYING before arming it, so
  typed lines kept re-entering whatever CONN_TRIGEDIT_* state was still
  set. Fixed with a dedicated `CONN_TRIGEDIT_SCRIPT` state that owns
  `editor_feed()` directly in its own switch case, the same fix shape
  CONN_REDIT_DESC already uses for exactly this reason (room
  descriptions, reached from CONN_REDIT_MENU). New `tests/
  smoke_test_trigedit_menu.py` (24 checks covering the menu mechanics
  themselves) plus conversion of all 3 pre-existing trigger-authoring
  test files (smoke_test_trigger.py/_wait.py/_dg.py) from the old one-
  shot syntax to a shared `author_trigger()` helper -- all confirmed
  passing after the fix.
- Both features deployed to production, zero build warnings, `help
  trigger`/docs/TRIGGER_SCRIPTING.md updated for the new command shape.

Previous update: 2026-07-25 — Session 63 (home): **DG Scripts-style trigger
language revamp (user: "use the DG_* source files to revamp triggers" --
full language port). Also reconciled a stale local git clone (9 commits
behind after other sessions' work landed elsewhere) and fixed two
pre-existing stale-test bugs found along the way.**
- **Full DG Scripts language port, scoped to the LANGUAGE only** (user:
  "stick to the edit unification" -- authoring stays through the existing
  `edit trigger` flow, no separate dg_olc.c-style editor). New
  `trigger_script.h`/`.c` interpreter core, ported from tbaMUD's real
  `dg_scripts.c`/`dg_variables.c` (reference clone at `../tbamud-master/`,
  now gitignored): `%var%` substitution (`%self%`/`%actor%`/`%arg%`/
  `%time%`/`%random.N%` + user-defined locals via `set`), `if <expr>/
  elseif/else/end`, `while <expr>/done`, `switch <val>/case/default/done`
  (real DG fallthrough -- no implicit break), `break`, `set`/`unset`/
  `eval` (locals), `global` (persisted, new `trigger_global_var` table/
  `trigger_var_repo.c`). Tobin's existing fixed action vocabulary (echo/
  echoroom/emote/say/teleport/give/damage/log/wait, trigger.c's
  `trigger_dispatch_action()`) is unchanged in meaning, just now
  `%var%`-substituted and usable inside if/while bodies.
- **Block structure is computed statically from the line array on every
  jump, not tracked on a runtime stack** -- backward/forward bracket
  matching by keyword family (if/end vs. while|switch/done) means a
  `done`/`break`/`elseif` resolves correctly from just its own line index,
  with no call-stack state to snapshot. This is what let `wait` improve on
  its pre-revamp limitation: the full `set`/`eval`/`global` variable scope
  AND the resume point inside a `while` loop now survive the real-time
  pause (only `actor` itself still doesn't, same as before -- may have
  disconnected/moved/died by resume time). `pending_trigger_t` (trigger.c)
  now carries `resume_pc` + a copy of the original script text (so line
  indices stay stable) + the variable snapshot, replacing the old "raw
  remaining text" scheme.
- NOT ported (disclosed scope cuts): DG's full mob/obj/room command sets
  (`dg_mobcmd.c`/`dg_objcmd.c`/`dg_wldcmd.c` -- hundreds of DG-specific
  script commands), `remote`/`context`/`attach` (multi-script targeting),
  and `dg_olc.c` (a separate numbered-script-per-vnum authoring model --
  Tobin keeps one script per `edit trigger` row). `eval`'s arithmetic is
  left-to-right with no operator precedence (disclosed simplification,
  matches how the vast majority of real DG scripts use it anyway --
  single binary op). Trigger TYPE roster (room enter/random; mob greet/
  speech/death/random; obj get/wear) is unchanged -- this revamp is the
  language underneath, not new firing hook points.
- Verified live via a new `tests/smoke_test_trigger_dg.py` (38 checks: if/
  else branch selection, while/eval loop counts, switch/break vs. real
  fallthrough, `%actor%`/`%self%`/`%arg%` substitution, `global`
  persistence across two DIFFERENT mobs' triggers, and the wait-preserves-
  variables improvement) plus a full regression pass of the pre-existing
  `smoke_test_trigger.py`/`smoke_test_trigger_wait.py`.
- **Found and fixed along the way**: `smoke_test_trigger.py`'s obj get/wear
  sub-tests were a stale casualty of the 2026-07-22 "`load` now puts the
  item in the loading immortal's own inventory" change -- never got the
  `drop` calls the OTHER three affected test files received that session
  (flagged then as "the other ~55 test files using `load obj` weren't
  audited"). Fixed here since it surfaced live as a false regression signal
  while validating this session's actual change.
- **Local git hygiene**: this session's local `E:\New MUD` clone had drifted
  9 commits behind `origin/main` (oedit, sign language, drug tracking, and
  a death-message colorization polish pass all landed via other sessions'
  clones in the interim) -- reconciled via stash + fast-forward pull,
  confirmed the stashed working-tree edits were byte-identical (modulo
  CRLF) to what was already committed, then dropped the stash. No real
  work was at risk, just a stale local HEAD (same class of issue as
  Session 61's Work-box reconciliation).
- Deployed straight to production (preview retired per user, 2026-07-22).
  Exact-PID kill + relaunch (pid 3769), confirmed no duplicate process from
  the watchdog cron racing the restart window. Zero build warnings on a
  full clean rebuild.
- News/wiznews: builder-only change (trigger authoring), no player-facing
  news entry -- matches oedit's own precedent.

Previous update: 2026-07-24 — Session 62 (home): **Finished an in-progress,
uncommitted death-message colorization + flavor-taunt change left over from
a prior session, plus cleanup and two unrelated stale-test bugs found along
the way.**
- **Death-message colorization, finished + cleaned up**: found `combat.c`/
  `descriptor.c` sitting uncommitted with the DEAD in a death message
  wrapped in `<R>` (bright red) and the account menu's N/D/X/Q letters
  wrapped in `<C>` -- plus several new death-taunt flavor lines already
  added to `DEATH_TAUNTS[]`. Changed `<R>` -> `<r>` (dim red) to match the
  codebase's own convention for this class of alarm text (`PANIC!`,
  shutdown notices, `wipe` all use dim `<r>`; bright `<R>` is reserved
  elsewhere for things like worst-tier object condition). The account
  menu's `<C>` letters were already consistent with that same menu's
  existing style elsewhere in the file, so left alone. Fixed two typos in
  the new taunt lines (perrished -> perished, daisys -> daisies) and
  trailing whitespace on one line.
- **Two unrelated pre-existing test bugs found and fixed while verifying
  live**: `smoke_test_kill.py`'s mortal-vs-mortal Part 1 never called
  `toggle pk` -- a gate added by a later PK opt-in feature;
  `smoke_test_combat.py`'s identical PvP setup had already been fixed for
  this in a past session but the fix was never propagated here. Separately,
  `smoke_test_kill.py`/`smoke_test_combat.py`/`smoke_test_menu_letters.py`
  all had raw-substring assertions ("You are DEAD!", "N create") that broke
  once those two spots started wrapping text in color tags -- fixed by
  turning color off in the two combat tests' player setup (neither test is
  about color) and relaxing the menu-letters check to the label text
  instead of the letter+label pair (that test deliberately runs with color
  on, by design). `smoke_test_accounts.py` failed on the same run too, but
  that's a third, separate, unrelated pre-existing bug (the character list
  has been hidden by default since Session 47 and this test never presses
  `C` to reveal it) -- reproduced identically via `git stash` against the
  pre-patch build to confirm it predates this session's work; left alone,
  out of scope.
- Clean rebuild (`rm -rf build`), zero warnings. Confirmed live:
  `smoke_test_kill.py`, `smoke_test_combat.py`, `smoke_test_crit.py`,
  `smoke_test_menu_letters.py` all pass. Full regression sweep not run
  (user: skip it, just commit).
- News + wiznews entries added per house rule (small player-facing
  cosmetic change, still gets both).
- Next: same open items as before -- `edmobile` is next up per TODO.md.

Previous update: 2026-07-22 — Session 61 (work): **Repo hygiene (Work box's
git bookkeeping reconciled with origin/main -- no real work was at risk,
just a stale local HEAD) + root-caused and fixed
`smoke_test_object_maintenance.py`'s real, reproducible hang (flagged by
Session 60 as "hasn't gotten a single clean confirmed pass") + `edit
object` (oedit), TODO.md's "NEXT UP" item and the last piece of the
builder-tools-OLC audit gap + a stale test assertion found and fixed
along the way.**
- **`edit object` (oedit)**: menu-driven object-prototype editor over the
  existing upstream-seeded `obj` table -- same snapshot-working-copy
  pattern as `edzone`/`edplayer` (obj_proto_load()/obj_proto_save(), new
  CONN_OEDIT_* state machine in descriptor.c). Field numbering/labels
  ported from the real upstream's own `update_obj_menu()`
  (misc/create_objs.cc), renumbered sequentially (1-17) rather than
  preserving its 1-8/10-21 gaps -- Tobin's other editors don't preserve
  upstream slot numbers either. EDIT-ONLY, same scope boundary `edroom`
  draws around rooms -- no way to allocate a brand-new vnum here (a
  separate concern, like `dig` is separate from `edit room`). Take
  Flags/Extra Flags (wear_flag/action_flag) open a toggle-by-number
  submenu, same shape as edroom's room-flags submenu -- new
  `obj_wear_flag_count()`/`_name()` and `obj_action_flag_count()`/`_name()`
  accessor pairs (obj.c) over the existing display-only name tables.
  `obj_proto_t` gained `vnum`/`action_flag`/`spec_proc` (previously
  load-only, now also editable/round-tripped). Three real upstream fields
  disclosed as OUT of scope, not silently dropped: `action_desc` (the
  real menu labels this slot "9) Unused" with NO case in its own
  dispatcher either -- genuinely not exposed there, not a Tobin
  omission), Extra Description (needs an objextra-style per-object table
  Tobin doesn't have, unlike rooms' roomextra), and Applys (the objaffect
  stat/AC bonus rows -- its own related-table submenu in the original,
  also wizpower-gated at 53+, a granular permission system Tobin doesn't
  have and isn't replicating as a separate gate). No `zone_can_edit()`
  check, unlike edroom/edzone -- the `obj` table has no zone column at
  all, so there's no ownership boundary to enforce; gated at
  BUILD_MIN_LEVEL only. New `tests/smoke_test_edobject.py` (17 checks,
  verified live against a real sandbox prototype row): menu display
  (including the real seeded wear_flag/weight rendering correctly),
  dirty-before-save, flag toggling round-tripping through the submenu,
  four-values editing, Save persisting everything in one write, a
  nonexistent-vnum rejection, and Discard genuinely discarding. New `edit
  object` help topic; wiznews entry (builder-only feature, no player-
  facing news entry). Regression swept clean: edzone/redit/edplayer/
  balance/stat/objects/object_maintenance all still pass after the shared
  `descriptor_in_editor()`/`handle_line()` changes.
- **Stale test assertion found and fixed along the way (unrelated to
  oedit)**: `smoke_test_help_topics.py`'s own `gust` check still expected
  the OLD generic "keyworded component" Requires wording from before
  Session 60's later same-session follow-up (real per-spell component
  mapping) shipped -- confirmed the committed `skill_help.sql` already
  has the real wording ("a rabbit's foot on a silver chain"), so this
  wasn't a regression from anything in this session, just a test
  assertion nobody updated after that follow-up landed. Fixed to check
  for the real, already-shipped wording instead.
- **Repo hygiene**: `git status` on db.kullit.com showed ~200 modified +
  30 untracked files against its own stale local HEAD (`a7a00c8`, several
  commits behind `origin/main`). Investigated before touching anything --
  confirmed via a full tree diff against the already-current Windows dev
  tree that content matched origin/main almost exactly (only this
  session's own in-progress TODO.md edit differed); the box's checked-out
  HEAD was simply never advanced after `origin/main` moved on (drug
  tracking, skill-help redesign, etc. already landed there from
  elsewhere). No commits existed on the box that weren't already on
  origin (`git log origin/main..HEAD` empty) -- safe to reconcile.
  `git reset --hard origin/main && git clean -fd` on the box brought its
  bookkeeping back in line with zero content loss. Lesson: a big
  `git status` diff on a build box doesn't necessarily mean real
  unsynced work -- check against origin, not just assume.
- **`smoke_test_object_maintenance.py` hang, root-caused**: reproduced
  live (server stays at 0% CPU, still accepts new connections, and its
  own `pulse_count` keeps advancing normally under `gdb` the whole
  time -- ruled out a server-side deadlock/spin entirely). The test's own
  `recv_all(imm, 1.5)` (its combat-wait poll loop, right next to a
  correctly-tuned `recv_all(tgt, 0.3)`) uses a timeout LONGER than
  `COMBAT_ROUND_PULSES`'s real cadence (12 pulses @ 100ms = ~1.2s,
  pulse.h): since the test deliberately sets the attacker's dexterity to
  900 against a dex=1/999999-HP target specifically so combat lands
  (almost) every round, the fight generates a fresh message on `imm`'s
  socket roughly every 1.2s -- faster than the 1.5s window `recv_all()`'s
  internal "drain until a quiet gap" loop needs to ever see a native
  `socket.timeout`. The single initial `cmd(imm, f"hit {tgt_name}")` call
  (and, if it had ever gotten that far, the while-loop's own
  `recv_all(imm, 1.5)`) would drain forever, since the fight never
  naturally ends (999999 HP). Confirmed with `gdb -p <pid> bt full` (had
  to `kill` the existing passive crash-watcher gdb first --
  `ptrace`  only allows one tracer -- then reattached it afterward, same
  recipe as CLAUDE.md's standing instruction). Fixed by lowering `imm`'s
  poll timeout to 0.3s (matching `tgt`'s already-correct value) and the
  initial `hit` send's own timeout to 0.3s too -- both now safely below
  the round cadence. Verified live end-to-end: full test now passes
  clean in ~72s (`ALL CHECKS PASSED`). Cleaned up leftover `Obj%`
  sandbox rows left behind by the hung runs. This was a genuine
  pre-existing test-script bug (present since whenever this test was
  written), not a regression from any recent session's server-side
  changes.

Previous update: 2026-07-22 — Session 60 (work): **Skill/spell help topics
redesigned (user wireframe, "gust" worked example) + Sign language
(Sneezy → Tobin feature audit) + Drug tracking (Sneezy → Tobin feature
audit) + a same-session regression sweep of the
`load obj` → inventory change (Session 59, home) that had gone un-audited across the
rest of the suite.**
- **Skill/spell help redesign**: user handed a wireframe using the live
  `help gust` output as the worked example, with inline notes on what to
  fix: a more descriptive body (area-effect vs. single-target), a bare
  `Classes:` list (no `(Class, level 1)` parenthetical), a real
  `Syntax:` line, `Requires:` naming the actual component instead of a
  vague phrase, and a new `Approx. Level:` footer. Then a follow-up ask:
  "line up the : to make it more readable and colorize appropriate."
  Rebuilt `skill_help.sql` (275 rows) with a one-shot Python generator
  (not committed, matching the file's own existing convention) that
  parses `skill.c`'s `SKILLS[]` roster directly and categorizes each
  entry with the EXACT SAME substring rules `cmd_cast.c`'s `task_cast()`
  uses to dispatch the real effect (heal/cure/ward/area/damage/
  uncategorized), so the help text can't silently drift from what a
  spell actually does. `Requires:` now names a real, concretely-seeded
  example (`a pouch of spell components` / `a wooden holy symbol` --
  both genuinely common seeded rows, confirmed live, not invented).
  **Follow-up ask, same session**: "this should list the actual
  component required for the spell, do this for all skills/spells that
  require components or holy symbols." Found the real per-spell binding
  in the bundled upstream source: `obj/obj_component.h`'s own `const int
  COMP_<SPELL> = <vnum>;` table (124 entries) names the SPECIFIC themed
  reagent each spell traditionally used, not a generic placeholder.
  Cross-referenced against `skill.c`'s own names (accounting for the
  original's abbreviation style -- `CLOUD_OF_CONCEAL` -> "cloud of
  concealment", `DETECT_INVIS` -> "detect invisibility", etc.) and
  confirmed every match against Tobin's real seeded `obj` table live --
  56 solid matches (`hellfire` -> "some liquid brimstone", `feathery
  descent` -> "an orange pigeon feather", ...), each a real row, not a
  guess. Every OTHER Mage/Druid spell (the upstream never assigned
  these a specific vnum either -- ordinary cantrips always drew from
  the same generic pool) honestly keeps the generic phrasing rather
  than fabricating a specific item. Cleric's holy-symbol Requires stays
  generic throughout -- the upstream's Ritualism component system never
  assigned individual holy items per-prayer the way Wizardry did per-
  spell, so there's no equivalent real binding to port. The generator
  now queries the live DB directly (one `mariadb` call, `subprocess`)
  to pull each matched vnum's real `short_desc` rather than hardcoding
  151 lines of Tobin-side item text that could drift from the DB.
  `cmd_help.c` gained two new trailing-directive footers (`Approx.
  Level:`, `Classes:`) rendered in the SAME cyan/14-char-aligned style as
  the existing `Syntax:`/`Requires:`/`Related:` labels, pulling `Classes`
  out of body prose entirely so every label's colon lines up in one
  column. Two real generator bugs caught before shipping, not after:
  (1) universal skills (`riding`/`sign`, duplicated onto every class
  including Mage/Cleric) were being misread as cast/pray-reachable
  purely because a caster class happened to list the name -- fixed by
  checking for a real dispatch-table route FIRST, before any class-based
  inference; (2) a physical Warrior skill's own flavor text ("a burst of
  offense") false-matched the damage category on the word "burst" --
  fixed by gating spell categorization on the entry actually having a
  Mage/Druid/Cleric class at all, matching that `task_cast()`/
  `task_pray()` never even run for a name no caster class lists. A third
  bug found only by checking the LIVE database, not just the generated
  file: the file's own `ON DUPLICATE KEY UPDATE name=name` (the correct,
  standing no-op convention for every other seed file that only ever
  INSERTs brand-new topics) silently prevented this REFRESH of already-
  seeded rows from ever taking effect -- fixed with `ON DUPLICATE KEY
  UPDATE body = IF(updated_by='seed', VALUES(body), body)`, refreshing
  only rows never hand-edited via `hedit` (confirmed none of the 275
  names collide with a real `hedit`-edited row before applying).
  `tests/smoke_test_help_topics.py` had two assertions hardcoded to the
  old wording -- updated to match, all `help*` tests (`help`, `help_
  content`, `help_format`, `help_topics`) pass clean.
- **Second follow-up, same session**: "is there a list of components to
  spells from the code we could map along with the % of discipline
  needed". Both real, both much bigger than what shipped first.
  **Components**: `misc/spell_num.cc`'s `mapFileToSpellnum()` (~500
  entries, ported into `file_to_spell.py`) maps a component object's raw
  file-format "value 3" field to its real spell -- and Tobin's own
  `obj.val2` preserves that exact field verbatim (confirmed live:
  `val0`/`val1` are the already-documented charge pair, `val2` varies
  meaningfully per real component, `val3` is something else, consistent
  with `assignFourValues(x1,x2,x3,x4)`'s real argument order). The
  generator now queries EVERY real "component"-keyword row in Tobin's
  `obj` table, maps each one's `val2` through this table, and keeps the
  lowest vnum per spell (matching the original's own `CompInfo.comp_num`
  "lowest vnum wins" convention) -- 83 real matches, up from the first
  pass's 56 (`gust` went from the generic fallback to a real "a rabbit's
  foot on a silver chain"). **Discipline %**: `misc/spell_info.cc`'s
  `discArray[SPELL_X] = new spellInfo(..., START_n, ...)` -- `start` is
  a genuine 0-100 threshold checked directly against a live discipline
  percentage in `misc/gaining.cc`, not a guess. Real complication,
  surfaced before implementing rather than after: the original splits
  casters across several separate per-school disciplines (Air/Fire/
  Water/Mage/Ritualism/...), each its own 0-100 track, while Tobin
  collapsed all of that into one `basic_disc_pct` + one
  `advanced_disc_pct`. Asked the user how to reconcile a raw "START_26"
  against that mismatch rather than silently picking an interpretation;
  they chose showing the real upstream value as-is. New `Discipline:`
  footer line (skill/spell topics only, shown only when the upstream
  source actually assigned this spell a real value -- 248 of 275 do) in
  `cmd_help.c`, same cyan/aligned style as every other label, peeled as
  a fifth trailing directive (Classes -> Discipline -> Approx. Level ->
  Related -> Requires, bottom-up -- peeling a directive that isn't
  actually present is a harmless no-op, so this fixed order is safe
  whether or not a given topic has one). Both new data sources
  (`file_to_spell.py`, and a direct `discArray[]` regex parse of
  `spell_info.cc`) are additions to the SAME one-shot generator script,
  not new files kept in the repo.
- **Regression sweep first**: pulled Session 57-59's work (repair-shop
  economy, banking, material properties) in; Session 59's own STATUS.md
  entry had explicitly flagged "~55 other test files that also call
  `load obj`... not audited" as a real risk. Ran the targeted set already
  identified as touching the changed files (`skillcombat`, `objects`,
  `object_maintenance`, `repair`, `bank`, `material`) and found exactly
  that risk materializing twice, live: `smoke_test_objects.py` (three
  `load obj` call sites that expected the item on the room floor, one of
  them a cross-character load-by-immortal/get-by-victim pattern) and
  `smoke_test_skillcombat.py`'s disarm test (same cross-character pattern
  -- an immortal `load obj`s a sword, a DIFFERENT Warrior character was
  expected to `get` and wield it for the disarm check; the sword now sat
  in the immortal's own inventory instead, so the "victim" never had a
  weapon at all, and the disarm attempt failed with "they aren't even
  holding a weapon" instead of ever reaching the real proficiency roll).
  Fixed both test scripts with an explicit `drop` after each cross-
  character `load obj`, same fix Session 59 itself already applied to
  `smoke_test_repair.py`/`smoke_test_object_maintenance.py`. Both pass
  clean now; `repair`/`bank`/`material` were already clean.
- **A real, currently-live production issue found and resolved along the
  way, unrelated to sign language**: multiple past test runs (material,
  repair, and this session's own first two `object_maintenance` re-runs)
  left the server logging `foreign key constraint fails` on `player_
  attrs`/`player_progress`/`player_inventory` every ~60s, indefinitely --
  an orphaned in-memory connection whose underlying `player` row had
  already been deleted by that same test's own cleanup, being retried on
  every autosave tick. Matches Session 59's own "17-minute hang" root
  cause exactly (test process hangs or gets force-killed before its own
  `finally`-block socket close runs), confirming the "close in finally"
  fix from that session's `93e7080` doesn't fully close the gap -- a
  force-killed (`kill -9`) or genuinely-hung process never reaches its
  own `finally` block at all, hang or no hang. Resolved by killing the
  stuck test PIDs directly (same remedy Session 59 found) -- the FK-error
  spam stopped within one autosave cycle, no server restart needed.
  Leftover fixture rows (two full sets of `object_maintenance` rooms/
  objs/players, one from a run killed for hanging ~22 minutes) cleaned up
  by hand. **Not fixed at the root**: there's still no way to force-
  disconnect a stuck in-memory descriptor short of a full server
  restart -- Session 59 already flagged this exact gap ("no admin
  'disconnect a stuck connection' command exists yet"); still open.
  `smoke_test_object_maintenance.py` itself still hasn't gotten a single
  clean confirmed pass this session (both attempts here hung and were
  killed) -- carried forward, not this session's own regression, but
  worth another attempt before it's trusted again.
- **Sign language**: checked the real upstream first (`docs/systems/
  important/communication-system.md`'s "Sign Language Reception"
  section, `misc/talk.cc`'s `doSign()`): silent, room-only speech that
  only a fellow `SKILL_SIGN` holder actually reads -- everyone else sees
  a generic "makes funny motions with hands" line, except a Thief
  signer (a real, deliberate stealth-class exemption in the original:
  hand-talk is common underworld knowledge, read by anyone regardless of
  their own skill). New `sign <message>` command (`cmd_sign.c`) and a new
  `sign` skill added IDENTICALLY to every class's roster (`skill.c`) --
  the original lists it under `DISC_ADVENTURING`, a general skill every
  class gets, not a per-class one, so duplicating one entry across every
  class table is the same "genuinely universal skill" precedent `riding`
  already established, not a new pattern. Gating: not fighting, not
  asleep, both hands empty, neither arm at the real `limb_status_text()`
  "hurt" threshold (<20%) or worse. **Deliberately not ported**: the
  original's exact `POSITION_CRAWLING` minimum-position check -- Tobin's
  `position_t` is never actually driven to CRAWLING/ENGAGED/FIGHTING by
  anything today (position stays STANDING while fighting; "fighting" is
  derived from the separate `fighting` pointer, per the Combat decision
  row), so porting that literal enum comparison would have been a silent
  no-op at best -- and garble/drunk speech distortion.
- Command-table placement: inserted right after `sip`, deliberately NOT
  before `sit` -- an existing comment on `sit`'s own table row already
  reserves the "si" abbreviation for it specifically ("SWAP: sit before
  sip, so 'si' sits"), so `sign` had to land after both to avoid stealing
  it; "sig" is already unambiguous on its own.
- **Own test-design bugs hit while verifying, both fixed, both now
  disclosed in the test's own comments**: (1) tried the fighting-gate
  check immediately after `attack`, which collided with `cmd_dispatch()`'s
  own global wait-state gate (attack sets `COMBAT_ROUND_PULSES` of lag on
  the attacker) -- same trap `smoke_test_skillcombat.py`'s own
  `attack_and_settle()` helper already documents; fixed with the same
  sleep. (2) `attack` silently no-oped ("They aren't here.") because
  mortal-vs-mortal combat requires BOTH sides opted into PK
  (`combat_pk_allowed()`, combat.c) -- missed on the first two attempts,
  root-caused by reading `combat_find_room_target()` directly rather than
  guessing further; fixed with `toggle pk` on both test characters,
  matching a requirement `smoke_test_skillcombat.py`'s `make_pair()`
  already names in its own docstring.
- New `tests/smoke_test_sign.py` (12 checks: no-discipline refusal, empty
  message, a fellow signer reading the real message, a non-signer seeing
  the generic line, the Thief stealth exemption reaching a non-signer
  too, and all four gating refusals -- fighting, hands full, a hurt arm,
  asleep). New `sign` help topic; `news.sql`/`wiznews.sql` entries
  (player-visible: a new command anyone can use).
- **Drug tracking** (Sneezy → Tobin feature audit): scoped via
  AskUserQuestion -- user picked "full system, remapped stats" over a
  smaller consumption-only slice. Checked the real upstream first
  (`docs/systems/informational/drug-tracking.md`, `obj/obj_component.h`'s
  `TDrug`/`TDrugContainer` split): consumption applies a real temporary
  stat effect, tracked for addiction (lifetime average consumption rate)
  and withdrawal (a real penalty once overdue past a per-drug onset).
  The original's own drug effects lean on BRA/AGI/FOC/SPE/PER/KAR -- six
  attributes Tobin's simplified STR/DEX/CON/INT/WIS/CHA system doesn't
  have at all (the same already-documented gap as the Magic Items
  session) -- so every effect is a deliberate remap (SPE→DEX, FOC→INT,
  KAR→WIS; STR/CON/CHA exist verbatim), not a literal port. Two
  deliberate non-ports, disclosed in `drug.c`'s own comments: (1) Opium's
  real upstream effect is documented as outright buggy (checks one stat,
  sets another) -- a clean, internally-consistent penalty used instead;
  (2) Frogslime's real GARBLE (speech-scrambling) effect isn't ported --
  Tobin has no drunk/garble-speech mechanic anywhere yet (a separate,
  bigger lift) -- kept as flavor + a real chance of `POSITION_SLEEPING`
  instead. New `being_t.drugs[DRUG_COUNT]` array (a dedicated
  `drug_state_t`, not reusing `active_affect_t` -- that struct is only
  `{type, rounds}`, no generic per-instance stat-delta storage). Two
  different time representations, deliberately: `first_use`/`last_use`
  are real wall-clock `time(NULL)` (same convention `player.birth_time`
  uses) so withdrawal is testable by SQL-seeding a fake `last_use` far in
  the past and forcing one `aitick`; an active dose's own effect window
  is instead a tick countdown (`effect_ticks_left`, same convention
  `CORPSE_DECAY_TICKS` uses) so dose-expiry is ALSO `aitick`-forceable --
  a wall-clock expiry was the first design attempt, refactored away once
  it became clear it couldn't be tested without a real ~2-minute wait,
  unlike every other Tobin decay system (`pulse_current()` was found not
  to exist anywhere in `pulse.h`, which is what prompted the tick-count
  redesign in the first place). New `smoke <item>` command
  (`cmd_smoke.c`) -- drug items identified purely by the keyword "drug"
  (same generic-by-keyword convention spell components/holy symbols
  already use), `val0`=`drug_type_t`, `val1`/`val2`=current/max charges,
  spending a charge and destroying the item at 0 (same lifecycle
  `consume_component()`/`consume_symbol()` already use). Real
  `ITEM_TYPE_NAMES[]` index 56 ("DRUG") used for the four new seeded
  items (vnums 90010-90013) after an initial mistaken attempt at type=9,
  caught by checking `obj.c`'s real table before shipping. New
  `player_drug` table (Tobin-specific, `player_id`+`drug_type` key),
  loaded on login alongside `player_attrs`/`player_progress`, saved on
  each `smoke`. New `tests/smoke_test_drugs.py` (10 checks: non-Hobbit
  penalty, dose consolidation not stacking, `aitick`-forced dose expiry,
  Hobbit bonus, low-charge item destruction, and a SQL-seeded overdue-
  and-addicted withdrawal case) -- one real bug in the test itself, not
  the mechanic: the first seeded `total_consumed=60` produced a
  withdrawal rate of ~1.94/hour, just under pipeweed's own 2.0/hour
  addiction threshold, so the check never fired; bumped to 100
  (~3.23/hour) for a comfortable margin, verified against a live run.
  Regression pass (`drugs`, `objects`, `skillcombat`, all four `help*`
  suites) all pass clean -- `skillcombat`'s own run looked hung past a
  300s timeout on first attempt, but a longer run showed it's just
  genuinely slow end-to-end (~5m50s for the full file, this box, this
  session), not a real hang; not a regression from this change. New
  `smoke` help topic; `news.sql`/`wiznews.sql` entries (player-visible: a
  new command).

Previous update: 2026-07-22 — Session 59 (home): **Material property system
(Sneezy → Tobin feature audit, task #24) + a cluster of player-facing
polish requests that came up live while testing it: real condition-text
wording/colors, qualitative combat-hit intensity everywhere, `load obj`
landing in inventory, and three "applied live only" DB content
regressions found and fixed for good.**
- **Material property system**: checked the real upstream first
  (`misc/materials.h`/`.cc`, `docs/systems/informational/
  material-system.md`) — its own doc claims direct weapon-damage/AC
  multiplier formulas that do NOT actually exist in the shipped code;
  what's real is durability (mutual hardness-vs-hardness wear) and value
  (a flat weight × material-price lookup). Asked before building rather
  than assuming: scoped as "durability + value only" (faithful) vs. "also
  add damage/AC multipliers" (a disclosed Tobin invention going further
  than the real upstream) vs. skip. User picked the invention. Reused
  Tobin's EXISTING real seeded `obj.material` column (populated since
  earlier object-affects work, never mechanically read until now) rather
  than adding a new field — bucketed the 83 real `MAT_*` IDs into 5
  Tobin-scale tiers (Common/Fine/Superior/Rare/Legendary, new
  `material.c`) instead of porting all 83, matching the audit's own "3-5
  tiers" sizing call. Each tier: damage/AC multiplier (folded into
  combat.c's existing gamewide `dmg_mult` and `obj_armor_ac()`), a
  `max_struct` bonus at creation (feeds the repair-shop economy's
  ceiling too), and a shop value multiplier. New `tests/
  smoke_test_material.py` (12 checks: exact AC/structure-bonus math, a
  statistical damage-multiplier check, a real-shop value-multiplier
  check with the item's material temporarily bumped and reverted).
- **Condition-text wording + colorization**: user, after seeing my own
  invented 6-tier wording in a test run: "put the condition of items
  after the short desc. search sneezy for 'like new'." Found the real
  `TObj::equip_condition()` (misc/info.cc) — an 11-tier ladder with real
  per-tier ANSI colors (`<C>brand new<1>` down to `<r>destroyed<1>`),
  ported verbatim rather than guessed at a second time. New shared
  `obj_condition_word()` (obj.h/.c), shown inline right after an item's
  short_descr in `inventory`/`equipment` (parens, e.g. "a long sword
  (<C>brand new<1>)") and as a new `Condition:` row in `identify`.
  Confirmed live: real ANSI escapes render (`\x1b[1;36mbrand new\x1b[0m`).
- **Combat messages: qualitative intensity, not raw numbers**: the
  2026-07-12 "don't report damage" decision only ever covered
  mortal-visible melee text — an immortal-visible melee branch AND every
  spell/trap/wand-staff damage message still printed the raw number.
  User: "take out the damage number and use it to describe how hard the
  hit was" (pointing at one specific immortal-visible melee line);
  confirmed via `AskUserQuestion` that this should apply everywhere, not
  just melee. Ported the real upstream's own `describe_dam()`
  (misc/combat.cc) — an 11-tier ladder ("pathetically" through "into
  shreds"/"into a bloody pulp"), damage compared against the struck
  limb's CURRENT pre-hit HP (not max), so the same raw number reads more
  brutal against an already-battered limb. New shared `describe_dam()`
  (combat.h/.c) reused across combat.c/cmd_cast.c/cmd_pray.c/
  cmd_move.c/cmd_use.c. Broke `smoke_test_material.py`'s own
  damage-multiplier measurement (it parsed "for %d damage!" text, now
  gone) — fixed by reading `tgt`'s own `score` HP line before/after a
  fixed real-time combat window instead.
- **`load obj` → inventory, not the room floor**: user request, simple
  on its face, but revealed a real wide-reaching test-suite risk once
  actually changed: several existing tests load an item as one immortal
  and `get` it as a DIFFERENT character (the real test subject) — with
  items now landing in the LOADING immortal's own inventory, the other
  character's `get` fails outright. Fixed the two tests this directly
  broke (`smoke_test_repair.py`, `smoke_test_object_maintenance.py`, both
  now `drop` explicitly before the other character `get`s). **Not**
  audited: roughly 55 other test files that also call `load obj` —
  flagged as a real risk for the next full sweep rather than assumed
  fine.
- **Three "applied live only" DB regressions, found and actually fixed
  this time**: while picking the bank's real seeded room (Session 58),
  found it still said "Grimhaven First Kingdom Bank" despite a "done"
  TODO.md entry for a global Grimhaven→Tobin City rename. Investigated:
  639 rooms on Home's own database still said "grimhaven" — the original
  fix was applied as one-off live SQL against a single running instance,
  never captured in `tobin_migrations.sql`, and Home/Work each run their
  own independent database despite sharing git-synced code. Re-applied
  live AND (new this time) captured as idempotent migrations so it can't
  silently regress again. User separately flagged a second instance live
  ("That'll be 198 talens. should be gold") — same root cause,
  talens→gold (263 shop rows, also "done" in TODO.md, also never
  migrated). Proactively swept for a third: SneezyMUD→TobinMUD (4 rows,
  same pattern). All three now live in `tobin_migrations.sql` as
  idempotent `REGEXP_REPLACE`/`REPLACE` statements. Work box's own DB
  state for these three remains unconfirmed.
- **Real incident, mine, mid-session**: a process-restart command
  (`kill 16005; ... TOBIN_PORT=4003 ...`) accidentally also matched and
  killed PRODUCTION (a broader pattern than intended) — the cron
  watchdog auto-restarted it cold (no copyover) within a minute, so
  anyone connected got a hard disconnect rather than a graceful
  reconnect. Disclosed immediately; user said not to worry about
  reboots at this stage of development ("just do it, dont worry about
  players"). Since then: killing by exact PID only, never by
  port-matching pattern, and (per user, same conversation) preview
  retired entirely — "stop running preview, do all work on production."
  A second, separate incident: a regression-test rerun hung for 17
  minutes (not the expected ~90s) — root-caused to a pre-existing
  test-script hygiene gap (sockets never closed in a `finally` on
  assertion failure), which left the server retrying a failed autosave
  for an orphaned-but-still-connected test character every ~60s after
  the test's own cleanup had already deleted its `player` row out from
  under it. Killing the stuck test process resolved it immediately (no
  server-side bug) — not yet fixed at the test-script-hygiene level
  across the suite, just diagnosed and worked around this session.
- Verified live end-to-end on production (all of skillcombat/objects/
  object_maintenance/repair/bank/material re-run clean after every
  change in this cluster, not just the newest one).

Last updated: 2026-07-21 — Session 58 (home): **Money system v2
(banking/taxes), Sneezy → Tobin feature audit — same session as #57,
picked next per the user's stated order (22 then 24).**
- **Scope confirmed via `AskUserQuestion` before building** (this
  session's habit, same as the repair-shop's "full system" pick):
  checked the real upstream first (`spec/spec_mobs_banker.cc`,
  `misc/shopowned.cc`/`shopaccounting.cc`,
  `docs/systems/critical/17-economy-system.md`) -- per-shop bank
  accounts, a fractional-reserve central bank (regular banks must hold
  `total_deposits * centralbank.profit_buy` in reserve, withdrawals
  that would violate it are rejected), and sales tax that's actually
  scoped to player-OWNED shop transactions only, routed to a per-shop
  tax office and journalized through a genuine chart-of-accounts
  double-entry ledger (`shoplogjournal`, debit/credit pairs, COGS
  tracking, year-end book-closing). All of it entangled with a
  player-owned-shop/corporation economy Tobin doesn't have and has no
  plans for at its current population. Asked two questions rather than
  guessing: tax destination (sink vs. a global treasury vs. skip tax
  entirely) and bank scope (one global bank vs. per-shop accounts).
  User picked **global treasury** and **single global bank**.
- **Banking**: new `player_progress.bank_gold` (a second wallet,
  alongside `gold`) — `bank` / `bank balance` / `bank deposit <amt>` /
  `bank withdraw <amt>` (new `cmd_bank.c`), usable only at a real shop
  flagged `is_bank` (same genuinely-new-column precedent as
  `is_stable`/`is_repair`). Picked shop_nr 4, "Tobin City First Kingdom
  Bank" (room 31750, keeper "banker Tobin City") over 5 other
  bank-themed seeded rooms found in the data (Brightmoon Bank, The
  Logrus Bank, Second Bank of Amber, Banking Window, A Marshy Bank) —
  most of those have `keeper == in_room` in the seeded data, which
  looks like broken/unset import data rather than a real mob reference;
  only shop_nr 4 and 123 have a keeper vnum genuinely distinct from
  their room, and 4's name fits "the single central bank" framing best.
  Interest: 0.5% once per real in-game day (`bank_interest_tick()`, new
  `bank.c`), tracked via a composite `year*12*28 + month*28 + day` key
  against `gametime.h`'s existing calendar (`gametime_day()` alone is
  only 0-27, not unique across month/year rollover) rather than adding
  any new day-rollover hook to the calendar system itself. Applied as a
  single SQL `UPDATE player_progress SET bank_gold = bank_gold +
  FLOOR(bank_gold * 0.005) WHERE bank_gold > 0` — deliberately NOT a
  per-online-character loop, so offline balances accrue interest too,
  matching the real upstream's own daily interest job semantics without
  needing to load every player into memory to do it.
- **Tax**: flat 5% surcharge on ordinary `buy` purchases only (hospital/
  stable/repair purchases branch out of `cmd_buy()` before reaching the
  tax code and stay untaxed — they're not "shop economy" transactions
  in the same sense). Collects into a new singleton `world_treasury`
  row (id always 1), visible to immortals via the new `treasury`
  command. No spend mechanic yet — a disclosed, deliberate gap, a hook
  for something later rather than dead weight now.
- **Command-table placement**: `bank` deliberately placed AFTER `bash`
  (not strict alphabetical order) so the far-more-frequently-typed
  Warrior combat skill keeps ownership of the "ba" abbreviation, same
  precedent as `retrieve` placed after `return` in Session 57. Confirmed
  no other collision by checking the full mortal `b`-block before
  inserting. `treasury` is a plain alphabetical insert into the
  immortal block, right after `transfer`.
- **New `tests/smoke_test_bank.py`** (17 checks) — found a real bug in
  the TEST itself while writing it, not the feature: the first draft
  bought item #1 at the real shop (a 3-gold torch), whose 5% tax rounds
  down to 0 gold and so never triggers `cmd_buy()`'s tax message at all
  (`if (tax > 0)` guards it) — the test would have silently passed for
  the wrong reason (no tax message ever appearing, mistaken for "no
  bug" rather than "rounds to zero"). Switched to a pricier item (#6,
  30-gold fuel brick) so the tax path is actually exercised. Daily
  interest itself isn't covered by the automated test — a real in-game
  day is ~96 real minutes at Tobin's default clock speed, the same
  "not practical to keep automated" call already made for
  `smoke_test_heartbeat.py`'s own real-time boundary — sanity-checked
  the UPDATE query's syntax and FLOOR() rounding behavior manually
  against a throwaway row instead (confirmed sub-200-gold balances
  correctly don't grow that day, by design, rather than rounding up to
  a free coin).
- Built clean (zero compiler warnings) on the first attempt this
  session — no db_query format-specifier or missing-include bugs this
  time, likely because `%f` (needed for the interest rate constant) had
  already been proven out by Session 57's `%i`-casting bug, so it got
  checked directly in `db.c` before writing `bank.c` rather than
  assumed.
- Verified live end-to-end on preview (all 17 checks passing, plus the
  existing banking/deposit/withdraw/tax flow manually re-confirmed).
  Production's on-disk binary already has this build (same situation as
  Session 57 — rebuilt once, serves both instances); deploy to
  production deferred to the user's own copyover, per their stated
  preference from Session 57 ("i'll copyover") rather than spinning up
  a disposable test immortal to trigger it automatically.
- **Real, unrelated regression found and fixed the same session**: while
  picking the bank's real seeded room, found the room still named
  "Grimhaven First Kingdom Bank" -- but TODO.md already has a "done"
  entry for a global "Grimhaven" → "Tobin City" text replace across 18
  columns (`room`, `mob`, `obj`, `zone`, etc.). Re-checked live: 639
  rooms on this box (Home, 192.168.254.200) still contained "grimhaven"
  text, across the exact same 18 columns the original fix covered --
  the local seed files (`zone_reset.sql` etc.) were already clean, so
  this wasn't a seed-file regression, just this box's live database
  never actually receiving the original fix (most likely a live-DB-only
  mutation that was run once, on one box, and never applied to the
  other -- Home and Work each run their own independent `sneezy`
  database, only the code is git-synced). Re-ran the same
  `REGEXP_REPLACE(col, 'grimhaven', 'Tobin City')` approach against all
  18 columns on Home. Two pre-existing wiznews entries (a lamp-lighting
  mob flavor note, and the original rename announcement itself, which
  legitimately needs to keep saying "Grimhaven" to make sense) were left
  alone -- only this session's own new wiznews/news/tobin_migrations.sql
  text got corrected to match. **Not yet verified**: whether the Work
  box's database has the same gap -- flagged in TODO.md rather than
  guessed at, since this session has no active connection to it.

Last updated: 2026-07-21 — Session 57 (home): **Object maintenance tasks
3-4 — the repair-shop economy (Sneezy → Tobin feature audit), closing out
the item Session 55 left half-done. Deployed to production, not just
preview, per a new standing preference.**
- **Scope**: Session 55 already had the user's "Full system" pick on
  record (decay timers + combat structure damage + a full repair-shop
  economy + per-class repair skills) and shipped tasks 1-2; this session
  picked up tasks 3-4. Checked the real upstream `misc/repair.cc`/
  `disc/disc_warrior_blacksmithing.cc` first, same "check Sneezy before
  building" habit as every other audit item: a mature, file-backed ticket
  system (`mutable/repairs/<repairman_vnum>/<ticket_number>`) with a
  real-time repair delay and genuinely per-MATERIAL repair skills
  (`SKILL_BLACKSMITHING` for metal, `SKILL_REPAIR_MONK` for
  organic/wood/hide/rock, `SKILL_REPAIR_CLERIC`/`DEIKHAN` for holy items,
  `SKILL_REPAIR_MAGE`/`THIEF`/`SHAMAN` for the rest, `SKILL_MEND` as a
  generic fallback). Tobin has no material-property system yet to gate
  those on (that's its own still-open audit item, deliberately not
  pulled forward just to unblock this one), so this shipped Tobin-scale:
  **one** `repair` skill (Warrior, CLASS tier, level 5, matching
  "blacksmithing" flavor most closely), **no real-time repair delay**
  (a DB-backed ticket has no equivalent need for a file-based background
  job's pacing — ready immediately), **self-repair costs flat gold**
  ("makeshift materials", `SELF_REPAIR_GOLD_PER_POINT=2`) rather than a
  seeded consumable item (avoided allocating new persistent-content
  vnums), and **monogram is cosmetic-only** (not yet surfaced in
  `look`/`examine` — a known, disclosed gap, not an oversight).
  Depreciation permanently lowers the repair ceiling
  (`effective_max_struct() = max(1, max_struct - depreciation)`), same
  idea as the real upstream's `TObj::maxFix()`.
- **Schema**: `player_inventory` gained `cur_struct`/`depreciation`/
  `monogram` columns — all nullable/zero-defaulted so existing rows read
  back correctly. This closes a real, latent gap from Session 55: that
  table previously stored only `vnum`+`slot` per carried item, so a
  damaged-but-not-destroyed item's structure damage was silently lost on
  every reconnect. New `repair_ticket` table (player_id, shop_nr,
  obj_vnum, item_label, orig_max_struct, depreciation_before, monogram,
  price), FK'd to `player.id` ON DELETE CASCADE. `shop.is_repair` column,
  seeded true for shop_nr 134 ("Blacksmith's Forge", room 7110) — a real
  seeded shop, thematically exact, same "new column when nothing real
  exists to reuse" precedent as `shop.is_stable`.
- **Code**: `include/repair_repo.h` + `src/db/repair_repo.c` (new) —
  `repair_ticket_create/find/delete/list_for_player()`. `src/cmd/cmd_repair.c`
  (new) — four commands: `repair <item>` (self, gold-cost, materials
  spent whether the proficiency roll succeeds or not, same "cost
  regardless of outcome" shape bash/kick/disarm already established),
  `submit <item>` (hand a damaged item to a repair-shop keeper for a
  ticket, destroys the carried item), `tickets` (list pending claims at
  the current shop), `retrieve <#>` (pay and collect — reconstructs the
  item via `obj_create_from_proto()`, carries depreciation+1 and the
  monogram forward, deletes the ticket). `shop_repo_is_repair()` mirrors
  `shop_repo_is_stable()`. Command-table placement: `repair`/`submit`/
  `tickets` land alphabetically; `retrieve` is deliberately placed AFTER
  `return` (not strict alphabetical order, with an inline comment) so the
  far-more-frequently-typed immortal `return` command keeps ownership of
  the "ret"/"retu" abbreviation.
- **Bugs found and fixed while building**: `db_query()` only supports
  `%s`/`%i`/`%f` — `repair_repo.c`'s first draft used `%li` for the
  `long player_id` parameter in three queries (server log: `bad format
  specifier 'l'`), fixed by casting to `(int)` throughout, matching every
  other call site's existing convention. Missing `#include <stdio.h>` in
  `repair_repo.c` (implicit `snprintf` declaration). Two
  `-Wformat-truncation` warnings, fixed by widening `obj_t.monogram` from
  `char[64]` to `char[65]` (exact match to `repair_ticket_t.monogram`)
  and the retrieve-message buffer. In `cmd_retrieve()`'s hand-back
  message, `ticket.item_label` already carries its own leading article
  ("a dented shield") — the first draft's "hands back your %s" read as
  "hands back your a dented shield"; fixed to "hands you %s, good as
  new."
- **Own test-script mistakes, repeated from earlier in the broader
  session**: hit the quit!-before-SQL ordering bug again in the first
  manual verification script (SQL row edits must land AFTER `quit!`, or
  `quit!`'s own `player_save()` clobbers them with stale pre-SQL
  in-memory state) — fixed by reordering. Also briefly misdiagnosed
  `quit!` dropping a test character's held item as a missing `SAVE
  ROOMS` room-flag bug before reading `cmd_quit.c` directly: `quit!`
  unconditionally drops everything by design, always — it's Tobin's
  deliberate "risky logout" (`rent` is the safe one that preserves
  inventory). Not a repair-shop bug at all; switched to the
  non-destructive `save` command to verify persistence instead, which
  worked correctly the first time.
- **New automated test-authoring bug, found and fixed**: the new
  `tests/smoke_test_repair.py`'s first two drafts asserted "repair
  refuses a character who hasn't learned the skill" by simply not
  inserting a `player_skill` row — but `being_knows_skill()` (skill.c)
  gates any `SKILL_TIER_CLASS` skill on `player_progress.basic_disc_pct
  > 0` alone; it never actually checks whether a `player_skill` row
  exists for that specific skill at all (that row only feeds
  `skill_roll_success()`'s proficiency roll once a skill is already
  "known" via the discipline-tier gate). Fixed by granting
  `basic_disc_pct` and the `player_skill` row together, in the SAME step
  as the "how do I unlock this" case, rather than treating the skill row
  as the gate.
- **New `tests/smoke_test_repair.py`** (15 checks) — run against the
  REAL seeded shop_nr 134/room 7110 rather than fabricating a shop row
  (the `shop` table's full schema — profit multipliers, four message
  templates — makes a synthetic INSERT more fragile than just using the
  live content the `is_repair` flag was seeded onto). Covers: refusal
  without the skill, refusal without enough gold (both self-repair and
  shop retrieve), a successful 100%-proficiency self-repair,
  cur_struct/depreciation/monogram round-tripping through `save`, and
  the full submit → tickets → retrieve shop flow including the ticket's
  DB row actually being deleted (not just hidden) on retrieval.
- **Deployed to production directly, not preview** — user: "i dont like
  running a preview copy, can we just use production?" Going forward,
  preview is pre-deploy verification only, not a standing parallel
  environment. Ran the targeted regression pass (skillcombat, objects,
  object_maintenance — the systems adjacent to this session's
  `obj_repo.c`/`skill.c`/`cmd_table.c` changes, not a full `sweep.sh` per
  the user's standing "no sweep" preference) against preview first, all
  three clean (the `smoke_test_object_maintenance.py` "scraps of" check
  flaked once mid-run — same known test-harness timing quirk documented
  in Session 56, reconfirmed as a flake, not a regression, by an
  immediate clean rerun). Deployed via a clean `copyover` the user ran
  themselves from their own live `Jesus` session (not a disposable test
  immortal, since `Jesus` was already level 60) — 1 connection restored,
  0 dropped, no repeat of Session 56's duplicate-connection incident.
  gdb re-attach wasn't actually needed: ptrace attachment survives
  `exec()` across a copyover, so the gdb session already watching
  production's PID kept tracing it seamlessly through the restart with
  no gap.
- Updated the "Sneezy → Tobin Feature Audit" artifact (Object maintenance
  flipped Partial → Done, 30/7/12/6).

Last updated: 2026-07-21 — Session 56 (home): **Synced Sessions 53-55's
work-box work in (Offensive spells, Magic items, Object maintenance
tasks 1-2); ran the follow-up testing pass Session 55 flagged.**
- Pulled/applied schema/rebuilt/deployed all three sessions' work to both
  Home preview (4003) and production (4000) -- production via a clean
  `copyover` (no player dropped; a real connected immortal, `Jesus`, was
  online at the time).
- **`tests/smoke_test_object_maintenance.py` run for the first time**
  (flagged as not-yet-run in Session 55). First attempt hit the exact
  test-harness quirk Session 55's own write-up already documented: the
  final "scrap object left in the room" check read from a socket still
  mid-combat (constant interleaved combat spam), so it missed the text
  even though the mechanic itself works correctly. Fixed the same way
  Session 55's own manual verification did -- a fresh spectator
  connection (a second immortal-level login, exempt from the multiplay
  gate) that never sees combat traffic at all, confirmed via a focused
  debug trace showing the scrap text present in both the target's own
  `look` AND the spectator's. All 7 checks now pass clean, repeatedly.
- **Real, unrelated issue found and fixed along the way**: the production
  `copyover` above produced a genuine duplicate-connection artifact --
  the real player's client apparently auto-reconnected in the ~second-
  long gap of the copyover's `exec()`, landing a SECOND fresh login for
  the same character moments after the ORIGINAL connection was already
  restored via the copyover recovery file. Diagnosed live (not guessed):
  `ss -tni` showed the surviving "active" connection was actually a dead
  half-open TCP socket (zero bytes received in ~21 minutes, stuck
  retransmitting), not a real live client -- confirmed with the user
  before acting, then a hard restart of production cleared it (no admin
  "disconnect a stuck connection" command exists yet in the codebase;
  worth adding as a future TODO item). No code change from this --
  purely an operational hazard of running copyover un-observed.

Last updated: 2026-07-21 — Session 55 (work): **Object maintenance (Sneezy
→ Tobin feature audit), decay timers + combat structure damage — HALF of
a 4-task "full system" scope, tasks 3-4 (repair-shop economy, per-class
repair skills) still open.**
- **Scope**: user picked "Full system" via `AskUserQuestion` (decay
  timers + combat structure/durability damage + a full repair-shop
  economy with tickets/skills/materials/monogram/depreciation), same
  full-system precedent as Magic items. Only the first two of four
  sub-tasks landed this session; the repair economy and repair skills
  are carried forward as open TODO items rather than rushed.
- **Decay timers (task 1)**: the upstream `obj` table's own real `decay`
  column (`-1`=never/`0`=this tick/`>0`=ticks remaining, matches the
  original's `OBJ_NOTIMER` convention, confirmed against real seed data:
  8757 rows at -1, 514 at 0, plus a real positive-tick distribution) was
  loaded but completely unused before this session. Now: `obj_t.decay_
  time` (obj.h), read verbatim in `obj_proto_load()`/`obj_create_from_
  proto()` (obj_repo.c/obj.c), ticked once per real ~60s pulse by new
  `obj_decay_tick()` (`world_for_each_obj()`-driven, room-floor objects
  only) — decrements, relocates a decaying container's contents to the
  room first (`obj_destroy()` doesn't touch children), announces "X
  decays into nothing." to the room, then destroys it. Corpses
  (`CORPSE_DECAY_TICKS=15`) and severed limbs (`LIMB_DECAY_TICKS=20`,
  combat.c) now get a real countdown instead of sitting forever.
  **Schema-default bug caught and fixed, not shipped as a hazard**: the
  `decay` column's real schema default was `0` ("decays this tick") —
  backwards for any INSERT that omits it, which 30+ existing smoke-test
  fixtures (and any future hand-authored content) do. Worse, `zone.c`'s
  4 persistent-object-creation call sites (`zone_cmd_load_obj_ground`/
  `_equip`/`_give`/`_place`) route through the same `obj_create_from_
  proto()` — honoring real `decay=0` data there verbatim would make
  ~514 real, persistent WORLD objects vanish within about a minute of
  every zone reset. Fixed two ways: a Tobin migration changes the
  column's schema DEFAULT to `-1` (protects future inserts, doesn't
  touch already-seeded rows), and all 4 `zone.c` call sites explicitly
  reset `decay_time = -1` right after creation (persistent content stays
  exempt; an admin's `load obj <vnum>` and Tobin's own ephemeral objects
  are deliberately NOT touched by that override, so real decay data and
  countdown-timers still apply there).
- **Combat structure damage (task 2)**: `obj_t.max_struct`/`cur_struct`
  already existed (real DB columns) but were read-only, feeding just a
  one-word condition summary in `look`. New `combat_maybe_damage_
  equipment()` (combat.c, hooked into `combat_strike()` only — normal
  melee, deliberately NOT the shared bash/kick/spell/wand/staff damage
  pipeline, an honest scope cut): flat 30% chance per landed hit
  (matches the original's documented base rate) that whatever the
  DEFENDER has equipped on the LIMB that got hit takes 1-2 structure
  damage; at 0 the item is destroyed — affects reversed (`obj_apply_
  equip_affects(..., -1)`), slot cleared, inventory saved if a PC, a
  "scraps of X" ephemeral object dropped in the room (`SCRAP_DECAY_
  TICKS=10`), original object freed. Held/wielded items are never
  touched (only `equipment[]`, matching Magic items' own worn-vs-held
  split); no material-susceptibility matrix (deferred to the separate,
  still-open "Material properties" audit item).
  **Verification took two wrong turns before landing**: (1) first
  attempt wore the test item on the IMMORTAL attacker — `combat_strike()`
  forces `dmg=0` against an immortal DEFENDER, so an immortal's own gear
  can never trigger this at all; fixed by wearing it on a mortal TARGET
  instead. (2) Confirming the destroy actually fired required a fresh
  spectator connection (`goto <target>`/`look <target>`) rather than
  trusting the original test sockets' own captured output — a real,
  reproducible test-harness quirk (see below) was truncating/dropping
  output on any socket left open and merely polling (no further writes)
  for 30+ seconds while driven from a live foreground SSH command.
  Confirmed working end-to-end this way: equipment slot cleared (`body:
  nothing`), "Scraps of a fragile test shirt lie here, ruined." on the
  room floor, target's inventory empty (destroyed item doesn't go to
  inventory).
- **Test-harness quirk found and worked around, not a product bug**: a
  descriptor that sits idle (server sends data/keepalive NOPs, client
  only polls via `recv()`, never writes again) for 30+ seconds
  reproducibly gets disconnected (`"X has lost its/her/his link."`,
  `descriptor_process_input()` reading EOF) ONLY when the driving test
  script is a direct foreground `ssh ... python3 ...` invocation; the
  IDENTICAL socket-idle-then-poll pattern run instead as a fully
  detached background process (`setsid nohup ... & disown`, polled via
  separate short-lived `ssh` calls against a result file) does not
  reproduce it at all, confirmed on two different tests
  (`check_struct.py` this session, `smoke_test_weapon_depth.py` re-
  verification). No corresponding server-side error (`descriptor
  flush_output`'s own "backlog full" log line never appears; immortals
  are already coded immune to the *policy* idle-out in `descriptor_
  idle_timeout()`, and the disconnected character in both cases WAS the
  immortal) — points at this dev sandbox's own foreground-SSH-channel
  process handling, not game code. Flagged here rather than chased
  further; if it recurs, prefer the detached-background-process pattern
  for any test that leaves a socket idle-but-open for a long poll.
- **`smoke_test_weapon_depth.py` fix (carried over from Session 54's
  root-cause, not re-litigated)**: `make_dummy()`'s mob is now seeded at
  `level: 50` instead of `level: 1` (its huge `hpbonus` inflated overall
  HP only, not the separate per-limb HP cap that Session 54 root-caused
  as the actual cause of dummies dying outright to major-limb
  destruction well before the test's 30-hit sample). The fix itself
  reads as correct against `being_limbs_full_heal()`'s own real formula,
  but a clean, fully-passing live run wasn't obtained this session — the
  two live attempts both ran into the test-harness quirk above instead
  (lost real hits mid-collection after the connection dropped, not a
  combat/limb bug: no defeat/slain log line for the dummy in either
  run). Re-verify with the detached-process pattern next session before
  trusting it fully.
- **Testing**: new `tests/smoke_test_object_maintenance.py` (corpse
  decay via `aitick`, equipment destruction via real combat against a
  fragile two-slot test item set) — written this session but not yet
  run start-to-finish as a file (its individual pieces were verified
  live via the manual `check_decay.py`/`check_struct.py` scripts
  instead). **Not done this session, flagged as a follow-up**: running
  `smoke_test_object_maintenance.py` itself plus the broader regression
  pass against `smoke_test_combat.py`, `smoke_test_objects.py`,
  `smoke_test_objmanip.py`, `smoke_test_skillcombat.py`, `smoke_test_
  weapon_messaging.py`, and any zone-reset test (all touch `obj.h`/
  `combat.c`/`zone.c`, which all gained new fields/logic this session) —
  session ended before a testing pass to get changes committed; do this
  first thing next session, before building tasks 3-4 on top.
- No new player command landed (decay/structure damage are both passive
  systems) — no help-topic changes needed this round. `news.sql`/
  `wiznews.sql` entries added (player-visible: corpses/scraps
  disappearing and gear breaking in a fight are both directly
  observable).
- **Not started yet** (carried to a follow-up session): task 3 (repair-
  shop economy — submit/retrieve tickets, pricing) and task 4 (per-class
  repair skills — blacksmithing etc., tool/material requirements).

Previous update: 2026-07-21 — Session 54 (work): **Magic items (Sneezy →
Tobin feature audit), full system — equipment stat/AC/HP/Vitality
affects, plus a new `use` command for wands/staves/scrolls.**
- **Scope**: the user picked "Full system (equipment + wands + scrolls +
  staves)" via `AskUserQuestion` over two smaller options offered — this
  is a substantially larger slice than most audit items, closer to a full
  port of the original's magic-item system than the usual Tobin-scale cut.
- **Equipment stat/AC/HP/Vitality affects**: the real, upstream-seeded
  `objaffect` table (already partially used by `obj_load_combat_mods()`
  for weapon hitroll/damroll) also carries real per-item bonus rows for
  STR/DEX/CON/INT/WIS/CHA, Armor Class, max HP, and max Vitality. New
  `obj_load_stat_affects()` (obj_repo.c) reads these; cached on the
  `obj_t` at creation (`aff_str`/`aff_dex`/.../`aff_ac`/`aff_hit`/
  `aff_move`, obj.h) rather than re-queried live, matching how combat
  mods already work. Two real discoveries, both caught by checking real
  data rather than assuming (house rule): (1) **AC sign-flip** — every
  real `objaffect` AC row (`type=11`) is negative, confirmed by querying
  the live table, while Tobin's own `being_total_ac()`/`obj_armor_ac()`
  convention is the opposite (higher is better, per `combat.c`'s
  `modifier -= being_total_ac(defender) / 2` and the mounted-bonus
  comment) — import negates the raw value. (2) **AC-affects-only-armor
  bug, caught live**: an initial version only applied `aff_ac` for
  `OBJ_CAT_ARMOR`-category items; manual verification with a real seeded
  ring (vnum 179, `objaffect` AC row, category JEWELRY not ARMOR) showed
  Armor Class not moving at all on wear. Fixed by having `obj_armor_ac()`
  apply real `aff_ac` data regardless of category — a ring/shield/other
  worn-slot item can carry a real AC row too — while the placeholder
  weight-based guess formula stays reserved for true armor-category items
  with no real data (guessing an AC for a ring with none would be
  nonsense). STR/DEX/CON/INT/WIS/CHA/max-HP/max-Vitality are STORED
  values (unlike AC, recomputed live every call) so these are applied via
  mutate-on-wear / reverse-on-remove (`apply_equip_affects()`,
  `cmd_object.c`), hooked into `cmd_wear()`/`cmd_remove()` — deliberately
  NOT `cmd_wield()`/`cmd_hold()`, since only true worn `equipment[]` slots
  carry these upstream (wielded weapons get their existing hit/damroll
  treatment instead, no change there).
- **New `use <item> [target]` command** (`cmd_use.c`) for scrolls/wands/
  staves. Checked the original's own doc first (scrolls single-use up to
  three spells; wands rechargeable/targeted; staves rechargeable/room-
  wide) and scoped to Tobin's real infrastructure: ONE spell per item, any
  character can use one regardless of class/level (matches the original),
  the effect reuses the SAME generic heal/protective-ward/single-target-
  damage dispatch `cast`/`pray` already have (keyed off the stored
  spell's own description) rather than a third full copy of
  `task_cast()`/`task_pray()`. **Raw magic-item `val[]` data ruled out**:
  an existing comment on `cmd_identify.c` (from an earlier audit item)
  already documented this data as unreliable import noise (a nonsense
  25650 "charges" value on a real scroll) — rather than reinterpreting
  it, a fresh Tobin-owned table (`obj_magic`, `db/sneezy/obj_magic.sql`)
  maps a vnum to the spell name and starting charge count it invokes;
  `obj_magic_repo_get()` (new `obj_magic_repo.h`/`.c`) reads it. A scroll
  applies its effect once and is destroyed (`obj_destroy()`); a wand/
  staff decrements a charge (`o->val[0]`, seeded from `obj_magic.max_
  charges` at creation) and refuses use once exhausted, with no recharge
  mechanic yet (an empty one just sits inert until a future `edobject`-
  style tool exists). Three seed items ship as real, usable examples:
  wand of gusts (90000, "gust", 5 charges), staff of fireball (90001,
  "fireball", 3 charges), scroll of minor healing (90002, "heal light",
  single-use).
- **Deliberately not attempted** (an honest Tobin-scale slice within an
  otherwise large scope): potions (a stretch for `use` — `drink`/`quaff`
  would fit better, a separate command entirely); a recharge command for
  empty wands/staves; mana costs (nothing in Tobin has a mana pool yet);
  the extended stats the original's `objaffect` enum covers that Tobin
  doesn't model at all (BRA/AGI/FOC/SPE/PER/KAR) and other unmapped
  types (MANA/SPELL/SPELL_EFFECT/LIGHT/NOISE/CAN_BE_SEEN/VISION/
  PROTECTION/DISCIPLINE/SPELL_HITROLL/CURRENT_HIT/CRIT_FREQUENCY/GARBLE)
  — left unapplied, same as before this work.
- **Known, accepted limitation, not a new gap**: `player_inventory` only
  persists `vnum` + `slot`, not per-instance `val[]` state (confirmed by
  reading `player_inventory_load()`, which calls `obj_create_from_proto()`
  fresh every time) — a wand/staff's spent charges reset to max on
  reconnect, matching the existing, already-accepted behavior for
  component pouches and holy symbols.
- **Testing**: new `tests/smoke_test_magic_items.py` (13 checks — real
  ring/token wear-then-remove round-tripping AC/max-HP cleanly, wand
  charges depleting and refusing further use once exhausted, staff room-
  wide effect reaching a bystander, scroll single-use destruction and
  actual removal from inventory, not just a cosmetic message).
  Regression-checked against `smoke_test_combat.py`, `smoke_test_
  objects.py`, `smoke_test_objmanip.py`, `smoke_test_skillcombat.py`,
  `smoke_test_weapon_messaging.py` (all touch `obj.h`/`cmd_object.c`/
  `combat.c`, which all gained new fields/logic this session) -- all
  pass clean. `smoke_test_weapon_depth.py` reliably fails, but root-
  caused live (not guessed, and confirmed unrelated to this session's
  changes) rather than dismissed: its `make_dummy()` helper seeds the
  training dummy at `level: 1` and relies on a huge `hpbonus` (5000) for
  it to "survive dozens of real hits," but never accounts for the
  SEPARATE major-limb-destroyed-is-instant-death mechanic
  (`combat.c`'s `is_major_limb()`/`combat_sever_limb()`, Session 42) --
  a level-1 mob's per-limb HP caps stay at the tiny level-1 baseline
  regardless of `hpbonus`, so purely by chance, enough hits landing on
  the same major limb (head/neck/waist/body) kills the dummy outright
  well before the needed 30-hit sample, no matter how large its overall
  HP pool is. Confirmed via direct live diagnosis, not assumption: the
  attacker's `set_dex(900)` boost was verified actually persisting
  (`score` and a direct DB query both showed it), and combat rounds were
  independently measured firing at the correct 1.2s cadence
  (`COMBAT_ROUND_PULSES`) with a ~95%+ hit rate -- ruling out both
  timing and to-hit as the cause before landing on the real one. This
  bug predates Magic items entirely (my changes this session don't
  touch `combat.c`, limb HP, or mob creation at all) and reproduces
  identically on a from-scratch server restart with zero other players
  connected, so it isn't the linkdead-pulse-slowdown class of flake
  Session 51/53 found either. Flagged as a separate follow-up
  (`spawn_task`) rather than fixed here, to keep this change scoped.
- Help topics: new `use` topic; `wear`/`remove` updated to mention real
  stat/AC/HP/Vitality bonuses. `news.sql`/`wiznews.sql` entries (player-
  visible: wearing gear and using items are both player-facing).

Previous update: 2026-07-20 — Session 53 (work): **Offensive spell breadth
(Sneezy → Tobin feature audit), closing the "real per-spell mechanics
remain a follow-up" note left on the original `cast`/`pray` v1.**
- **Three real gaps fixed in the offensive-damage path of both `cast` and
  `pray`.** (1) A single FLAT damage formula regardless of which spell was
  cast — a level-1 "gust" and a level-50 "atomize" hit identically hard.
  Now scales with the SPELL's own `min_level` (`spell_damage_for_level()`,
  duplicated per-file same as this codebase's other small helpers — rough
  calibration against `combat.c`'s melee formula). (2) Only ever usable on
  whoever `ch->fighting` already was — no way to open combat with a spell
  at all, and `cast` had no target syntax whatsoever (unlike `pray`, which
  already supported one). Ported `pray`'s `find_spell_and_target()` into
  `cmd_cast.c`; either command can now open a brand-new fight against a
  target in the room (same both-directions-engage logic `cmd_attack.c`
  uses), falling back to `ch->fighting` when no target is given so old
  no-target usage is unchanged. (3) A raw `being_hurt_limb()` with no
  defeat handling — a kill via spell damage skipped XP/corpse/cleanup
  entirely. Both the single-target and new area-effect paths now go
  through `combat_apply_skill_damage()` (bash/kick's shared pipeline,
  Session 52) instead.
- **Real area-effect, finally.** Several spells' own descriptions have
  said "area-effect burst of X damage" since the roster was first
  written (fireball, tsunami, hellfire, pebble spray, plague of locusts,
  earthquake, ...) but silently behaved single-target the whole time.
  New `cast_area_damage()`/`pray_area_damage()` hit every other being in
  the room except the caster and their own group (`being_in_group()`,
  same friendly-fire exclusion the original's area spells use) — PCs and
  mobs alike.
- **Deliberately not attempted** (an honest Tobin-scale slice, matching
  this session's established pattern): mana costs (no mana pool exists
  yet), and elemental damage TYPES as a real mechanic — no immunity
  system exists to back it, so messaging stays generic, same precedent
  as protective spells all sharing one `AFFECT_SANCTUARY` buff instead of
  ~30 bespoke elemental resistances.
- **A bug caught and fixed mid-development, never shipped**: an early
  draft resolved the default (no-target) offensive target as
  `ch->fighting` at the CALLER level in both `cmd_cast()`/`cmd_pray()`.
  That would have made a plain "pray heal light" (self-heal, no target)
  target the CURRENT OPPONENT instead of self whenever already in
  combat — caught by re-deriving the design before syncing, not by a
  failing test. Fixed by only falling back to `ch->fighting` INSIDE the
  offensive branches (a separate `atk_target` local computed once at the
  top of `task_cast()`/`task_pray()`); the heal/buff branches keep using
  `target` directly (self by default), fully unaffected.
- **Testing**: new `tests/smoke_test_offensive_spells.py` (12 checks:
  tiered damage scaling, `cast <spell> <target>` genuinely opening combat
  — proven by a follow-up no-target cast still landing on the same
  opponent, not just a one-off hit — the self-heal-while-fighting
  regression check above, and area-effect catching multiple separate,
  uninvolved bystanders). All four test characters promoted to immortal
  so the area-effect check could run in an isolated throwaway sandbox
  room instead of a real, populated production room where a live-fire
  spell could catch actual bystanders. Regression-checked against
  `smoke_test_castpray.py`/`smoke_test_immortal_castpray.py` (two stale
  assertions fixed — both checked for `"You cast gust"` as a substring of
  the OLD "nothing happens yet" placeholder text, which no longer
  appears now that gust has a real effect; updated to expect the new,
  correct "Cast that at whom?" response, which still only appears once
  the component/class gate has passed — what those tests actually
  check), `smoke_test_affects.py`, `smoke_test_component_charges.py`,
  `smoke_test_cure_and_inflict.py` (exercises the poison/disease
  offensive-prayer paths directly), `smoke_test_practice.py`,
  `smoke_test_water_drowning_flight.py` — all pass clean.
  `smoke_test_continue.py` fails on this box, but confirmed pre-existing
  and unrelated: it was already in the failed list from a full
  `tests/sweep.sh` run that predates any of these changes (root cause
  not chased down further — out of scope for this item).
- **Debugging note for posterity**: while writing the new smoke test,
  hit the exact same "character names can't contain digits" trap Session
  52's `smoke_test_skillcombat.py` write-up already documented (`Offbys1`/
  `Offbys2`/`Offimm2` silently failed character creation, leaving an
  empty account that then looked like a total login failure two ply
  deep) — worth internalizing as a standing habit, not just a one-off
  fix, since this is now the second time it's cost real debugging time
  in as many sessions.
- Help topics (`cast`/`pray`) updated to document the new target syntax
  and combat-opening behavior. wiznews.sql + news.sql entries added
  (changes what every spellcasting class experiences).

Last updated: 2026-07-20 — Session 52 (home): **Skill-based combat closed
out (bash/kick/disarm/parry, task 14) + synced Session 50/51's work-box
work in.**
- **Skill-based combat (bash/kick/disarm/parry), fully closed out.** Code
  landed the previous home session (commit `791d02c`); this session
  finished debugging `tests/smoke_test_skillcombat.py` (12 checks) through
  four real bugs, all in the TEST, not the underlying mechanic:
  1. `cmd()`'s ~1s blocking default timeout was eating most of the
     DEFENDER's 1.2s (`COMBAT_ROUND_PULSES`) post-bash wait window before
     the follow-up check even ran (the window starts when bash resolves
     server-side, not when the blocking call returns) -- fixed by using a
     short-timeout live read for the `bash` command itself, same "live
     read" pattern `smoke_test_vitality_terrain.py`'s `live_vit()` already
     established.
  2. `attack`'s own `cmd_attack.c`-side `being_set_wait()` on the ATTACKER
     at fight-initiation collides with an immediately-following skill
     command attempt ("You are still recovering!" before the skill roll
     even happens) -- fixed with a new `attack_and_settle()` helper
     (sleeps off the 1.2s lag before trying a skill command).
  3. Character names can't contain digits ("Names may only contain
     letters") -- the 0%-proficiency sub-test fixtures (`Bshw0`/`Kckt0`/
     `Dsmw0`) were silently failing character creation, leaving no player
     row at all (root-caused via a literal NULL-subquery SQL error).
     Fixed by renaming to letters-only (`Bshwz`/`Kcktz`/`Dsmwz`).
  4. `being_knows_skill()` gates `SKILL_TIER_CLASS` skills (disarm, for
     Warrior -- level 17) on BOTH `level` and `basic_disc_pct`, not
     `combat_disc_pct` like `SKILL_TIER_COMBAT` skills (bash/kick/parry)
     use -- the test's `make_pair()` helper only seeded level and
     `combat_disc_pct`, so disarm's characters hit "You don't know how to
     disarm an opponent." regardless of seeded proficiency. Fixed by
     adding an optional `level=` param to `make_pair()` and seeding
     `basic_disc_pct` alongside `combat_disc_pct` unconditionally (harmless
     for tiers that don't check it).
  All 12 checks now pass clean on both preview (4003) and production
  (4000). Regression-checked against `smoke_test_combat.py`/
  `smoke_test_positions.py`/`smoke_test_weapon_depth.py` (also exercise
  `combat_strike()`, which the passive parry hook modifies) -- the first
  `weapon_depth` run came up short (25/30 samples collected) and looked
  like a regression at first, but root-caused live (not guessed) to 16
  linkdead zombie characters left over from this session's own earlier
  crashed debug runs bogging down the combat pulse loop (`who` showed
  `Linkdead: [16]`) -- a clean preview restart cleared it and the rerun
  passed clean. This is the SAME underlying issue Session 51 (work)
  independently found and logged as the new TODO.md priority item below,
  from a completely different cause (months of tests that `s.close()`
  instead of `quit`). TODO.md's audit checklist also had a gap -- Session
  49's Mount/riding (13) and this session's Skill-based combat (14) had
  STATUS.md write-ups but were never checked off in the "Sneezy → Tobin
  feature audit" list -- both added retroactively. wiznews.sql + news.sql
  entries added (player-facing new commands).
- **Synced in Session 50/51's work-box work** (socials DB port + full
  Sneezy set + `edsocial` editor, redit Extra Descriptions builder half,
  the real `cmd_skills.c` crash fix, `sweep.sh`'s new per-test timeout) --
  see those sessions' own write-ups below for full detail. Home VM had
  also rebooted independently during this session (unrelated to the work
  above); rebuilt clean and restarted both preview/production on the
  synced code. Confirmed `smoke_test_edsocial.py` fresh at Home per
  Session 51's own sync-up note (it hadn't been verified there) --
  passes clean, along with `smoke_test_socials.py`/
  `smoke_test_redit_extradesc.py`/`smoke_test_skills.py`/
  `smoke_test_redit.py`. `gdb` was not installed on the Home VM (couldn't
  install it remotely, no interactive sudo password available) -- user
  installed it directly; both preview (4003) and production (4000) now
  have gdb attached per the Session 51 habit
  (`gdb_crash_preview.log`/`gdb_crash_prod.log`).
- **Linkdead auto-purge**, TODO.md's new PRIORITY item from Session 51,
  picked up and shipped same session. Asked the user the flagged open
  design question directly rather than defaulting silently: **save-then-
  destroy** (matches the real Sneezy's `nukeLdead()` and the user's
  original phrasing), not TODO's own discard-only leaning -- a known,
  disclosed trade-off (an admin DB edit to a character linkdead 5+
  minutes can still be clobbered by the stale pre-disconnect snapshot,
  same risk class `descriptor_destroy()`'s own comment already reasons
  through for its unaffected discard-only path). New
  `being_t.linkdead_since` (being.h), `world_purge_stale_linkdead()`/
  `linkdead_purge_tick()` (world.c/h, sibling to the existing discard-
  only `world_purge_linkdead()`), `pulse_register(600,
  linkdead_purge_tick)` (~60s cadence). Flat 5-minute threshold for
  everyone (not the original's 15/60 split), runtime-configurable via a
  new `TOBIN_LINKDEAD_PURGE_SECONDS` env var (config.h/.c, same pattern
  as `TOBIN_PORT` etc.) specifically so it could be verified live without
  waiting out the real 5 minutes. **Verified live, both halves, not just
  built and assumed**: restarted preview under
  `TOBIN_LINKDEAD_PURGE_SECONDS=5` and confirmed (1) a raw-socket-closed
  character is force-removed once past the threshold + one ~60s check
  cycle (`who`'s Linkdead count returns to 0, `goto <name>` no longer
  finds them), and (2) the save is genuine, not a discard: dropped a
  character's Vitality via charged moves (saved immediately), waited
  ~16s for `regen_tick_run()` to heal Vitality further IN MEMORY ONLY
  (regen never itself calls `player_progress_save()` -- see the
  Vitality/Terrain write-up above), confirmed the DB value was still the
  lower pre-regen number, disconnected, and the DB value after the purge
  fired exactly matched the higher live-regenerated number (20 -> 24).
  New `tests/smoke_test_linkdead_purge.py` (4 checks) -- the full
  threshold-crossing cycle isn't practical to keep automated against the
  standing preview/production instances (up to ~6 real minutes with the
  real 300s default), same call `smoke_test_heartbeat.py` already makes
  for its own real-time boundary; the automated test verifies what's
  fast and reliable instead (a raw disconnect produces exactly one
  linkdead body; reconnecting resumes it, not a duplicate or an error).
  Regression-checked against `smoke_test_accounts.py`/
  `smoke_test_multiplay.py`/`smoke_test_combat.py`/
  `smoke_test_positions.py`/`smoke_test_mid_fight_persist.py` (also
  touch descriptor lifecycle/reconnect) -- the first two failed
  identically on the OLD (pre-change) binary too, confirmed pre-existing
  and unrelated; `mid_fight_persist`'s one failure (292 != 295, a 3-HP
  mismatch) didn't reproduce on a clean rerun, a genuine timing flake
  (an extra combat round landing during the disconnect window that one
  run), not a regression -- nothing in this change touches combat.
  wiznews.sql + news.sql entries added (changes what a player finds on
  reconnecting after 5+ minutes away).

Last updated: 2026-07-20 — Session 51 (work): **Socials → DB + full Sneezy
set + `edsocial` editor (both halves of the TODO item), plus a real
crash fix (`cmd_skills.c`) found live along the way.**
- **DB-backed socials, full ~155-verb port.** Socials moved from the old
  compiled 16-entry table to a new `social` DB table (`social_repo.h/.c`,
  loaded once at boot into an in-memory cache — `social_cache_load()`,
  `src/core/socials.c` — same "cache at boot, checked on nearly every
  unmatched command" precedent as `balance_cache_load()`). The full set was
  ported from `sneezymud-master/lib/actions` via a new `db/import-
  socials.py`, verified line-for-line against the real upstream parser
  (`misc/actions.cc`'s `fread_action()`) rather than guessed — including a
  position-code translation table (raw file codes 7–11 do NOT map directly
  to `position_t` ordinals; verified against `misc/create_mobs.cc`'s
  `mapFileToPos()`) that would otherwise have silently mis-gated every
  social with `min_position` 7+. The upstream `$n`/`$N`/`$P`/`$s`/`$S`/`$e`/
  `$E`/`$m`/`$M` token grammar (verified against `comm.cc`'s `act()`) is
  now a generic expander (`social_expand()`), which let the old hand-rolled
  shake/comfort/poke pronoun special-casing be deleted outright — the real
  ported text already carries proper tokens. `point`'s Tobin-original
  held-item form (not upstream) is preserved as a bypass of the generic
  templates. Targeting yourself now gets its own dedicated message
  (`self_auto`/`others_auto`) instead of repeating the no-target one — a
  real behavioral improvement that came along for free. `socials` (the
  list command) is now paged (`descriptor_page_start`), 4 columns per row
  like `help`'s `send_columns()`, since ~155 verbs comfortably overflows
  one screen.
- **Bug found + fixed during testing: self-targeting a social never
  worked.** `find_room_pc()` (the room-name lookup socials use to resolve a
  target) excluded the caller themselves from the search — inherited
  unchanged from before the DB port, when self-targeting wasn't a thing
  worth testing. Caught by the new smoke test's self-target check;
  `find_room_pc()` no longer excludes `ch`, so `smile me` (or `smile
  <own name>`) now actually reaches the `tgt == ch` branch instead of
  silently falling through to `not_found`. Confirmed nothing else in the
  tree called `find_room_pc()`, so no other caller depended on the
  self-exclusion.
- **Housekeeping found while deploying**: `pkill -9 -f 'build/tobin_c'`
  inside a restart script matches the *script's own* command line (which
  literally contains the string `build/tobin_c` in its `pgrep`/`kill`
  invocations) — running it kills the SSH session running the deploy
  script itself, not just a stuck server process, taking production
  offline until the next manual restart. Hit this twice this session.
  Fix: kill by captured PID only, never a second `pkill -f` sweep with the
  same substring the script itself contains. Also confirmed `watchdog.sh`
  (cron, same as the rebuilt home VM per Session 48) runs on the work box
  too, and will race a manual restart if the server is down for more than
  a few seconds — a transient duplicate "bind() failed" log line from the
  watchdog losing that race is expected, not a real error, as long as
  exactly one `tobin_c` ends up listening afterward.
- **Real player impact**: production was down for roughly a minute across
  two restarts while landing this fix, which dropped the connection of the
  only player online at the time (`Jesus`, the box's first immortal). No
  copyover-capable login was available to avoid it (copyover requires an
  authenticated 59+ character; only real player credentials can do that,
  and none were on hand). Worth deciding whether a bot-usable "deploy as
  immortal" credential should exist for this box, mirroring however the
  home VM's `/tmp/deploy_copyover.py` gets its login.
- **Testing**: `tests/smoke_test_socials.py` fully rewritten (17 checks:
  untargeted/targeted/self-target wording, gendered pronoun substitution,
  per-social `not_found` text, `min_position` gating, the paged list
  (drains the pager fully — a verb near the end of the alphabet, `wave`,
  only appears after paging through), abbreviation, and `point`'s
  held-item form). Verified live on production (port 4000); full
  `tests/sweep.sh` regression run before commit.
- **`edsocial` (55+), the editor half.** `edit social [name]` -- bare
  form browses the full list (unpaged, same precedent as
  `show_redit_extra_menu()`'s small-list convention: a builder tool, not
  the paginated player-facing `socials` command), an exact name jumps
  straight to its detail view, `new <name>` creates a blank one. Detail
  view: 8 numbered message fields (columns aligned via a shared `%-22s`
  label width so values start in the same column top to bottom, same
  treatment on the two top fields and the H/P/R/D action row --
  `EDSOCIAL_FIELD_LABELS` in descriptor.c), `H` toggles `hide` (the
  upstream `act()`'s per-recipient invisibility gate, `sys/comm.cc` --
  correctly labeled as inert in Tobin today, since there's no
  invisibility system yet; initially mislabeled as "hide from the
  `socials` list" during development, caught before shipping by
  checking the real upstream semantics rather than guessing), `P` sets
  `min_position` by name (new `position_from_name()`, `being.c` --
  reverse of the existing `position_name()`), `R` renames, `D` deletes.
  Same "commits immediately, no working copy, `social_cache_load()`
  after every write" shape as the Extra Descriptions submenu. New
  `cmd_edsocial.c`, `EDSOCIAL_MIN_LEVEL` (55), help topic + `edit`
  master-topic update, wiznews entry. New `tests/smoke_test_edsocial.py`
  (9 scenarios, including the level gate and the "another connection
  sees the edit live, no restart" check that's the whole point of the
  immediate-commit design).
- **Real crash fix, found live via gdb (not part of this TODO item, but
  serious enough to fix the same session): `cmd_skills.c`'s immortal
  "show every class" view segfaulted the server.** `print_tier()`'s
  header/reagent-note/"(none)" writes had no overflow guard (only the
  per-skill loop did), and `snprintf`'s return value can exceed the
  buffer once truncation starts -- so once the ~300-skill catalog (all
  classes, fully unlocked) overflowed the old 16000-byte buffer, the
  next `outsz - *n` computation underflowed (both unsigned) into a huge
  bogus size, handing `snprintf` free rein to write past the real
  buffer. Reproduced and root-caused live: attached gdb to the running
  production process (`-batch -ex continue -ex "bt full"`, per the new
  standing habit below) during a `tests/sweep.sh` run, caught the exact
  SIGSEGV with a full backtrace pointing straight at the bug, rather
  than treating the resulting 106-failure sweep as a mystery regression.
  Fixed with a new `append_fmt()` helper (guards entry AND clamps `*n`
  to `outsz` afterward, closing both ends of the underflow) used
  consistently everywhere in the file, plus growing the immortal
  buffer to 65536 bytes (real headroom, not just barely enough for
  today's catalog). New regression check in `tests/smoke_test_skills.py`
  (an immortal's `skills` renders the full catalog and the connection
  survives) -- this is the test that actually caught the crash in the
  first place. Also fixed one unrelated stale assertion in the same
  file (`"(level 45)"` -- no warrior skill has ever had that exact
  min_level; the roster was rebalanced after the test was written).
- **New standing habit (user 2026-07-20): always run the server under
  gdb (attached, `continue`) while testing/developing**, not just
  reactively after a crash is suspected -- see `CLAUDE.md`'s "Build /
  run / test" section for the exact command. This is precisely what
  caught the `cmd_skills.c` crash above with an instant, exact
  backtrace instead of an ~85-minute mystery sweep failure.
- **Found, NOT fixed this session, logged as the new TODO.md priority
  item**: a linkdead PC's `being_t` stays fully resident in its room
  forever (by design -- see `descriptor_destroy()`'s comment on why an
  eager save is deliberately avoided), and nothing currently cleans one
  up automatically. Discovered because it's the likely proximate cause
  of a real slowdown/hang while testing this session: months of smoke
  tests that `s.close()` a raw socket instead of `quit`ting had left
  Center Square alone with 70+ linkdead bodies (2762 total player rows,
  597 created just since a checkpoint earlier this session). Verified
  against the original first: `misc/periodic.cc` already has exactly
  this mechanic (`nukeLdead()` at 15 min mortal / 60 min immortal
  linkdead). User-directed deviation for Tobin: a flat 5 minutes for
  everyone. Open design question logged in TODO.md: save-then-destroy
  (matches the original, and the user's own first phrasing of the ask)
  vs. discard-only (matches Tobin's own existing `world_purge_linkdead()`
  precedent, avoiding the same clobber-a-fresher-DB-edit risk
  `descriptor_destroy()` already reasons through) -- needs a decision
  before implementation, not a silent default. **The 597 stray test rows
  from today were NOT cleaned up this session** (mass-deleting 597
  player rows was correctly blocked by the auto-mode classifier as a
  bulk destructive action, and the user redirected toward fixing the
  root cause instead of a one-off manual purge) -- expect a cluttered
  Center Square and a slower server until the auto-purge feature lands
  or someone does a manual `purge linkdead`-style cleanup.
- **Sync-up note for the next session (Home)**: production (`db.kullit.com`)
  is running a clean build of everything in this entry (socials DB port +
  edsocial + the `cmd_skills.c` crash fix), verified via two full
  `tests/sweep.sh` runs with gdb attached the whole time (zero crashes on
  the second, clean run -- 134 passed / 21 failed, and the 21 are
  pre-existing/environmental: `set`/`news`/etc. are SYNC.md's own
  documented rotating flakes, confirmed by rerunning `set` standalone
  clean; `accounts`/`zones` failed standalone too but look like
  collateral from the linkdead-glut room slowdown above, not a real
  regression from anything in this entry -- worth a clean rerun once
  that's fixed). **`tests/smoke_test_edsocial.py` itself was NOT
  confirmed passing this session** -- written and believed correct, but
  live verification kept getting sidetracked by the linkdead-glut
  discovery; run it fresh at Home before treating `edsocial` as fully
  done. All of this session's `db/sneezy/*.sql` changes (including
  edsocial's help topic and the two newest wiznews entries) are applied
  to production's DB as of the end of this session -- `db/apply-tobin-
  schema.sh` re-run clean, no separate step needed at Home beyond a
  normal `git pull` + the usual schema-apply-after-pull habit.

Last updated: 2026-07-20 — Session 50 (work): **redit Extra Descriptions,
builder half.**
- **`edit room` menu 8: Extra Descriptions submenu.** The mortal-facing
  half (`look <keyword>` reveals a room's hidden detail) shipped Session
  49; this closes out the item with an in-game authoring UI so builders
  no longer need direct SQL. New `CONN_REDIT_EXTRA_MENU` / `_ITEM` /
  `_KEYWORDS` / `_DESC` / `_DELETE_CONFIRM` / `_DELETE_ALL_CONFIRM`
  states in `descriptor.c`, entered via a new "8) Extra Descriptions"
  line on the existing numbered room menu. List (queried fresh from
  `roomextra` every time, not cached), Add (keywords, then straight into
  the shared line editor for the description text), a per-item detail
  view (mirrors the Exits submenu's target/door/conditions/remove
  shape) offering Keywords/Description/Delete, and Z) Delete ALL (the
  original's `DeleteExtraDesc()` bulk action, `misc/create_rooms.cc` --
  Sneezy redit items 6 & 10, both now covered).
- **Deliberate deviation from the rest of `edit room` (documented per
  house rule):** every other field here (name/description/flags/sector/
  capacity/height/exits) edits a `d->redit_work` WORKING COPY, only
  written to the DB on (S)ave. Extra descriptions instead commit
  IMMEDIATELY, on every add/rename/edit/delete. Reason: extras were
  never modeled in `room_t` at all -- `room_repo_extra_desc()` (the
  mortal-facing lookup) already hits the `roomextra` table fresh on
  every `look <keyword>`, no in-memory cache exists to keep in sync --
  so buffering them in a NEW parallel in-memory structure just to fit
  the working-copy pattern would be one more source of truth for no
  real benefit. Same "commits immediately, no working copy" precedent
  the account editor already established (`edaccount_id`'s comment,
  `descriptor.h`). This also means extras are the one part of `edit
  room` that ignores (Q)uit-without-saving/(D)iscard -- there is nothing
  to discard, since nothing was ever staged.
- New `room_repo.h`/`.c` functions: `room_repo_extra_list()` (array-out,
  same convention as `ignore_repo_list()`), `room_repo_extra_get()`
  (exact-name lookup, vs. the existing keyword-prefix-search
  `room_repo_extra_desc()`), `room_repo_extra_save()` (upsert --
  INSERT...ON DUPLICATE KEY UPDATE, so "add new" and "edit description"
  share one code path), `room_repo_extra_rename()`, `room_repo_extra_
  delete()`, `room_repo_extra_delete_all()`. A rename that collides with
  a DIFFERENT entry's exact keyword string (the `roomextra` primary key
  is literally `(vnum, name)`) fails cleanly at the DB layer (duplicate-
  key error -> `db_query()` returns false) -- no separate existence
  pre-check needed, and the failed rename leaves both entries fully
  untouched (single-statement UPDATE, no partial write).
- **Testing**: new `tests/smoke_test_redit_extradesc.py` -- add (keywords
  -> description), the new entry appearing numbered in the list, rename,
  a colliding rename refused cleanly, description edit, single delete,
  delete ALL, cancelling Add (blank at the keywords prompt) leaves no
  row, aborting the description editor on a BRAND-NEW add leaves no row
  and returns to the LIST (there's no item yet to show a detail view
  for) vs. aborting on an EXISTING entry's description leaves it
  unchanged and returns to THAT item's detail view instead (tracked via
  a new `redit_extra_is_new` scratch flag, `descriptor.h`), and an
  end-to-end check that `look <keyword>` immediately reveals what redit
  just authored. Wiznews entry added (builder/immortal-only change, no
  news entry per house rule -- no mortal command changed).
- **Housekeeping found + fixed while resuming this session**: the work
  box's own git checkout had drifted behind origin (stale by ~100
  commits, including two format-truncation warnings from a much earlier
  session that had only been fixed via an ad-hoc tar-sync, never
  actually committed on this box) -- reconciled via `git stash` + a
  clean fast-forward pull before any of the above landed. Also added a
  per-test wall-clock timeout to `tests/sweep.sh` (a single hung test --
  `smoke_test_weapon_depth.py`'s old poll-vs-combat-round-interval bug,
  independently already fixed by home the same way -- had stalled a full
  sweep on this box for 2h+ before this) since 100 upstream commits had
  landed, and this box's own copy of the sweep script had none.
- **Two more bugs found + fixed while verifying this deploy** (deploying
  to production surfaced both -- neither is a regression from the work
  above, both pre-date this session):
  - `tests/smoke_test_redit.py`'s own final DB-verification check
    expected `room_flag == 8` (INDOORS alone), but the room it bootstraps
    already sets ALWAYS-LIT (bit 0 = 1) -- a default added in a LATER
    commit (the darkness-mechanic cross-test-hazard fix) that never
    updated this one assertion. `1 | 8 = 9` is what actually persists,
    and always has since that default landed; the test itself was stale,
    not the save path. Root-caused via a one-off instrumented copy of
    the test (`/tmp/debug_redit.py`, not committed) rather than guessing
    from a bare traceback -- printed the exact `BASE`/raw query output
    to confirm precisely which field diverged before touching anything.
    Fixed the one hardcoded `"8"` -> `"9"`, with a comment explaining why.
  - `tests/smoke_test_redit_extradesc.py` (new this session) had its own
    bug: the `mariadb -N` CLI escapes an embedded real newline inside a
    field's own VALUE as the literal two characters backslash+n, not an
    actual newline byte -- descriptions saved via the line editor always
    end in a real `\n`, so the test's naive `.strip()` never found it
    (comparing "text.\n" against "text." failed even though the exact
    right value was sitting in the DB the whole time). Fixed by
    unescaping `\\n` -> real `\n` in `extras_for()` before comparing.
  - Both surfaced only because production was actually exercised after
    deploy, not assumed clean from a passing build -- the standing "test
    the golden path for real" habit paying for itself.

Last updated: 2026-07-19 — Session 49 (home): **User batch (tips toggle,
`level` command, prompt expansion, animal-race gold) + Mount/riding
system.** Worked a batch of 5 user-reported items logged earlier this
session (a wiznews/news pager truncation bug and a weather-leaking-
indoors bug were already fixed and deployed before this entry — see the
TODO.md "User batch 2026-07-19 (evening)" section for both writeups),
then continued down the numbered audit backlog into task 13.

- **`toggle tips`** — dedicated `PLR_NOTIPS` pflags bit (being.h),
  independent of `toggle newbie` (previously the only way to silence the
  periodic tip echo was leaving the whole newbie channel). New
  `tests/smoke_test_toggle_tips.py` (8 checks).
- **`level` command** — new `cmd_level.c`, "You have X experience and
  need Y more experience to level," reusing `progress_xp_for_level()`
  (the same curve `progress_add_xp()` levels a player up against) so it
  can't drift out of sync with a real level-up. New
  `tests/smoke_test_level.py` (8 checks).
- **Animal-race mobs no longer drop gold** — AskUserQuestion-confirmed
  scope: Tobin's 6 PLAYER races have nothing literally "animal" among
  them, so this gates the existing mob gold-drop-on-kill by the mob's
  upstream `mob.race` column (previously wholly deferred/display-only)
  being a mundane real-world creature race. `mob.race` now loads into
  `mob_proto_t` and copies onto a new `being_t.mob_race` at spawn time;
  new `mob_race_is_animal()` (being.c, 46-case lookup, verified index-
  by-index against the name table via a live Python check on the VM)
  excludes fantastical/sapient races and plants/oozes/elementals. XP
  unaffected. New `tests/smoke_test_animal_no_gold.py` (6 checks).
- **Prompt expansion** — `prompt exp`/`prompt expneed` (experience,
  experience-needed-to-level) join hp/gold/vit; new `prompt all` sets
  every available one at once. Mana/piety remain blocked (still don't
  exist as resources). Checks folded into the existing
  `smoke_test_parser_display.py` prompt section (15 checks) rather than
  a new file.
- **Spell-help-placeholder report — confirmed already fixed, no code
  change.** The user re-reported `help haste` showing generator
  placeholder text; turned out commit `ca55246` (2026-07-18, the day
  BEFORE the report) already generated real per-spell help topics for
  all 271 skills/spells. Verified live (zero placeholder matches across
  424 help_topic rows, `smoke_test_help_topics.py` clean) and closed
  the TODO item as confirmed-resolved rather than redoing finished work.
- **Mount / riding system** (Sneezy → Tobin feature audit, task 13) —
  scoped way down from Sneezy's real system (misc/riding.cc: no Deikhan
  class, no height/weight-ratio gauntlet). AskUserQuestion-confirmed
  scope with the user: a "fuller" tier (mounted combat bonus +
  skill-gated mount success) plus a simple immortal-stocked stable using
  the existing shop system. Any class can `ride`/`mount <target>` a
  HORSE-race mob (`mob_race_is_rideable()`, being.c — reuses the same
  `mob.race` plumbing the animal-gold item just wired up) via a new
  universal "riding" skill (one entry per class, same learn-by-doing
  shape as cast/pray/peek) gated by `ch->progress.combat_disc_pct` same
  as every other Combat-tier skill. Success links `being_t.mount`/
  `.rider` bidirectionally (mirrors `fighting`'s single-pointer shape)
  and sets `POSITION_MOUNTED` (already existed in the enum, unused until
  now) on both sides — the mount stops wandering/aggroing while ridden
  since mob_ai.c's own gates already key off `position ==
  POSITION_STANDING`. `cmd_move.c`: mounted movement costs half vit
  (rounded up), the mount follows the rider room-to-room, and entering
  an INDOORS room forces a dismount (mount stays behind, doesn't move
  into the building). `combat.c`/`being_total_ac()`: a small mounted
  attack bonus and AC bonus, and mounted defenders are excluded from the
  existing non-standing-defender "easier target" bonus. `being_destroy()`
  gained bidirectional mount/rider teardown (same treatment as
  `fighting`/`last_heal_target`). Stable: a genuinely new
  `shop.is_stable` column (not a reused/fabricated `spec_proc` value —
  Sneezy's own `SPEC_PROC_STABLE_MAN` was verified dead code, never
  really assigned to a mob) seeded true for shop_nr 164 — user-selected
  Petir's "Carnivorous Companions" (room 564), a real seeded shop that
  was completely non-functional before this (empty `shopproducing`,
  framed in its own room description as "buy a trained familiar" — also
  earmarked as the future Pet/charm shop). New
  `tests/smoke_test_mount.py` (17 checks: mount/dismount success and
  failure, movement discount, indoor auto-dismount, purge-teardown,
  stable purchase). Verified live on both preview and production;
  regression-checked against `smoke_test_positions.py`/
  `smoke_test_combat.py`/`smoke_test_vitality_terrain.py` (still running
  as of this write-up — check their result before treating task 13 as
  fully closed if picking this up cold).
- **Also logged, not yet started**: a new user request — "immortals can
  see inventory when looking at a mob or player and can also see the
  contents of any container they carry" (extend `look <target>` for
  immortal viewers) — see TODO.md's newest "User batch (later evening)"
  section.
- **Known gap found and partially repaired**: this session's player-
  facing work had NOT been getting `news.sql` entries (CLAUDE.md house
  rule: every command/behavior change affecting players gets one, in
  ADDITION to wiznews.sql) — only wiznews.sql was kept current in the
  moment. Backfilled at the end of this session for the 5 items above
  (tips/level/prompt/animal-gold/mounts); the two EARLIER same-session
  fixes (wiznews pager truncation, weather-indoors) were treated as
  under-the-hood bugfixes and deliberately left wiznews-only, matching
  how Session 48's own bugfixes (the three stray-`\r\n` fixes) also
  didn't get news.sql entries. Worth double-checking this rule is being
  followed going forward — it's easy to drop when working fast down a
  backlog.

### Session 48 (home): **Total VM loss and rebuild,
News follow-ups, three stray-\r\n formatting fixes.** The old VM
(192.168.254.200) was lost entirely — no DB backup existed, so all live
player data is gone. Rebuilt from scratch on a fresh Fedora Linux 44 Server
VM at the same IP: SSH key auth, a GitHub deploy key (turned up the repo had
been silently un-pushed 93 commits behind `origin/main` this whole time —
not data loss, just a broken `git push` step nobody had caught), packages,
upstream + Tobin DB seed, zero-warning build, fresh `TOBIN_DB_PASS`/
`TOBIN_WIPE_PASSWORD` in `.env.local`, firewalld ports opened, `watchdog.sh`
back in cron. First player (`Jesus`) promoted to level 60 via direct SQL
(no self-service bootstrap path exists for the very first immortal). Then,
while a full `sweep.sh` regression run went in the background (which itself
caught a real bug: setting a real DB password for `mud` broke every smoke
test's bare `mariadb sneezy` calls, fixed with `~/.my.cnf`) — shipped "News
follow-ups" (edit/delete existing news+wiznews in-game, unseen-news login
notice) and three stray-`\r\n` formatting fixes in `look`/the game prompt
the user found by hand. Both deployed to a separate preview instance (port
4003) alongside the untouched production instance (port 4000) specifically
so the still-running sweep wouldn't be disrupted; production itself gets
rebuilt+restarted only once the sweep finishes clean.

- **News follow-ups** (user 2026-07-17 batch: "edit/delete existing news
  in-game (addnews only creates); show unseen news at login (per-player
  last-seen)") — see TODO.md/wiznews.sql for the full writeup. Short
  version: `news_repo_add` became `news_repo_upsert` (INSERT ... ON
  DUPLICATE KEY UPDATE instead of a straight INSERT that failed on the
  duplicate title), `edit news`/`edit wiznews` preload an existing
  headline's body the same way `edit help` already does, both gained a
  `delete <headline>` sub-form, and a new `player.news_last_seen_id`
  column drives a login notice. New `tests/smoke_test_news_followups.py`
  — its own first draft had a real bug worth remembering: a pager-drain
  `while "ENTER for more" in full:` loop with no iteration guard, which
  hung for 30+ minutes against a real feed with many real items before
  being caught (diagnosed via `/proc/<pid>/wchan` showing
  `poll_schedule_timeout` plus zero bytes pending on both ends of the TCP
  connection — nothing server-side was wrong, the test client was just
  spinning). Fixed by copying `smoke_test_news.py`'s existing `guard < 60`
  convention, which that file already used for the exact same pattern.
- **Three stray `\r\n` fixes** (user, found by hand while testing the
  rebuilt box): (1) `look`'s room description had a spurious blank line
  before Exits — root cause was DATA, not the format string: ~95% of
  seeded `room.description` rows carry their own trailing `\n` (confirmed
  via `HEX(RIGHT(description,4))` on a real row), stacking with
  cmd_look.c's own appended `\r\n`. Fixed by trimming a COPY of the
  description before rendering, not the stored row (redit/stat/etc. still
  see the raw text). (2) cmd_look.c inserted an extra blank line between
  the Exits line and the room-contents listing — deleted outright. (3)
  game_loop.c's per-turn prompt sent `"\r\n\r\n"` before `"HP: ... > "` in
  both branches (flags-on and flags-off) — double what the adjacent
  comment says was ever intended ("insert A \r\n before each new
  prompt"); both now send a single `\r\n`. Verified against the existing
  `smoke_test_exits_display`/`_look_capitalization`/`_look_equipment`/
  `_look_fixture_order`/`_room_stacking`/`_parser_display` tests (all
  still pass) rather than a new dedicated test, since this is pure
  whitespace with no new behavior to assert.
- **Trigger `wait`/`say`, zone opcode `A`, world-wide `max_exist`, `scan`
  range trim** — same-day continuation, not detailed again here; see
  TODO.md/wiznews.sql for the full writeup of each (search "wait", "Zone
  opcode", "max_exist", "scan").
- **`lock`/`unlock` for doors and containers** (TODO.md "Keys unlocking
  doors", now unblocked) — new `cmd_lock.c`. A key is matched by its own
  object vnum, not any val[] field on the key itself, confirmed against
  the real SneezyMUD C++ source before writing anything
  (`has_key()`/`keyCheck()`, misc/movement.cc); `room_t` gained
  `exit_key[]` to actually carry `roomexit.key_num` through (previously
  loaded and silently discarded). 1,141 real seeded doors and dozens of
  containers already carry working key data. New
  `tests/smoke_test_keys.py` (23 checks); `cmd_table.c` additions
  verified with `tests/tools/cmd_abbrev_check.py` (also patched — it was
  missing 3 level macros added by later sessions and errored out before
  this).
- **Mid-fight HP persistence** (TODO.md) — HP was only saved at
  defeat/quit; a mid-fight disconnect (crash, or a losing player quietly
  pulling the plug) reloaded at whatever HP was last saved BEFORE the
  fight started, silently undoing all damage taken.
  `combat_process_run()` now saves both PC participants' HP after every
  round the fight is still ongoing (reuses the existing
  `player_progress_save()` call, not a new mechanism). Limb HP
  deliberately NOT covered — it isn't persisted at all yet by any path,
  defeat included; separate "Meaningful limb damage" item. New
  `tests/smoke_test_mid_fight_persist.py`: fights a tanky sandbox mob,
  hard-closes the socket mid-fight (no `quit!`), reconnects — HP landed
  exactly where it was, not reset to the pre-fight max. Found and fixed a
  real stale-test bug along the way: `smoke_test_combat.py`'s PvP setup
  predated the PK opt-in feature, so two fresh mortals could no longer
  `attack` each other without `toggle pk` first.
- **TODO.md backlog triage**: closed 4 stale entries found while sweeping
  for unblocked work — `examine`, `drink`/`sip`, and "Typed logs" were
  already fully shipped in earlier sessions and never checked off;
  "Personalized immortal log messages" was based on a misread of the
  original engine (LOG_JESUS/LOG_PEEL are each a named developer's own
  ad-hoc scratch debug channel, not a fixed set of messages to build —
  Tobin already has the real infrastructure for it).

### Session 47 (home): **Practice-system follow-up
polish, real per-skill proficiency (Sneezy-style learn-by-doing), and `set`
command growth.** Direct continuation of Session 46's practice redesign,
worked live against the running server with the user testing each fix
in-game between iterations.

- **Practice-system polish** (playtesting found 4 real UX bugs in Session
  46's redesign): bare `practice` used to require a guildmaster in the room
  just to show your OWN percentages/points — now always shows status
  anywhere, guildmaster only adds a training reminder. That reminder used to
  invite all three disciplines regardless of which guildmaster tier was
  actually present — now only suggests the one that guildmaster teaches
  (the underlying gate was already correct, only the flavor text was
  misleading). `practice <yourclassname>` (e.g. `practice warrior`) now
  works as a synonym for `practice basic`, matching how `skills` already
  labels that tier by class name. New `goto <classname>` (e.g.
  `goto thief`) gives directions to that NAMED class's own Basic
  guildmaster, not just the caller's own class — checked last in `goto`'s
  landmark chain so it never shadows the fixed landmarks.
- **`practice <discipline>` reworked to be useful anywhere**: user tested
  `practice combat` away from a guildmaster and got flatly refused — not
  useful for checking progress. Split on whether an explicit count is
  given: bare `practice combat` now always shows that ONE discipline's
  skill/spell listing with each accessible skill's own proficiency
  percentage (new `practice_show_discipline()`, cmd_practice.c), no
  guildmaster required; a guildmaster present adds a training reminder.
  Only `practice combat <count>` (explicit count) still spends points, and
  that form is unchanged — still needs the matching guildmaster. Real
  behavior change from before (bare form used to silently spend 1 point);
  deliberate, since checking status shouldn't cost a scarce resource.
- **Real per-skill proficiency — Sneezy-style learn-by-doing** (user:
  "they should gain access to a skill by practicing, but the actual gain in
  proficiency should be gained as in sneezy"). Researched the actual Sneezy
  mechanic first (`code/code/misc/skills.cc`/`discipline.cc` in the
  reference tree) rather than guessing, then locked scope via
  AskUserQuestion before building (same precedent as the practice
  redesign): real d100 success roll (not cosmetic), hooked into
  `cast`/`pray` plus every already-mechanically-wired skill
  (`settrap`/`disarmtrap`, dual wield). Discipline percentages
  (`*_disc_pct`) now ONLY gate tier ACCESS; each individual skill/spell
  additionally has its OWN 0-100% proficiency (new `player_skill` table:
  player_id, skill_name, pct, last_gain_at) that climbs via
  `skill_learn_from_doing()` on every attempt, win or lose — gain chance
  shrinks as it nears a ceiling set by the relevant discipline percentage
  (raising the discipline via `practice` raises the ceiling; using the
  skill climbs toward it), Wisdom softens the diminishing-returns curve
  (integer exponent 1/2/3 by Wisdom tier, avoiding a new libm dependency —
  same no-`math.h` precedent as `practice.c`). `cast`/`pray` roll
  `skill_roll_success()` against the resulting proficiency for every
  attempt (component/symbol still consumed either way; failure fizzles —
  "You fumble the casting/prayer..."). `settrap`/`disarmtrap` roll the
  same way; a fumbled disarm deliberately does NOT spring the trap on the
  disarmer (kept non-punishing, v1 scope). Dual wield gets learn-by-doing
  only (no roll — it's a passive stance, PCs only). `skills` shows each
  known skill's proficiency in brackets (`bash [34%]`). New
  `skill_repo.h`/`.c` (DB access) + `skill.c` additions (`skill_find()`,
  `skill_proficiency()`, `skill_learn_from_doing()`, `skill_roll_success()`).
  **Gap**: no dedicated smoke test written this session (time-boxed to
  shipping the mechanic + docs) — needed before the next full sweep is
  trusted to catch a regression here.
- **`set` command grows practices/basic/combat/advanced fields** (user:
  "need the ability for the set command to adjust practices and any other
  stat you can think of, we'll get in the habit of updating set with new
  items as we go" — saved as a standing habit memory). 4 new
  `apply_field()` cases in `cmd_set.c`; no new online-sync code needed
  since the existing sync loop already copies the whole `progress_t`
  struct wholesale.
- **Pagination for long-output commands** (user: "when typing skill or any
  other item that has long output, pass it to pagination"). `skills`,
  `bug`, `idea`, `rules` all built an unpaginated buffer and dumped it in
  one `descriptor_send()` — same bug class `news`/`wiznews` already had
  fixed in an earlier session. All four now go through the existing shared
  pager (`descriptor_page_start()`). `skills`' immortal branch also
  restructured to accumulate all 6 classes into one buffer and page it
  once, instead of sending each class's block immediately per-iteration
  (which bypassed pagination entirely).
- **`snoop` notifies on target disconnect** (user: "when you are snooping
  and the player loses connection, send a message to the snooper saying
  you are no longer snooping <target name>" — found while the user was
  live-snooping a test character during the sweep-failure investigation
  below). `descriptor_destroy()` already unhooked the `snoop_target`/
  `snooped_by` link in both directions on teardown, silently — now tells
  the snooper first.
- **Major discovery: a PRE-EXISTING connection-handling bug, confirmed
  NOT caused by this session.** The pre-push full sweep (113 passed, 11
  failed) initially looked like a serious regression — most failures were
  live hangs in combat-adjacent tests. Traced it methodically: A/B-reverted
  the session's own two `combat.c` changes (still hung, ruling them out),
  then built a completely pristine copy of the last commit (`74ead6d`,
  zero of this session's changes) on a separate port (4001) sharing the
  same DB, isolated from the live server — **the identical hang reproduced
  there too.** This is a genuine pre-existing bug: a connected session can
  silently stop responding under concurrent load (2-3 connections active
  at once), with the server itself staying healthy throughout (idle CPU,
  zone resets still firing, no MySQL deadlock, normal fd count) — looks
  like a `select()`/descriptor dispatch issue, not a code path this
  session touched. Logged as a detailed, prioritized TODO item with next-
  steps for whoever investigates (likely `game_loop.c`'s `maxfd` tracking
  or a descriptor state that silently excludes a connection from
  dispatch). Affects 11 existing smoke tests; none of them are actually
  regressions.
- **Two genuinely stale tests updated** (the OTHER 2 of the 11 sweep
  failures, unrelated to the bug above): `smoke_test_skills.py` and a full
  rewrite of `smoke_test_practice.py`, both still testing the pre-redesign
  behavior. Verified as far as possible — every check that ran before
  hitting the pre-existing hang (10/19 and 6/16 respectively) passed
  clean, validating the rewrites even though a full end-to-end pass is
  blocked on the bug above.
- **CORRECTION, same day (continued session): the "connection-handling
  bug" above was a false positive, not a real bug.** `smoke_test_kill.py`'s
  `recv_all()` helper blocks for its FULL timeout on every call (never
  exits early just because a complete reply arrived); `make_player()`
  calls it ~9 times per character, and the full script creates 5
  characters — 45+ seconds of legitimate, correct blocking with zero bugs
  anywhere. Every "hang" this session (and presumably the original sweep's
  11 failures) was the script still correctly running, checked against a
  15-25s "stuck" threshold that was simply too tight. Proved by re-running
  the exact same repro with a 90s timeout: completed in 72.8s, all checks
  passed except one small unrelated flake (a bystander's trailing prompt
  arriving slightly after `recv_until()`'s 5s deadline, plausibly due to
  Center Square being cluttered with a couple dozen linkdead test
  characters from today's repeated runs — a DB-cleanup item, not a code
  bug, and not yet investigated further). See TODO.md's RESOLVED entry
  for the full trail. One real, unrelated improvement survives from the
  investigation: `socket_write()` silently dropped data on `EAGAIN`/short
  writes with no retry (a genuine, if never-yet-triggered, bug) — replaced
  with a proper per-descriptor output backlog (`descriptor_write()`/
  `descriptor_flush_output()`, `writefds` watched in `game_loop.c`,
  flushed before `copyover`'s `execl()` too). Deployed to the live Home VM
  server via a hard kill+restart (user pre-authorized this going forward:
  "when you need a reboot, just hard boot as i may not be here to
  copyover"), binary md5 confirmed live.
- **Deploy mechanics this session**: iterated live against the Home VM
  (192.168.254.200) via per-file `scp` + `make` (plain-make, not cmake —
  cmake isn't on the Windows dev box) + `bash db/apply-tobin-schema.sh` +
  `copyover`, with the user manually running `copyover` in-game each time
  and reporting results back live. One real deploy gap found: an early
  `copyover` request was answered before the rebuild had actually finished
  (binary mtime AFTER the copyover timestamp) — the "fix" the user tested
  was still the old binary. Lesson: confirm the build's `md5sum` matches
  the running process's `/proc/<pid>/exe` before declaring a fix live, not
  just that `make` exited 0.
- Clean builds throughout, zero warnings. Full sweep run before this
  session's push (113 passed, 11 failed — every failure now understood
  and none are regressions, see above).

Last updated: 2026-07-17 — Session 46 (home): **`cmd_table.c` alphabetized,
as a pure refactor with zero behavior change.** This finishes the reorder
that Session 44 deliberately left on hold (its wiznews entry said "further
alphabetizing within each tier is on hold pending a follow-up decision").

- **The reframing that unblocked it.** The original plan was "pin the
  documented exceptions, alphabetize the rest, accept the fallout." But this
  table's *order is its semantics* — `cmd_dispatch()` takes the FIRST entry
  the caller can see whose name starts with the typed verb — so that plan
  would silently rewire player muscle memory (`a` attack→affects, `c`
  close→cast, `h` hit→help, `p` pray→practice). Reframed as: *find the
  alphabetically-smallest ordering that still resolves every abbreviation
  exactly as it does today.* That's a topological sort — derive precedence
  edges mechanically (for every prefix at every level, today's winner must
  keep preceding the other matches), then Kahn's algorithm with an
  alphabetical min-heap tiebreak yields the lexicographically smallest legal
  order.
- **It cost far less than expected.** Only **16 of 74** mortal entries and
  **1 of 39** immortal ones must break strict A-to-Z, and every one is a
  local swap of an adjacent pair: `say`<`save` ("sa"), `score`<`scan`
  ("sc"), `sit`<`sip` ("si"), `who`<`whisper` ("wh"), `look`<`limbs` ("l"),
  `rest`<`remove`/`rent` ("r"/"re"), `drop`<`drink` ("dr"),
  `exits`<`examine` ("ex"), `continue`<`consider` ("con"),
  `news`<`newbie` ("new"), `inventory`<`idea`/`immort` ("i"),
  `attack`<`affects` ("a"), `close`<`catchup`<`cast` ("c"/"ca"),
  `hit`<`help` ("h"), `pray`<`practice` ("p"), `wiznews`<`wizhelp`/`wiznet`
  ("wiz"). Each is marked inline with the abbreviation it protects. The
  movement head stays pinned; the mortal/immortal tier split is unchanged.
- **Verified mechanically, not by eye** — this collision class had already
  bitten the table three times. A script resolves all **432** prefixes at
  all **8** distinct levels against both the old and new tables and diffs
  them: **zero differences**. Kept in-repo at
  `tests/tools/cmd_abbrev_check.py`; run it against any future reorder
  (`git show HEAD:c_port/src/cmd/cmd_table.c > /tmp/old.c` for the
  baseline). It also has a report mode listing each command's shortest
  reachable abbreviation and what shadows it.
- **Stale comments corrected.** The per-entry prose had drifted from
  reality: it claimed `"c"` reached `color` and `"h"` reached `help`, but
  `close` and `hit` had quietly owned those since they were added ahead of
  them. New comments were written from the tool's measured output rather
  than from prose, and the file's header now says to trust a prefix diff
  over any comment, including its own.
- **Pre-existing test bug fixed (not a regression from this change).**
  `smoke_test_immortal_cmds.py` asserted `users` prints the raw IP
  `127.0.0.1`, but `descriptor_display_host()` (descriptor.c:353) returns
  the reverse-DNS hostname once the off-thread lookup lands, falling back to
  the IP — loopback resolves to "localhost" (live game log confirms:
  `[localhost]` ×28). The test predated hostname resolution and was racing
  the resolver. Now accepts either. **Confirmed pre-existing by A/B**:
  rebuilt the ORIGINAL table and reproduced the identical failure.
- Clean build, zero warnings. Smoke: `immortal_cmds`, `doors`,
  `exits_display`, `room_stacking`, `alignment` (the test that originally
  caught `set`→`settrap`), `goto_guildmaster`, `limbs_cmd`,
  `mortal_toggle`, `news`, `save`, `socials`, `trap`, `wiznews`,
  `practice`, `combat`. wiznews + TODO entries added.
- Environment note: the Home VM had just booted when this session started —
  SSH timed out and ping failed for ~30s, which looks exactly like the
  bridged-over-WiFi failure the adapter config invites. It was only the boot
  race. Zone boot also takes ~2s after the process starts, so a restart
  needs a wait-for-"Listening on port" gate, not a fixed `sleep 2`.

Last updated: 2026-07-13 — Session 45 (background task, follow-up to
Session 44's sweep triage): fixed the pulse-scheduler timing bug flagged
below rather than just noted. `src/game_loop.c`'s pulse counter used to
advance once per main-loop iteration; `select()` returns immediately
whenever a socket has data ready (not just on its 100ms timeout), so
under concurrent connection traffic every `pulse_register()`-based system
(HP regen, combat rounds, the game clock, mob AI, zone aging, puddle
decay) could fire far more often than its constant's real-time meaning
implied. Fixed by gating pulse advancement on real elapsed wall-clock
time (`clock_gettime(CLOCK_MONOTONIC)`, `now_usec()`/`next_pulse_due` in
game_loop.c) instead of loop iterations, with a bounded catch-up
(`MAX_PULSE_CATCHUP` = 50, ~5s) so a genuine stall doesn't queue an
unbounded burst. Rebuilt, deployed, restarted; verified via
`smoke_test_trigger_seed.py` (the flaky "damage 2" check that originally
surfaced this now passes clean, previously observed net -1 HP instead of
-2 from a regen tick racing in), plus `smoke_test_combat.py`,
`smoke_test_zones.py`, `smoke_test_gametime.py` (confirms the clock still
advances by exactly the intended 15-mud-minute tick), and
`smoke_test_multiplay.py`. wiznews + TODO entries added.

Last updated: 2026-07-12 — Session 44 (home): large batch this session,
**committed locally as `06995c5` (119 files) but NOT YET PUSHED** — blocked
on a clean full sweep (see failures below, mid-triage).
- **`goto` redesign**: mortal-visible now (was immortal-only). Gives
  walking directions via a new BFS pathfinder (`goto_bfs()`, cmd_goto.c)
  instead of teleporting — `goto guildmaster` (own-class Basic guildmaster),
  `goto rent` (room 557, The Roaring Lion Inn), `goto surplus` (room 563).
  Immortals keep instant vnum/player teleport (`goto <vnum>`, `goto <name>`).
  BFS lazily loads not-yet-resident rooms on demand (`goto_get_room()`) —
  first draft dead-ended at the edge of loaded territory without this.
- **Char creation reorder** (user: "selection of race and class should go
  before picking attributes"): flow is now name → race screen (1-6) →
  class screen (1-6) → attribute point-buy → `done` → alignment screen
  (1-3) → welcome/playing. This broke wire-protocol assumptions in ~130+
  test files; migrated via script + manual fixes, re-audited to 0
  remaining at the time. **However the sweep below found 4 more files the
  audit missed** — see Known Issues.
- **`stat player <name>`**: new subcommand, stats an offline/online player
  (fixed a `%li`-format-string bug in `db_query()` along the way — it only
  supports `%s/%i/%f/%r`, not real printf specifiers).
- **`open door <direction>`**: was missing entirely (bare-direction form
  only existed). Added, careful not to let "door" shadow "down"'s "d"/"do".
- **`cmd_table.c` mortal-first/immortal-second reorder** (user: "place
  immortal commands lower in the list... immortals are less likely to make
  mistakes"): full table reorder, immortal commands now sorted after all
  mortal ones. Found/fixed a `set`/`settrap` collision and a `get`/`goto`
  abbreviation collision along the way.
  **PAUSED sub-task**: further alphabetizing each tier block was requested,
  then explicitly halted by the user mid-edit ("STOP what you are doing and
  wait for the user to tell you how to proceed") because alphabetizing
  would break the movement-must-be-first invariant. Do not resume without
  explicit user go-ahead.
- **Help content fixes**: removed a leftover "Sneezy always warned about"
  phrase from `help playing`; fixed the hand-authored `classes` topic
  (typos, missing Druid). Two stale test assumptions ("goto is immortal-only")
  fixed by swapping to `transfer` as the example instead — per user
  instruction "leave [the help architecture] alone and resolve conflicts as
  they occur" (no help/wizhelp split was done).
- **Practice system redesign — DESIGN LOCKED, ZERO CODE WRITTEN YET.** New
  three-discipline system (Basic/Combat/Advanced, replacing the old
  two-discipline flat-step version in `cmd_practice.c`). Locked decisions:
  practice points on level-up = `random(6,8) + round(wisdom_bonus *
  wisdom_practice_modifier)` where `wisdom_bonus = floor((wisdom-120)/10)`
  and the modifier is a new gamewide `game_config` row (default `1`,
  adjustable via a new `balance wisdom` subcommand); each point spent raises
  a discipline by a random 1-2%; Advanced unlocks only once BOTH Basic and
  Combat hit 100%. Guildmasters: Basic = existing level-51 mobs (unchanged),
  Advanced = existing level-100 mobs (unchanged), Combat = **6 NEW
  per-class mobs** (not 1 shared mob — reversed from the user's original
  phrasing after confirming 6-per-class is actually simpler in code). `goto
  guildmaster`→Basic (existing), `goto combat`→per-class Combat trainer
  (needs class-aware routing, same pattern as Basic), `goto advanced`→
  always refuses with flavor text, no pathfinding. Command syntax (added
  2026-07-13): `practice <discipline> [<#>]` -- e.g. `practice combat 7`
  spends 7 points in one command instead of seven separate ones; bare
  `practice <discipline>` still spends exactly 1. The spend loop must
  stop early and report the actual count landed if points run out or the
  discipline hits 100% partway through a requested `<#>`. Next
  implementation step: DB schema (`player_progress.combat_disc_pct` +
  `player_progress.practice_points` columns, `game_config` row), then find
  the level-up code path to wire in the award (already located:
  `progress_add_xp()` in `src/core/being.c:647`, called from
  `combat_defeat()` in `src/core/combat.c` around line 466-488 — see
  TODO.md's fully-ordered implementation plan for this feature).

**Sweep triage — COMPLETE (this session).** A full sweep came back 101
passed, 23 failed. Every failure was individually re-run standalone and
root-caused; none turned out to be a real product-code regression from
this session's own changes. Breakdown:
- **Sweep-only pollution, not real bugs** (pass clean standalone, no
  action needed): `smoke_test_goto_guildmaster.py`, `smoke_test_transfer.py`,
  `smoke_test_bleeding.py`, `smoke_test_trigger.py`, `smoke_test_redit.py`.
- **Pre-existing, tracked separately, not fixed here**:
  `smoke_test_immortal_cmds.py` and `smoke_test_logging.py` both assert an
  IP-address-shaped regex against the connect log line, but the server
  shows "localhost" for loopback test connections — unrelated to this
  session, background task `task_bf7692bd`.
- **Real regressions from the char-creation reorder — FIXED** (test files
  only; the reorder itself is correct, these 9 files just had stale
  step sequences from before it): `smoke_test_quit_creation.py`,
  `smoke_test_quit_menu.py`, `smoke_test_account_delete.py`,
  `smoke_test_accounts.py`, `smoke_test_gametime_persist.py`,
  `smoke_test_parser_display.py`, `smoke_test_timezone.py`,
  `smoke_test_trade_attrs.py`, `smoke_test_look_capitalization.py`. All
  rewritten to the real name→race→class→attrs→done→alignment→welcome
  order and reverified passing standalone.
- **Pre-existing stale test text, unrelated to this session — FIXED**:
  `smoke_test_news.py` checked `help ednews`, but that topic was renamed
  to `edit news` in an earlier session (help_topic.sql's "edit <noun>"
  convention) and the test never followed; `smoke_test_crit.py` checked
  for the string "Decapitated", which has never existed in the codebase
  (`cmd_hurtlimb.c` actually sends "Instant death (major limb
  destroyed)."); `smoke_test_corpse.py`/`smoke_test_zones.py`/
  `smoke_test_mobiles.py` all asserted a corpse's room text using the
  mob's `name` (keyword) field, but `combat_defeat()` correctly builds it
  from the mob's `short_desc` field instead (e.g. actual text is "The
  corpse of a vrock demon lies here.", not "...of vrock demon..." or
  "...of <keyword> lies here."). All fixed to match real, correct
  behavior — verified by manual live repro before touching each test.
- **DB seed-data drift, backfilled — not a code bug**:
  `smoke_test_trigger_seed.py`'s bramble `get`-trigger row (target_vnum
  1000001) was simply missing from the live `trigger` table — `obj` row
  present, `trigger` row absent, meaning `db/sneezy/trigger_seed.sql` (or
  at least its trigger INSERT) was never (re-)applied to this DB after
  being written. Backfilled by re-running the idempotent seed file live.
  **Separately noticed while chasing this**: the same test's "damage 2"
  check is flaky even after the backfill (observed net -1 HP instead of
  -2) — traced to `regen_tick_run()` (regen.c) healing +1 in the gap
  between the trigger firing and the next `score` read. Root cause is
  architectural: `game_loop.c`'s pulse counter advances once per
  `select()` return, and `select()` returns immediately on any ready
  socket — so pulse count (and therefore `REGEN_PULSES`-gated ticks) can
  race far ahead of real wall-clock time under concurrent test-connection
  traffic instead of firing strictly every ~5s. **Pre-existing, NOT a
  regression from this session, not fixed** — flagged here for whoever
  next touches the pulse/regen system; likely wants a real elapsed-time
  check rather than a raw increment-per-loop-iteration counter.
- **Real test bug, unrelated to char-creation — FIXED**:
  `smoke_test_ordinal_target.py`'s three `kill N.dummy` checks used a
  naive `.count(standing_text)` against the room listing, but room
  listings stack identical mobs as one line with an "(xN)" suffix (the
  2026-07-11 mob/object-stacking feature) rather than repeating the line
  N times — so the count was always capped at 1 regardless of how many
  actually died. Added a `count_standing()` helper that parses the
  "(xN)" suffix; verified the underlying kill/ordinal-targeting code was
  always correct via manual live repro before touching the test.

All 23 originally-failing tests now individually reverified passing.
Deliberately did NOT re-run the full `tests/sweep.sh` this session (user:
"dont run full sweep") — that's the next step before pushing `06995c5`.

Last updated: 2026-07-11 — Session 43 continued (home): the first batch
above committed and pushed (clean full sweep). Mobile_Attitude
(alignment stat + mob aggression reaction) implemented locally, not yet
deployed/tested.
- **Alignment stat + mob aggression reaction**: new `progress_t.alignment`
  (-1000 evil .. +1000 good, 0 neutral default, being.h), persisted via a
  new `player_progress.alignment` column. `score` shows it as a word
  (`alignment_word()`, being.c); `set <name> alignment <value>` (58+)
  changes it. `mob_ai_tick()` (mob_ai.c) now reads `ACT_AGGRESSIVE` (bit
  5, value 32): an aggressive mob picks a fight with a non-immortal PC in
  its room unless their alignment is >= 350 (good/saintly). Scoped way
  down from the original's full Mobile_Attitude (emotional attributes,
  hate/fear lists, hunting/pathfinding, faction combat -- see
  sneezymud-master/docs/systems/critical/14-monster-ai-behavior.md) to
  just this one prerequisite-plus-reaction, per TODO.md's own earlier
  scoping note. New `tests/smoke_test_alignment.py`.

Last updated: 2026-07-10 — Session 43 continued (home): a batch of
account/combat/admin features, deployed and verified via standalone smoke
tests (full sweep still pending).
- **New-account login confirmation**: an unrecognized account name now
  asks "New account. Are you sure you want to create the account <name>?
  (y/n)" (new `CONN_CONFIRM_NEW_ACCOUNT` state, descriptor.h/descriptor.c)
  before falling into password creation -- "n" (or anything but y/yes)
  sends the connection back to re-enter the name instead. Ripple: this is
  a new step in front of EVERY new-account flow, so every existing smoke
  test that creates a fresh account needed a `y` answer inserted --
  swept across tests/*.py (delegated to a background agent, ~11 files
  touched). New `tests/smoke_test_account_confirm.py`.
- **Delete entire account (account menu)**: new `X` / `delete account`
  command, mirroring the existing per-character delete flow one level up
  (YES, then re-enter the account password). `account_delete()`
  (account.h/account_repo.c) just deletes the `account` row --
  `player.account_id` already carries an `ON DELETE CASCADE` FK, so every
  character on the account goes with it automatically. New
  `tests/smoke_test_account_delete.py`.
- **Deterministic limb tests**: `combat_debug_set_limb_hp()` (combat.c,
  backs the `hurtlimb` debug command) now also fires the same
  injury-tier `tell()` messages a real `combat_strike()` hit would, not
  just sever/decapitate -- makes it a true stand-in for a real hit.
  `tests/smoke_test_limbs.py`/`smoke_test_limbs_cmd.py` rewritten to set
  a limb's HP directly via `hurtlimb` instead of waiting on combat RNG to
  cross an injury tier within a fixed round budget (the pre-existing
  flake diagnosed earlier this session).
- **Weapon-aware combat messaging + hit/dam bonuses**: `combat_strike()`
  (combat.c) now picks the attacker's wielded weapon (dominant hand
  first) and keyword-buckets its name/short_descr into a verb --
  slice/chop/bludgeon/stab/pierce/lash/hit -- replacing the old
  hardcoded "hit". The `objaffect` table (vnum, type, mod1, mod2) turned
  out to already exist in the live DB with real seeded data; cross-
  checked its `type` column against the bundled original SneezyMUD
  source (`sneezymud-master/code/code/misc/enum.h`'s `applyTypeT`) to
  confirm 15=APPLY_HITROLL, 16=APPLY_DAMROLL, 17=APPLY_HITNDAM. New
  `obj_load_combat_mods()` (obj_repo.h/obj_repo.c) sums those three types
  for a vnum; combat_strike applies the result to hit_roll/dmg for
  whichever weapon is wielded (0/0 for bare hands). New
  `tests/smoke_test_weapon_messaging.py`.
- **`purge` command**: bare `purge` (51+, cmd_purge.c) clears the
  caller's room of mobs and objects (never PCs). `purge linkdead` (58+,
  gated inside cmd_purge() itself) force-removes every linkdead PC in
  the game -- new `world_purge_linkdead()` (world.c) reuses the same
  `g_rooms` walk `world_find_linkdead_pc()` already did for reconnect,
  deliberately not saving first (matches `descriptor_destroy()`'s own
  documented reasoning: an eager save of a linkdead body could clobber a
  fresher DB-side change). New `tests/smoke_test_purge.py`.
- **`transfer` command**: `transfer <name>` pulls an online player into
  the caller's own room; `transfer <name> <vnum>` sends them to a
  specific room instead (cmd_transfer.c). Mirrors the original's `trans`
  (bundled sneezymud-master reference tree) plus the user's own room-vnum
  variant. New `tests/smoke_test_transfer.py`.
- **Deletion logging**: both character and account deletion already
  logged via `log_info()` (file/console only) -- confirmed that's the
  right call per user: "the messages should just go to game log, not
  broadcast" (i.e. NOT `game_log()`, which would also echo live to online
  immortals).
- **Bugs caught while writing/verifying the above**: (1) an off-by-one in
  a hand-copied `INSERT INTO mob` SQL fixture (one extra `0` value vs.
  column count) in two new test files, caught by mariadb's own "column
  count doesn't match value count" error. (2) `attack`/`kill` both route
  to `cmd_kill.c`, which instant-slays for an IMMORTAL caller (bypassing
  `combat_strike()`'s normal multi-round messaging entirely) -- the
  weapon-messaging test's attacker had to be restructured to a mortal
  character to actually exercise the verb/hit-bonus logic.

Last updated: 2026-07-10 — Session 43 continued (home): critical
`.gitignore` bug fixed (was silently excluding src/core/ from git),
half-hour real-time heartbeat tick added.
- **`.gitignore` bug**: a bare `core`/`core.*` pattern (meant for Unix
  core dumps) unintentionally matched the `c_port/src/core/` directory
  by name, silently excluding it from version control. `gametime.c` and
  `zone.c` had never actually reached the git repo despite being pushed
  as part of "everything" earlier this session. Anchored the patterns
  to the repo root (`/core`, `/core.*`) and force-added both orphaned
  files. Swept the rest of the repo for similar false-positive
  exclusions -- found none.
- **Half-hour heartbeat**: new `heartbeat.c`, registered alongside
  `gametime_tick()`. Sends a bare blank line once per real wall-clock
  half-hour (bucket-boundary logic, dedup'd against re-firing every
  pulse). Verified live with a temporarily shortened test interval,
  then reverted before redeploying the real values.
  `tests/smoke_test_heartbeat.py` (a real hourly boundary isn't
  practical to wait for in the sweep, so this only sanity-checks no
  blank-line flooding in a short window).

Last updated: 2026-07-10 — Session 43 continued (home): game clock now
persists across boots.
- **gametime persistence**: `gametime_load()`/`gametime_save()`
  (gametime.c) reuse `multiplay.c`'s exact `game_config` key/value
  pattern -- hour/minute/day/month/year rows, saved on every tick,
  loaded at boot (main.c, right after `multiplay_load()`). Caught a real
  bug while writing it: `db_query()`'s custom format parser only
  accepts `%i` for integers, not `%d` -- would have silently failed
  every save. Verified via a real restart (clock resumed at the
  persisted value, not the 8:00 AM default). `tests/smoke_test_
  gametime_persist.py` (3 checks).

Last updated: 2026-07-10 — Session 43 continued (home): get/drop dispute
logging + a duplicated cap_first() bug caught in two more files.
- **Get/drop logging**: `LOG_SILENT` already existed for file-only,
  never-echoed logging -- cmd_get()/cmd_drop() (cmd_object.c) now call
  it with who/what/vnum/room, reachable via `log search`. Verified a
  same-room immortal sees nothing from it.
- **cap_first() bug, round 2**: the leading-inline-color-tag fix from
  the earlier `look` bugfix only landed in cmd_look.c's copy --
  cmd_scan.c and cmd_object.c each duplicate the same helper function
  independently. Fixed both. `tests/smoke_test_getdrop_log.py` (6
  checks).

Last updated: 2026-07-10 — Session 43 continued (home): `test` command
(58+, shows the currently-running smoke test).
- Existing `@test`/`@test done` loopback hook (descriptor.c) was
  fire-and-forget; added `log_test_set_running()`/`_clear_running()`/
  `_current_name()` (log.h/log.c) so it now persists the name, plus a
  new `test` command that prints it. `tests/smoke_test_test_cmd.py` (3
  checks).

Last updated: 2026-07-10 — Session 43 continued (home): pushed the whole
session to sculpy/NewMUD (commit 90e09b0), then `idea`/`delidea` command.
- **`idea`/`delidea`**: direct mirror of `bug`/`delbug` -- new `idea`
  table, `idea_repo.{h,c}`, `cmd_idea.c`, `LOG_IDEA` log type (added to
  `cmd_setsev.c`'s toggle list too). `tests/smoke_test_idea.py` (9
  checks, all passing).

Last updated: 2026-07-10 — Session 43 continued (home): pre-push sweep
found a real regression + a pre-existing stale test, both fixed.
- **Regression**: the personal time-zone-offset account-creation prompt
  (added earlier this session) broke 3 pre-existing smoke tests that
  scripted a literal `y`/`n` answer to the color prompt and expected to
  land directly on the account menu -- they now land on the new time
  zone prompt first. Fixed `smoke_test_accounts.py`,
  `smoke_test_color_pref.py`, `smoke_test_menu_letters.py` to answer it
  (blank = none) before continuing. Grepped every other smoke test for
  the same pattern -- none left.
- **Pre-existing stale test** (unrelated to this session, predates it):
  `smoke_test_objects.py`'s drop-on-death check still expected the old
  loose-on-the-floor behavior ("A tattered cloak is lying here.") from
  before the corpses-on-death feature landed -- a defeated PC's gear now
  goes into a lootable corpse container instead. Updated to match
  (`The corpse of <name> lies here.` + `get <item> corpse`).
- **Investigated and ruled out** two other sweep failures
  (`smoke_test_limbs.py`, `smoke_test_limbs_cmd.py`, "limb eventually
  shows an injury flag/tier") as unrelated to anything changed this
  session: confirmed via `hurtlimb` that the underlying limb-HP/status-
  tier mechanism works correctly; the tests' own math is just marginal
  -- `LIMB_MIN_MAX_HP` (15, from an earlier session) combined with a
  random limb per hit across 13 limbs makes reliably crossing a status
  tier within the tests' fixed round budget statistically unlikely, not
  a code bug. Not fixed this session (test-design issue, not urgent).
- **`scan` linkdead fix**: no linkdead check existed at all in
  cmd_scan.c's occupant loop (look's room listing shows them tagged,
  combat already excludes them from targeting -- scan just never
  checked either way). Fixed to skip PCs with no live descriptor.
  Extended `tests/smoke_test_scan.py` (6th check).

Last updated: 2026-07-10 — Session 43 continued (home): three `look`
bugs found and fixed together, all against real seeded mob vnum 33271.
- **Capitalization ignored**: `cap_first()` (cmd_look.c) blindly
  uppercased byte 0, a no-op when the label starts with an inline color
  tag (`<o>a dirty refuse hauler<1>`, real seeded content) -- now skips
  leading `<X>` tags first.
- **Wrong name in `look <mob>`**: showed the raw keyword-match list
  ("You look at man dirty refuse hauler.") instead of `short_descr` --
  fixed, uncapitalized since it's mid-sentence.
- **Truncated long description**: `BEING_APPEARANCE_LEN` (256, sized
  for player.appearance's real column) was shared with mob.description
  (mediumtext, real max ~1200 chars) -- bumped to 2048, plus two
  downstream buffers that then tripped `-Wformat-truncation`. New
  `tests/smoke_test_look_capitalization.py` (6 checks).

Last updated: 2026-07-10 — Session 43 continued (home): fixed a real
ANSI bug -- regular-intensity color tags didn't clear a preceding bold.
- **Color engine fix**: `ansi_for_tag()` (colorstring.c) lowercase tags
  now emit `\033[0;NNm` instead of a bare `\033[NNm` -- SGR bold and
  color are independent attributes, and most terminals leave bold stuck
  on until it's explicitly cleared, so `<C>ENTER<c>` (bright, then
  regular) was rendering everything bright. Found while colorizing the
  pager MORE prompt (below) -- the user's own example tag sequence
  exposed it. Fixes every `<x>` tag in the game, not just the pager.
  Updated hardcoded expected byte sequences in six smoke tests to match;
  all pass except `smoke_test_sector_color.py`, which has an unrelated
  pre-existing stale sector expectation (flagged as a separate task, not
  a regression here).

Last updated: 2026-07-10 — Session 43 continued (home): pager held-
messages + colorized MORE prompt.
- **Pager held-messages + colorized MORE prompt**: `descriptor_in_editor()`
  now also covers `page_len > 0`, so mid-pager (e.g. reading `news`)
  behaves like an editor -- messages hold for `catchup` instead of
  interrupting the page. `catchup` widened from immortal-only to
  mortal-level since `news` is mortal-accessible. MORE prompt recolored
  and put on its own line per the user's exact tag example. New
  `tests/smoke_test_pager_held.py` (5 checks).

Last updated: 2026-07-10 — Session 43 continued (home): personal
time-zone offset.
- **Personal time-zone offset**: confirmed the mud clock is fictional
  (no real timezone) and the VM/MariaDB are already America/New_York;
  then ported Sneezy's `CON_TIME`/`time <difference>` sub-feature that
  was explicitly deferred when gametime shipped (below). New
  `CONN_GET_TIMEZONE` account-creation state right after the color
  prompt, asks the offset in hours from the server's Eastern clock
  (range -23..23, blank = 0), persisted to the pre-existing but
  previously-unused `account.time_adjust` column (no migration needed).
  `time` now shows a second real-world-clock line shifted by that
  offset; `time <difference>` re-sets it later. New
  `account_set_timezone()` (account.h/account_repo.c),
  `tests/smoke_test_timezone.py` (9 checks, all passing).

Last updated: 2026-07-10 — Session 43 continued (home): <d> tag, $$g
token, time/day/date system.
- **`<d>` bold tag + `$$g`/`$g` ground token**: both investigated from
  Sneezy and ported (user request). `<d>`/`<D>` is a standalone bold
  toggle (colorstring.c). `$$g`/`$g` substitutes an object description's
  token with the room's ground-surface word -- new `room_ground_type()`
  (room.c) + `obj_apply_ground_token()` (obj.c), wired into both
  `long_descr` display sites in cmd_look.c. No weather-prefix (no weather
  system); confirmed zero real usages in the migrated DB content, so this
  is forward-looking infrastructure, not activating existing text.
- **Time/day/date system**: ported from Sneezy's `GameTime` class -- new
  `gametime.h`/`gametime.c` + `time` command, 28-day months, the same
  weekday formula, noon/midnight/month/year announcements. Ticks on a
  ~60s pulse (15 mud-minutes/tick), session-only (no persistence).
  Dropped weather-driven sun/moon tracking and the personal timezone-
  offset sub-feature. Found `pulse_register()` was silently no-op'ing
  past `MAX_PULSE_PROCESSES` (was 8, exactly filled by this addition) --
  bumped to 16, made overflow log an error instead of vanishing.

Last updated: 2026-07-10 — Session 43 continued (home): zone identity ->
edzone pivot + editors-absolute-quiet bug fix.
- **Zone identity pivot to `edzone`**: the initial one-shot `zoneassign`
  command was replaced (user: "make an edzone command to have a menu
  driven editor function like edroom etc") with a full menu-driven editor
  (`edzone <zone>`), same snapshot-working-copy shape as `edplayer`:
  name/enabled/lifespan/vnum-range are Save/Quit-gated, assigning/
  unassigning a builder (select an already-assigned name to toggle it
  off) applies immediately, an `R`eset-now action force-runs the zone.
  Kept `zone reset <n>` and added `zone list` (paginated, shows every
  zone with its builders) as one-shot shortcuts. Confirmed multiple
  builders can be assigned to the same zone simultaneously (the
  `zone_owner` table's PK was already `(zone_nr, player_id)`, a real
  many-to-many -- verified with a new test case, not a code change).
- **Editors-absolute-quiet bug**: `descriptor_in_editor()` only ever
  recognized the `CONN_REDIT_*` range -- `edplayer`/`edzone` were
  silently never covered by the hold-for-catchup mechanism, despite every
  broadcast call site already calling `descriptor_notify()` correctly.
  Fixed the shared predicate (one place, not per-call-site). Also fixed
  the same root cause in `who`/`promote`/`set`/`copyover`/`users`, all of
  which used `state == CONN_PLAYING` as an "online" proxy and so excluded
  anyone mid-edit (invisible to who, stale live-sync, lost session across
  copyover, mislabeled in users). `smoke_test_held.py` extended to cover
  edplayer/edzone.

Last updated: 2026-07-10 — Session 43 continued (home): Zones Part 2.
- **Zones Part 2 (zone_reset execution)** — user reported empty rooms and
  no mobs; root cause was that Part 1 (Session 38) only migrated the
  35,922-row `zone_reset` table into the DB, nothing ever executed it. New
  `zone.c`/`zone_repo.c` covers M/O/E/G/P/D (~84% of all rows by count);
  the rest (Y/X/Z sets, A random-room, V/H/F/T/L/K/C/R/I/J) are skipped --
  they need subsystems Tobin doesn't have (mob AI, object sets, loot
  tables, traps, grouping/charm/mounts). Full reset runs once at every
  process start (main.c's `zone_boot_all()`) for BOTH a cold boot and a
  copyover-resume -- verified first, before building, that a copyover does
  NOT currently persist room/mob/object state (only player connection
  info survives, cmd_copyover.c), so the two are indistinguishable from
  the world's perspective; then each zone tops up periodically on its own
  `lifespan` via a pulse tick. New `zonereset <zone>` immortal command
  force-runs a zone on demand. Two notable simplifications, both
  documented in TODO.md: E's placement is derived from the object's own
  wear_flag (via the existing wear_slot_for_flag()) rather than the
  original's raw arg3 slot index, which has no Tobin-limb equivalent; and
  there's no world-wide max_exist cap, only a per-room one (arg2). "Mobs
  wandering" still needs a separate movement/AI system -- this only
  populates rooms, doesn't move anything afterward. `smoke_test_zones.py`.

Last updated: 2026-07-09 — Session 43 continued (home): TODO backlog batch
+ corpses on death.
- **Corpses on death** (user: "make it so the corpse of a char loads into
  the room upon death... treated like a container... mobs and players
  alike") — `combat_defeat()` creates an ephemeral "corpse of <name>"
  container object (same `obj_create_ephemeral()` primitive as severed
  limbs) and moves the loser's entire inventory into it instead of
  dropping items loose. Not takeable whole, never closed/locked. Both PCs
  and mobs get one (a mob's is empty for now). `smoke_test_corpse.py`.
- **Backlog batch**: `@set` now dispatches (leading `@` stripped before
  the normal verb parse); immortal-vs-immortal `kill` guard (true-rank
  aware, protects a toggled-mortal immortal too); XP on kill
  (`loser->level * 50`, non-immortal PC winners only, via the existing
  `progress_add_xp()`); positions polish (+15 hit-roll bonus vs. a
  non-standing defender); gender-pronoun sweep of `socials.c`
  (`shake`/`poke`/`comfort` -- only 3 of 16 actually needed it despite the
  "~15" estimate; new `gender_reflexive()` helper); colorized copyover
  messages; `help color`/`help who` enriched with the full tag list and
  `<N>`/`<n>` mention; new `hit` command (thin passthrough to `cmd_attack`,
  lets an immortal have a real fight instead of instakilling).

Last updated: 2026-07-09 — Session 43 (home): crit-hit/decapitation system.
- **Crit-hit + decapitation** (user: "copy sneezys crit hit system, complete
  with object creation upon decapitation") — scoped with the user before
  building (see TODO.md's now-checked-off row for the full breakdown).
  No new RNG layer: triggers purely on a limb's HP crossing to 0% from
  ordinary combat damage (`combat_strike()`, combat.c). Any limb reaching
  0% sheds a lootable ephemeral object ("X's severed <limb>",
  `obj_create_ephemeral()` in obj.c/obj.h -- vnum 0, never persisted, same
  precedent as other session-only state) in the room; the HEAD specifically
  is a decapitation, an instant kill routed through the existing
  `combat_defeat()` "slain" path (not a new death path). PCs only for v1 --
  a mob's destroyed limb does nothing extra.
- **Limb-HP floor bug fix**: found while scoping the above -- a level-1
  character's ~25 max HP splits 13 ways to under 1 HP per limb (rounds to a
  bare 1), so literally any landed hit (minimum damage 1) already destroyed
  whatever limb it hit. Would have made early combat an almost-instant
  coin-flip decapitation. Fixed with a `LIMB_MIN_MAX_HP` floor (15) in
  `being_limbs_full_heal()` -- confirmed with the user first.
- **`hurtlimb <target> <limb> <hp>`** (new immortal-only debug command,
  cmd_hurtlimb.c) -- sets a limb's HP directly and runs the same sever/
  decapitate trigger a real hit would, so the feature (and any future limb
  work) can be tested deterministically instead of waiting on combat RNG to
  land on a specific limb by chance. New `smoke_test_crit.py` (18 checks).

Last updated: 2026-07-09 — Session 42 (home): world death taunt PC-only +
wiznews test hygiene.
- **World death taunt: PC deaths only** (user: "should only fire when a
  player dies, skip the mobs unless the mob is the killer") — `combat_defeat()`
  (combat.c) wraps the `[INFO]` world broadcast in `if (loser_is_pc)`; a mob's
  death is now silent world-wide, while a mob-as-killer still taunts normally
  (the message names the loser, not the winner, so this is unaffected).
  `smoke_test_mobiles.py` section 5 adds a bystander check confirming no
  `[INFO]` fires on a mob death.
- **`smoke_test_wiznews.py` test-hygiene fix**: each run posted a permanent
  "Staff Meeting <suffix>" row via `edwiznews` with no cleanup, same class of
  bug `smoke_test_news.py` already had fixed. After enough sweep runs this
  finally pushed the seeded "Immortal News Arrives" item past `wiznews`'s
  40-row display window, failing "the seeded wiznews item is shown". Fixed
  with the same DELETE-on-completion pattern as news.py; also purged the
  ~30 accumulated junk rows from the VM's `wiznews` table.
- **Flake note**: `smoke_test_kill.py`'s "unsolicited broadcast still leaves
  the bystander at a prompt" check failed once mid-verification, passed
  clean on immediate rerun — a timing flake, not a regression (confirmed the
  combat.c change is a no-op for a PC-loser path). Add to the known-rotating-
  flakes list alongside idle/parser_display/set/mortal_toggle.
- Applied the deferred `news.sql`/`wiznews.sql` content (load/equipment/
  hold-wield-switch and linkdead-persistence changelog entries) to the VM DB.

Last updated: 2026-07-09 — Session 41 (home): linkdead persistence + short_descr
capitalization.
- **Linkdead persistence** (user): losing link no longer destroys a character
  -- `descriptor_destroy()` now detaches (`desc = NULL`), leaving the being in
  its room. `world_find_linkdead_pc(player_id)` (world.c) finds it on
  reconnect; `enter_world()` does a FRESH `player_load()` as always (so any
  DB-side change made while linkdead -- a promotion, an edplayer/set edit --
  still takes effect) but resumes it in the linkdead body's ROOM instead of
  the load room, then discards the old body. Deliberately does NOT eagerly
  persist progress/inventory on detach -- that would clobber a concurrent DB
  write with the pre-disconnect snapshot, breaking the widespread
  create-then-SQL-promote-then-close test pattern; the being stays alive in
  memory so nothing is at risk under normal operation. Only recovers via
  reconnect or process end (copyover only restores descriptor-attached
  beings, so a linkdead body's memory simply ends with the old process).
  Room listing tags them "(linkdead)"; `combat_find_room_target()` (combat.c)
  skips linkdead PCs entirely, so no one can attack/kill them (user: "no one
  can manipulate a linkdead char"). New `smoke_test_linkdead.py`. Fixed 5
  existing tests whose abrupt-`close()`-right-after-creation pattern now
  goes linkdead instead of destroying -- each needed an explicit `quit!`
  first to test what they actually meant to test (objects, mobiles, edplayer,
  set, sector_color -- all relied on a SQL-driven room/level change applying
  to a truly-fresh next login, not a linkdead-room resume).
- **`short_descr` capitalization**: mob/object short_descrs are stored
  lowercase-first by convention ("a city watchman"); a shared per-file
  `cap_first()` helper now capitalizes them ONLY when one starts a whole
  message (room-listing "X is here.", inventory/container-contents bullets,
  scan results, the look-target long_descr fallback) -- mid-sentence uses
  ("You conjure a torch...") stay lowercase, per user spec.

Last updated: 2026-07-09 — Session 40 (home): NewMUD sync + load/equipment/
gender-pronoun follow-ups.
- **Synced the home VM to NewMUD**: dev box (`E:\New MUD`) repointed `origin`
  to `sculpy/NewMUD` (gh https auth, already scoped for private repos) and
  fast-forwarded onto its history (confirmed `c18d592` is a shared ancestor --
  same commit hash, not a re-import). Generated (but did NOT register --
  blocked by the safety classifier, pending user confirmation) a deploy key
  for the home VM per SYNC.md's "first-time setup for a build box"; the VM's
  `~/NewMUD` stayed the existing tar-deployed plain copy (not its own git
  clone) for now. Synced the full tree via tar, clean-rebuilt (zero warnings),
  applied schema (player count + zone_reset row count both held), restarted,
  verified `smoke_test_containers.py` green.
- **Merged `mload`/`oload` into one `load <mob|obj> <vnum|name>`** (user) --
  `cmd_load.c` replaces both; category is abbreviatable (a bare M/O works,
  since both are 1-letter prefixes of "mobile"/"object"). Table-order gotcha
  (same precedent as set/setsev): `load` is itself a prefix of `loadroom`, so
  `loadroom` now needs `loadr`+ (was `loa`+). Old help topics removed (with a
  DELETE, since ON DUPLICATE KEY UPDATE doesn't clean up renamed commands);
  new merged `load` topic. Updated the 3 tests that used mload/oload
  (objects/mobiles/containers).
- **Found + fixed stale legacy-editor-key help text while auditing
  help_topic.sql for the load rename**: `edrules`/`edhelp`/`ednews`/
  `edwiznews`/`edroom` topics still described the pre-Session-32 `.`/`~`/
  `/clear`/`/format` keys instead of `/s`/`/a`/`/b`/`/f`. Also found and
  deleted two fully orphaned topics (`redit`/`hedit`) left over from the
  ed*-rename that were never cleaned up.
- **`wear`/`hold`/`wield`/`switch` split** (user) -- `wear` now only covers
  body-slot equipment; a holdable item refuses `wear` and points to whichever
  of `hold` (non-weapons) / `wield` (weapons, gated on `obj_t.category ==
  OBJ_CAT_WEAPON`) actually applies. New `switch` swaps `held[0]`/`held[1]`
  in place, no unwielding needed. Shared `do_hold_or_wield()` helper in
  cmd_object.c keeps the fill-dominant-hand-first logic from the old `wear`
  HELD branch. Table collisions resolved (documented inline): `switch` needs
  `swi`+ (`sw` is southwest's own alias), `wield` needs `wie`+ (`wiznews`/
  `wiznet`/`wizhelp` already claim `wi`), `hold` only needs `ho`+ (`help`
  claims bare `h`).
- **Equipment display reformatted** (user) -- right-aligned `label: value`
  columns (`EQUIP_LABEL_WIDTH` 14, matching "secondary hold") replacing the
  old `<label> value` bracket form. **Genitalia removed from the listing
  entirely** -- it was never actually wearable (no wear_flag bit ever mapped
  to it), just cosmetically listed; it becomes an object on decapitation
  instead (crit-hit item below). "primary hand"/"off hand" renamed to
  "primary hold"/"secondary hold", now correctly tracking the caller's
  dominant hand (handed_right) rather than a fixed held[0]/held[1] -- a
  latent labeling bug for left-handed characters, fixed in passing.
- **Gender-specific pronouns, started** (user: "make ALL mud output gender
  specific") -- fixed the link-loss line (room echo AND the `[PIO]` log line)
  and `stand`'s room echo, both via `gender_possess()`. Surveyed the rest of
  the codebase: nearly everything else saying "their"/"they" is either a code
  comment or a genuinely-plural referent (bugs, exits, "the gods"), NOT a
  single-character pronoun -- except `src/core/socials.c`, whose ~15 social
  message pairs mostly use a bare "their". Left as a deferred, scoped TODO
  item (a real pass through that one file) rather than rewriting it under
  time pressure in the same session as everything else above.
- Full batch (new commands, table changes, help/news/wiznews entries, and
  the ~9 test files touched) about to be built + swept together -- see the
  sweep result logged right below this entry once it lands.

Last updated: 2026-07-09 — Session 39 (work): repo migration + containers.
- **Infra: home machine reformatted; repo migrated to `sculpy/NewMUD`.** The
  old sync repo was `sculpy/tobin-mud`; a fresh `NewMUD` repo was created and
  the full tobin-mud history merged into it (unrelated-histories merge, so all
  54 commits are preserved). The work box (`db.kullit.com`, user `mud`) was
  repointed from tobin-mud to NewMUD over a **read-only GitHub deploy key**
  (the box previously had no GitHub credentials at all and could not fetch);
  origin is now `git@github.com:sculpy/NewMUD.git`, scoped to that key via the
  repo's `core.sshCommand`. The box was reset to the clean NewMUD tip (its old
  git HEAD was frozen at Session 24, with the real code arriving only by tar --
  backup saved at `~/NewMUD_box_backup_*.tar.gz`). Deploy sweep after migration:
  clean rebuild zero-warning, schema applied (`zone_reset`=35922, `player`
  unchanged at 1597), 58 tests pass + the idle flake green standalone.
- **Containers (`put`/`get <item> <container>`, look-inside, open/close).** The
  obj model already had `OBJ_CAT_CONTAINER` and the `thing_t` chain already
  nests, so this was mostly wiring + rules:
  - `cmd_put` (new, `put <item> <container>`) and `cmd_get`'s new two-arg form
    (`get <item> <container>`) move items in/out of a container carried, worn,
    or on the room floor, via `thing_move_to`.
  - `look <container>` lists contents when open (`cmd_look.c`).
  - `open`/`close` (`cmd_open.c`) now also operate on a container object, using
    the `CONT_*` bits in `val[1]` (added to `obj.h`, verbatim upstream layout:
    CLOSEABLE 1, PICKPROOF 2, CLOSED 4, LOCKED 8). A closed container refuses
    put/get; capacity is enforced by weight (`val[0]`, `obj_contained_weight()`).
  - `smoke_test_containers.py` (19 checks, all green), `news` + `wiznews`, help
    topics (`put`, `containers`; `get`/`open`/`close` refreshed).
  - **Deliberate deviations (see decisions table):** (1) carried-container
    *contents* persistence is deferred -- the flat `player_inventory` table has
    no per-instance parent; to avoid item LOSS, `player_inventory_save` now
    recurses into carried containers and saves contents as loose, so they
    survive a relog but reload un-nested. (2) lock/unlock + keys deferred (pairs
    with the doors/keys TODO); a locked container just can't be opened. (3)
    open/closed state lives on the in-world instance and isn't persisted.
  - **Unblocks Zones Part 2's `P` opcode** (put obj in a container), the reason
    containers were built first (user call).

Last updated: 2026-07-07 — Session 38 (home): Zones part 1 (DB conversion).
- **Zonefiles converted to the DB**: `db/import-zones.py` parses the upstream
  DikuMUD-style `lib/zonefiles/*` into `db/sneezy/zone_reset.sql` -- a new
  `zone_reset` table (zone_nr, cmd_no, command, if_flag, arg1-4, comment).
  35,922 reset commands imported (M 11314 mob->room, O 6625 obj->room, plus
  E/D/G/P and Sneezy-specific opcodes kept for later; `?`-conditional x6750
  and one stray `Wrench` skipped). The `zone` metadata table already existed,
  so only the reset COMMANDS needed importing. Auto-loaded by
  apply-tobin-schema.sh. DATA ONLY -- execution (boot + reset pulse) is Part 2.

Last updated: 2026-07-07 — Session 38 (home), follow-ups:
- **Consistent editor slash-commands**: every ed* editor now shares one set,
  keyed to each action's first letter -- `/s` save, `/a` abort, `/b` blank
  (clear), `/f` format -- via the shared `editor_feed()`. The old `.`/`~`/
  `/clear`/`/format` keys were REMOVED (user follow-up) -- a bare `.`/`~` line
  is now literal text. All editor intro lines + ~7 editor smoke tests updated;
  `smoke_test_editor_format.py` broadened to cover the whole key set.
- **help/wizhelp list buffer** raised (2048->8192, name arrays 256->512) so
  the command list won't truncate as commands grow (already 77).
- **`vnum` paginates** instead of capping at 40: full list built into a 16 KB
  buffer and released a page at a time via the descriptor pager (like `news`),
  with a 500-row safety cap. `smoke_test_vnum.py` drains the pager + asserts it.
- Habit added (user): every code change also gets a plain-English `wiznews`
  entry (immortal dev changelog); recorded in CLAUDE.md + memory.

Last updated: 2026-07-07 — Session 38 (home): `scan` + `vnum`.
- **`scan [dir|name]`** (`cmd_scan.c`) -- faithful port of the original's
  `doScan()`: ray-casts up to 6 rooms deep down each exit, reporting the
  players/mobs out there with a distance word and direction; `scan <dir>`
  scans one direction, `scan <name>` filters by name, and a closed/secret
  door blocks the line of sight. Follows exit chains through unloaded rooms
  via a `roomexit` query; occupants only come from active rooms. Skipped the
  original's move-cost/blindness (Tobin has neither). Player command; news
  entry + help topic + `smoke_test_scan.py`.
- **`vnum <room|obj|mob> <pattern>`** (`cmd_vnum.c`) -- builder tool (51+):
  lists vnums + names of rooms/objects/mobiles whose name contains a
  substring (direct DB_TOBIN query, cmd_mudstats precedent), lowest vnum
  first, capped at 40. Help topic + `smoke_test_vnum.py`.
- Home VM brought current with the work-session Objects/Mobiles work first
  (full c_port sync, clean rebuild, schema reapplied -- player count held at
  4309, confirming the DROP-TABLE fix); baseline sweep 55/2 (parser_display +
  set, the known rotating flakes) before adding scan/vnum.

Last updated: 2026-07-07 — Session 37 (usability fixes, user-reported live).
Two real gaps found while trying the Objects/Mobiles work in-game:
- **`oload`/`mload` now accept a name, not just a vnum**: a purely-numeric
  argument is still treated as a vnum; anything else is a case-insensitive
  SUBSTRING match against `obj.name`/`mob.name` (`obj_find_vnum_by_name()`/
  `mob_find_vnum_by_name()`, new in `obj_repo.c`/`mob_repo.c`), taking the
  lowest-vnum match -- "oload sword" loads the first sword, "mload gua"
  matches "guard" same as "mload guard" (substring, not prefix-only).
- **`look <item>` now actually works**: `cmd_look.c`'s `look_at_target()`
  only ever matched `THING_PC`/`THING_MOB`; widened to fall back to a room-
  floor-then-own-inventory object search, showing the object's `long_descr`
  plus a condition line derived from `cur_struct`/`max_struct` (omitted if
  the prototype never set a max). The "you don't see anyone" message is now
  "you don't see that" (generic across all three target kinds) --
  `smoke_test_gender.py` updated for the new wording.
- Both `tests/smoke_test_objects.py`/`smoke_test_mobiles.py` gained coverage
  (uniquely-named fixtures for the name-lookup checks, since common words
  like "sword"/"vrock"/"demon" already exist in the real seeded content at
  lower vnums and would otherwise win the lowest-vnum tiebreak instead of
  the test's own fixture). Help topics for `oload`/`mload`/`look` refreshed.

Last updated: 2026-07-07 — Session 36 (incident: schema DROP-TABLE bug +
cleanup). While setting Jesus's level for the user, the level reverted --
investigation found `player_attrs.sql`/`player_progress.sql` were raw
mysqldump exports opening with an unconditional `DROP TABLE IF EXISTS` +
`CREATE TABLE` (NOT the safe `CREATE TABLE IF NOT EXISTS` every other schema
file uses). Since `db/apply-tobin-schema.sh` re-runs every file in
`db/sneezy/` on every deploy (its documented purpose: "apply new migrations
to an existing DB"), this silently wiped ALL players' level/xp/hp/attrs on
every run -- confirmed live: 1341 `player` rows, only 3 rows left in each of
`player_progress`/`player_attrs`. Almost certainly not a today-only
incident -- this script has been run across many past sessions.
- **Fixed**: both files rewritten to `CREATE TABLE IF NOT EXISTS` (same safe
  pattern as `player_inventory.sql`/`help_topic.sql`/`news.sql`), deployed
  immediately. Confirmed via `grep -l "DROP TABLE" db/sneezy/*.sql` that no
  other schema file has a live (non-comment) `DROP TABLE` statement.
- **User's call on the data loss**: no recovery needed -- the game isn't
  open/playable yet, so a player-data wipe is acceptable at this stage.
  Not attempting to reconcile against the home VM's separate DB.
- **Standing safeguard added** (user request): `player_repo.c`'s new
  `ensure_jesus_level()`, called from both `player_load()` (real login) and
  `player_load_admin()` (edplayer/set), force-sets the real character named
  Jesus to level 60 and persists it if it's ever anything else -- self-heals
  on every load instead of relying on a one-off manual SQL fix. A no-op once
  already correct.
- **Also fixed this session**: smoke tests were only announcing themselves
  at startup (`announce()`, Session 32), never at finish, and the
  "standing habit" of copying `announce()` into new tests had in practice
  only ever landed in the one file that introduced it
  (`smoke_test_logging.py`) -- every other test, including this session's
  new `smoke_test_objects.py`/`smoke_test_mobiles.py`, was silently missing
  it. Retrofitted all 56 `tests/smoke_test_*.py` files with a self-contained
  `announce()`/`announce_done()` pair (doesn't depend on the file's own
  `recv_all`/`send_line`, so insertion position doesn't matter) via a
  one-off script, verified every file still parses, spot-checked a few by
  hand, then deleted the script. `descriptor.c`'s `@test` hook now
  recognizes `@test done <name>` and logs `[TEST] finished %s` (distinct
  from `running %s`) -- `smoke_test_logging.py` gained a check for this.

Last updated: 2026-07-07 — Session 35 (Mobiles, Phase 2D). User asked to
"do the same for mobiles" as the just-finished Objects pass -- same
scope-limiting decisions (full runtime system now, `edmobile` editor
deferred), no new questions to re-litigate. Two research passes over
`sneezymud-master/code/code/` (`misc/monster.h`'s class hierarchy,
`sys/db.cc`'s mob HP generation, `misc/defs.h`'s `ACT_AGGRESSIVE`) plus
Tobin's own `combat.c`/`regen.c`/`cmd_look.c` confirmed the design needed
NO new struct -- `being_t` already anticipated this (its `desc` field was
already commented "NULL for mobs"; `combat.c`'s `tell()` helper already
said "no-op for a mob (once mobs exist)").
- **A mob is just a `being_t` with `kind = THING_MOB`**, `player_id`/
  `account_id = 0`, `desc` always NULL -- confirmed faithful to the
  original, where `TMonster` inherits `TBeing` directly and shares its
  entire combat/HP/limb machinery (not a meaningfully different data
  model). No `mob_t` struct, unlike Objects (which genuinely needed one).
- **`being_create_mob(vnum)`** (`being.c`, mirrors `being_create_pc()`)
  loads a prototype via new `mob_repo.c`'s `mob_proto_load()` -- reads the
  upstream-seeded `mob` table directly (same "prototypes already exist"
  precedent as `obj_repo.c`), using only `name`/`short_desc`/`description`/
  `level`/`hpbonus`/`sex`. **Deliberate simplification**: the mob table's
  real 12-stat columns (str/bra/con/dex/agi/intel/wis/foc/per/cha/kar/spe)
  are a completely different, wider scale than Tobin's `ATTR_BASE=120 ± 30`
  6-stat system (real seed values run from -25 to 350+) -- copying them
  verbatim would make `combat_strike()` wildly unbalanced. Instead, a
  mob's `attrs_t` is derived from its `level` (`ATTR_BASE + level`, capped
  at `ATTR_MAX`), and `max_hp` from a placeholder formula using `hpbonus`
  (the original's real per-mob HP-scaling parameter, confusingly named --
  same "placeholder, revisit later" precedent as `being_calc_max_hp()`/
  the XP curve/regen rate). `class`/`race`/`letter`/`attacks`/`tohit`/`ac`/
  `damage_level`/`damage_precision`/`gold`/`faction`/`actions`/`affects`/
  `spec_proc`/`skin`/`vision`/`can_be_seen`/`max_exist`/sounds/`pos`/
  `def_position` are all unused, explicitly deferred to a future
  `edmobile`/AI session.
- **`mload <vnum>`** (`cmd_mload.c`, immortal, `BUILD_MIN_LEVEL`, direct
  mirror of `oload`): manual only, no zone-reset system yet (2E) to
  respawn one automatically -- an `mload`ed mob doesn't survive a restart,
  same documented gap as room-floor objects.
- **Combat widened, not rebuilt**: `combat_find_room_target()` now
  matches `THING_MOB` alongside `THING_PC`, and switched from whole-string
  `strncasecmp` to a new shared `thing_name_matches()` (`thing.c`,
  per-keyword prefix matching -- a mob's `name` can be multi-word like
  "vrock demon", same issue Objects already solved for get/drop; a
  single-word PC name behaves identically to before). `combat_process_run()`
  needed ZERO changes -- it already iterates by descriptor and calls
  `combat_strike()` on whichever two `being_t*` are fighting, so a
  PC-vs-mob round already resolved correctly once targeting could find a
  mob at all. `combat_defeat()` now branches on `loser->base.kind`: the
  HP-half-patch/limb-heal/`player_progress_save()`/eject-to-menu path is
  PC-only (a mob has no player_id row, no menu); a defeated mob is instead
  `being_destroy()`ed outright -- permanent, no respawn. **This also fixed
  a latent dormant bug**: before mobs existed, defeating any `desc == NULL`
  being would have silently left it sitting in the room forever, since
  only the PC branch ever removed anything.
- **`look <mob>`**: `cmd_look.c`'s `look_at_target()` widened the same way
  (kind filter + `thing_name_matches()`) -- already generic over
  `appearance`-or-"nothing special", and a mob's `description` column is
  loaded into that same field by `being_create_mob()`, so no mob-specific
  branch was needed. (Does NOT touch the separately-logged `look <object>`
  TODO gap -- different kind, different fix, left alone.)
- `tests/smoke_test_mobiles.py` (SQL-bootstrapped sandbox room + mob
  prototype at a high vnum, full positional/named column list since the
  `mob` table's columns are NOT NULL with no defaults, unlike `obj`):
  `mload` gate, room-generic listing, `look <mob>` by either of its two
  keywords, a mortal `kill`/`attack`ing the mob by an abbreviated keyword
  with real multi-round combat, and permanent removal on defeat. Help
  topic for `mload` + refreshed `attack`/`kill`/`look` topics to mention
  mobiles; a news entry.

Last updated: 2026-07-07 — Session 34 (Objects, Phase 2C). User asked for
`edobject`/`edmobile` next; investigating first found there was no object
system at all (`THING_MOB` was an enum label only, no `obj_t`/repo/commands).
Asked two blocking questions: user chose the FULL object system now (not
just a DB-prototype editor), and to draft the wireframe myself when the
editor session comes -- `edobject` itself stays explicitly deferred to that
future session. Built per TODO.md's own Objects (2C) checklist:
- **`obj_t`** (`include/obj.h`/`src/core/obj.c`): collapses the original's 60
  `itemTypeT` values (verified by reading `misc/obj.h` directly) into 16
  `obj_category_t` buckets via a single lookup table (`category_for_item_type()`),
  same precedent as `sector_color()`'s keyword bucketing. **Key finding**:
  object PROTOTYPES already exist -- the upstream-seeded `obj` table
  (`db/sneezy/obj.sql`, thousands of real rows, PK vnum) is read directly
  by `obj_repo.c`'s `obj_proto_load()`; no new prototype table needed. Only
  in-world INSTANCES are new state.
- **Containment**: every obj_t (room floor, carried, worn, or held) lives in
  the ONE existing `thing_t` chain (`stuff_head`/`stuff_next`/`parent`, via
  `thing_move_to()`/`thing_remove_from_parent()` -- both already existed,
  unused by any kind until now, clearly anticipating this). `being_t.
  equipment[LIMB_COUNT]`/`held[2]` are fast-lookup pointers INTO that same
  set, not separate storage -- `inventory` walks `stuff_head` excluding
  anything also pointed to by those arrays; `equipment` reads the arrays
  directly. `THING_OBJ` added to `thing_kind_t`.
- **Wear-flag -> limb mapping** (`wear_slot_for_flag()`): `obj_t.wear_flag`
  is stored VERBATIM in the original's upstream bit layout (TAKE=1,
  FINGERS=2, NECK=4, BODY=8, HEAD=16, LEGS=32, FEET=64, HANDS=128, ARMS=256,
  BACK=1024, WAIST=2048, WRISTS=4096, HOLD=16384, THROW=32768) so every
  already-seeded object "just works" with zero data migration; translated to
  a Tobin `limb_t` only at wear time. HEAD/NECK/BODY/WAIST map 1:1; FINGERS/
  ARMS/LEGS/FEET pick the first empty of the L/R pair (prefer right); HOLD
  goes to `held[]` (respecting `handed_right`). **HANDS/WRISTS/BACK/THROW
  have no Tobin limb equivalent** (the 13-limb set was already deliberately
  trimmed vs. the original's real slot list, see the Limbs decision row) --
  such an item is carriable but not wearable in this port; a documented
  content gap, not a bug. No second slot enum -- reuses `limb_t` directly,
  per TODO.md's explicit constraint.
- **Commands** (mortal, `src/cmd/cmd_object.c`): `get`/`drop`/`inventory`/
  `wear`/`remove`/`equipment`. Multi-keyword object names ("bag large real")
  match if the typed word is a case-insensitive prefix of ANY keyword, not
  just the whole string (object names, unlike player names, are DB keyword
  lists). Abbreviation collisions documented inline in cmd_table.c: bare "i"
  already reaches `immort` (needs "in"/"inv"), "we" already reaches `west`
  (needs "wea"), "re" already reaches `rest` (needs "rem"), "d"/"do" already
  reach `down` (needs "dr"); `equipment`'s "eq" and `get`'s "g" (mortals
  only -- immortals still get `goto` for "g", since `get` is placed after it
  in table order) don't collide with anything.
- **`oload <vnum>`** (immortal, `src/cmd/cmd_oload.c`, `BUILD_MIN_LEVEL`
  same tier as `edroom`/`goto`): manual builder tool, spawns a prototype
  into the caller's room. No zone-reset system yet (still-future 2E) --
  a room-floor object placed this way does NOT survive a server restart.
- **`cmd_look.c`**: room-floor objects now print their prototype's
  `long_desc` verbatim (e.g. "A hairball is laying here.") in the same loop
  that already listed other occupants, instead of the generic "<label> is
  here." used for PCs/mobs.
- **Persistence** (`db/sneezy/player_inventory.sql`, new Tobin table,
  `CREATE TABLE IF NOT EXISTS` -- NOT the mysqldump-style unconditional
  DROP+CREATE some earlier tables use, since this one WILL be live data
  players don't want wiped on a schema reapply): only player-carried/worn/
  held instances persist (`player_inventory_load`/`_save`, wired into
  `player_repo.c`'s `player_load()` and called immediately after every
  inventory-mutating command, same "save at the point of mutation"
  precedent as `set`/`cmd_mortal.c`'s progress saves -- NOT a generic
  save-at-quit). **Deliberately NOT wired into `player_load_admin()`**
  (edplayer/set's snapshot-copy-then-destroy pattern would leave a dangling
  pointer once `being_destroy()` frees a populated inventory -- documented
  inline in `player_repo.c`). Room-floor objects are NOT persisted (no
  zone-reset system to repopulate them at boot yet) -- documented gap.
- **Drop-on-death** (`combat.c`'s `combat_defeat()`): resolves the "Future
  direction" open question already on record -- a defeated character's
  carried, worn, and held items all fall into the room they died in (same
  safe-unlink-while-iterating pattern as `being_destroy()`), not lost or
  carried to the account menu.
- `tests/smoke_test_objects.py` (SQL-bootstrapped sandbox room + 5 object
  prototypes, same discipline as `smoke_test_doors.py`): oload gate, look
  listing, get + no-TAKE refusal, inventory, wear + already-occupied
  refusal, equipment display (worn + held), remove, drop, reconnect
  persistence (both carried and held survive), and combat-defeat drop
  (verified via a bystander's `look` after the kill, and the victim's own
  empty inventory/equipment on their next reconnect). Help topics for all
  seven new commands (`help_topic.sql`) + a `news.sql` entry.
- Explicitly deferred (not silently dropped): `edobject` editor itself;
  containers holding sub-items (`put`/`get from container` -- containers
  exist as objects and can be carried/worn, just can't hold anything yet);
  carry-weight/volume limits; zone resets (2E); keys actually unlocking
  `EXIT_COND_LOCKED` doors (a natural next step now that `KEY`/`CONTAINER`
  categories exist, but a separate follow-up); shops/money economy;
  `examine`/extra-descriptions.

Last updated: 2026-07-07 — Session 33 (work-box redeploy + reconcile verification).
Adopted the fully-reconciled `origin/main` (through Session 32, commit `73977f0`)
locally (`git reset --hard`), then found db.kullit.com's own checkout was still
stuck at Session 24 with a pile of uncommitted local diffs and no working git
credentials (`fatal: could not read Username for 'https://github.com'`) --
resolved with a `git archive main | ssh ... tar xf -` overwrite (user-approved,
same method as prior manual deploys), a clean `rm -rf build` rebuild (zero
warnings), migrations applied, server restarted. Full sweep: 53/55, the 2
failures (`smoke_test_notify.py`, `smoke_test_set.py`) both re-ran clean
standalone -- confirmed transient (the same "rotating flake" pair the handoff
already flagged), not a regression. Reconciliation + redeploy is COMPLETE.

Last updated: 2026-07-06 — Session 32 (reconcile follow-ups). In order:
- Reconciled the second machine's `work-2026-07-06` branch onto `main`:
  cherry-picked Sessions 26-31, backported the Session 25 delete-time password
  reconfirmation standalone (`CONN_CHAR_DELETE_PASSWORD`), narratives merged.
- **`LOG_TEST` + `@test` announce hook**: every smoke test now announces
  itself to the running MUD. A loopback-only `@test <name>` line in
  `handle_line()` emits `game_log(LOG_TEST, "running %s", ...)` at any
  connection state (so it works before login); `LOG_TEST` is on by default and
  toggleable per-immortal via `setsev test` (some immortals don't need test
  announcements). Tests inline an `announce()`
  helper and call it at startup (canonical copy + regression check in the new
  `smoke_test_logging.py`). Standing habit going forward.
- **Connect is now a typed `[PIO]` log**: `enter_world()` logs
  "<name> has connected. [<ip>]" via `game_log(LOG_PIO, ...)` (was a file-only
  `log_info`), symmetric to the link-loss line and carrying the IP.
- Note: the delete-password backport commit (188b955) was code-correct but its
  first VM "verification" accidentally ran stale files (a `git archive HEAD`
  deploy of then-uncommitted changes); re-deployed from the working tree and
  properly validated here -- delete tests exercise the real reconfirm flow.
- Gotcha fixed (user caught "prompt not firing"): the `CONN_CHAR_DELETE_PASSWORD`
  enum insert shifted `CONN_PLAYING`'s value, but the `tar` deploy kept old file
  mtimes so `cmake` skipped recompiling `game_loop.c` -- it compared the stale
  `CONN_PLAYING` and the game-loop prompt stopped firing everywhere. A clean
  rebuild (`rm -rf build && cmake ...`) synchronized all units. Standing rule
  saved to memory: clean-rebuild after any header change on deploy.

Last updated: 2026-07-06 — Sessions 26-31. This session, in order:
- **`setsev`** (port of `misc/immortal.cc`'s `doSetsev()`): per-immortal
  opt-out from `game_log()`'s `[TAG]` echoes. New `being_t.severity` bitmask
  (default: every type on); bare `setsev` lists game/pio/combat/bug/db/edit
  with on/off state, `setsev <type>` (abbrev ok) flips one. The personalized
  `jesus` type is hidden from and unsettable by anyone but the immortal
  actually named Jesus. Deliberately session-only, not persisted (no
  migration for a niche admin preference -- see `being.h`'s field comment).
  `smoke_test_setsev.py` (includes an end-to-end check that toggling `bug`
  off actually stops that immortal's `[BUG]` echoes via a real `bug` post,
  while a second immortal still sees them) + a help topic. Deployed to
  db.kullit.com under the `mud` account, rebuilt clean, full sweep green.
- **Sector-tinted `look`**: new `sector_color()` (`room.c`) buckets each of
  the 61 sector types by keyword into a base color letter (lava/fire->red,
  city/road/building/mountain/cave/solid rock->white, ocean/river/beach->
  blue, arctic/atmosphere->cyan, desert->yellow, swamp/forest/jungle/
  grassland/plains/hills->green, astral->purple, else->white). Deliberately
  never returns black/gray (`<k>`/`<K>`, unreadable on a dark terminal --
  an earlier pass used it for the mountain/cave group; fixed same-session
  per user feedback). `cmd_look.c` now wraps the room NAME in the bright
  (uppercase) tag and the DESCRIPTION in the dim (lowercase) one, in both
  the mortal and immortal-builder-header display paths.
  `smoke_test_sector_color.py` (raw-byte ANSI checks, both paths) + help
  topic update. Rebuilt clean, deployed, verified live.
- **Editor `/format`**: new `editor_format()` (descriptor.c), wired into
  the shared `editor_feed()` alongside `.`/`~`/`/clear` -- reflows the
  in-progress buffer to `EDITOR_FORMAT_WIDTH` (78) columns, re-joining and
  re-breaking words while keeping blank-line paragraph breaks. Because
  every `ed*` editor (edroom's description, edhelp, ednews, edwiznews,
  edrules) already routes through this one shared function, all five get
  `/format` for free from a single change. `smoke_test_editor_format.py`
  (via edhelp: confirms a long unwrapped line is untouched until `/format`
  is invoked, the reflowed buffer has no over-width lines, paragraph
  breaks and every word survive, and the reformatted text -- not the
  original -- is what `.` actually saves). All 5 editor intro messages
  and their help topics updated to mention it.
- **`edplayer`** (menu-driven player editor, 58+, matching `promote`'s
  tier): built instead of a separate one-shot `set`/`@set` command, per
  user direction -- the original's 1279-line `@set` covers classes/
  factions/objects/mobs/rooms Tobin doesn't have, and the realistic
  subset (level/attrs/hp/gender/title/location) already overlapped with
  this already-planned editor. Covers level, experience, HP/max HP,
  attributes, gender, title, load room, and handedness -- everything a
  player record currently persists. Works on any player by exact name,
  online or offline (`player_load_admin()`, admin-wide like
  `player_set_level_by_name()`). **Unlike `edroom`, the working copy is a
  DB snapshot, not a live pointer** -- players aren't kept resident in
  memory the way rooms are, so there's no `world_get_room()`-equivalent to
  point at. (S)ave writes the snapshot back to `player_progress`/
  `player_attrs`/`player` and, if that player happens to be connected and
  playing right now, also syncs their live `being_t` directly (same
  online-target courtesy `promote` already gives) -- so an online edit
  takes effect with no relog. New `player_load_admin()`,
  `player_set_gender_by_name()`, `player_set_handed_by_name()`,
  `player_set_appearance_by_name()` in `player_repo.c` (the title/load-room
  setters were already account-scoped, reused with the looked-up
  account_id). `smoke_test_edplayer.py` (gate at 58, nonexistent-name
  rejection, every field settable and reflected in the menu, save
  persists via a fresh reconnect, the live-sync case against an
  already-connected session, and Quit's (D)iscard truly discarding an
  unsaved change) + help topic.
- **`set`** (one-shot sibling of `edplayer`): `set <name> <field> <value>`
  (`cmd_set.c`, 58+, same as `edplayer`) -- the user confirmed both a menu
  editor AND a scriptable one-liner were wanted for `set`/`@set`, not one
  instead of the other. Same field list as `edplayer` (level/xp/hp/
  attributes/gender/title/loadroom/handed), same admin-wide-by-exact-name
  reach and online-target live sync. Table-order gotcha: `set` (the exact
  3-letter name) had to be placed BEFORE `setsev` in `cmd_table.c` --
  otherwise a level-58 caller typing "set" would match "setsev" instead
  (both start with "set", first table match wins) -- documented inline.
  Refactored the attribute-name lookup (`attr_field`, previously `static`
  in `descriptor.c`, used only by character creation) into a public
  `attrs_field()` in `being.c`/`being.h` so character creation, `edplayer`,
  and `set` share one copy instead of three. `smoke_test_set.py` (gate --
  including the documented prefix-collision quirk when "set" itself is
  invisible at the caller's level, so the check verifies nothing actually
  changed rather than asserting a specific error string -- validation,
  every field, persistence via reconnect, online live-sync) + help topic.
- **Door mechanics**: the door type/condition data every exit already
  carried (set via `edroom`) finally does something. New `open`/`close
  <direction>` (`cmd_open.c`) toggle the exit's `EXIT_COND_CLOSED` bit
  (new named bit constants in `room.h`, alongside `_LOCKED`/`_SECRET`);
  `cmd_move.c` blocks movement through a closed door ("The door is
  closed."); `cmd_look.c`/`cmd_exits.c` hide any exit with the Secret bit
  set from their listings (still walkable if you know the direction).
  `open` refuses a Locked door -- there's no way to lock/unlock one yet
  (that needs a key, which needs the object system; deferred). **Door
  state is per-exit, not mirrored to the reverse exit** -- confirmed by
  checking `edroom`'s own exit auto-fix logic first: a newly auto-created
  reverse exit already gets its own independent, doorless condition
  bitmask rather than copying the forward exit's, so per-direction
  independence is the existing data-model assumption, not a gap this
  session introduced. `smoke_test_doors.py` (SQL-bootstrapped sandbox
  rooms, same discipline as `smoke_test_redit.py`) covers secret-exit
  hiding, movement blocking, open/close confirmation and error cases
  (no exit, no door, already open/closed), DB persistence, and that a
  secret exit is still walkable. New `open`/`close` help topics; `exits`
  updated to mention secret-hiding. **Real interaction found by the first
  post-feature sweep** (not a flake): `smoke_test_redit.py` sets an
  exit's condition to Closed while testing the Conditions toggle, then
  walks through it -- previously a no-op cosmetic flag, now a real block.
  Fixed the test to `open` the door first, matching what a real player
  would now have to do; this is the feature working as intended, not a
  bug.
- **Flake note**: `smoke_test_parser_display.py` failed once mid-sweep
  (auto-look prompt check) then passed cleanly on two immediate reruns and
  a full clean sweep (51 passed, 0 failed) -- transient, not a regression
  from any of this session's changes; not touching that test.
- **Reconcile**: Sessions 26-31 (above) were built on a second machine
  (`work-2026-07-06` branch) with no common git ancestor to this box's
  history; they were cherry-picked onto `main` on top of Session 24 and the
  STATUS/TODO/help narratives combined. The delete-time password
  reconfirmation from that branch's Session 25 was also backported
  standalone (`CONN_CHAR_DELETE_PASSWORD`); the rest of Session 25 was a
  full-tree sync already covered by this box's own 22-24 lineage.

Last updated: 2026-07-06 — Session 24. This session, in order:
- deployed + verified `bug`/`delbug`, `newbie` channel (PLR_NEWBIE pflag),
  `rules`/`edrules` (59+, `EDIT_RULES` line editor) — all green on the home VM
- fixed a real `smoke_test_kill.py` flake: the bystander's post-broadcast read
  now waits for THIS fight's death taunt + a trailing prompt (was a fixed 1s
  window that lost the prompt under load / got fooled by concurrent taunts) —
  8/8 sequential, full sweep clean
- wrote `ENVIRONMENT.md` (repo root): execution-ordered home⇄work relocation +
  from-scratch work-server (`db.kullit.com`) setup incl. creating the `mud` user
- Committed the session's backlog (bug/newbie/rules) + the above.

Last updated: 2026-07-05 — Session 23. This session, in order:
- color tiers, `goto <char>`, `help edit`, multiplay control, `point` social
- player `title` (+ `<N>` name substitution) & `who` filter args
- who/score color the rank BRACKET, not the name
- gender + pronouns + appearance at creation (`look <player>`, score)
- color preference asked at account creation, persisted (backward-compatible
  color prompt via re-dispatch)
- idle: mortal idle-timeout (30m) w/ immortal immunity + tighter 12s keepalive
- colorized help format: magenta `<m>` body + cyan `Syntax:`/`Minimum Level:`
  (Syntax parsed from the body's Usage line); added `<m>`/`<M>` magenta alias
- social abbreviation (`poi`->`point`; socials now prefix-match like commands)
- `flee` (combat escape), `toggle` (unified player/game switches),
  `exec` (lvl-60 host shell: blocklist + `timeout` + audit log)
- typed logs: `log_type_t` + `game_log()`; link-loss now `[PIO]` (was `[LOG]`)
Habit going forward: tastefully colorize player/immort output with LOWERCASE
color codes (see memory: tobin-colorize-habit).

## Quick orientation

- Original C++ source: `../sneezymud-master/code/code/` (sys, misc, obj, disc, spec, cmd, game, task) — a fresh upstream clone, the reference to verify against (Session 22 consolidated the old scattered `code/`/`lib/`/`db/` copies into this one tree)
- This port: `c_port/`
- Build: `cd c_port && cmake -B build && cmake --build build` (Linux/Fedora — see README.md)
- Run: `./build/tobin_c`, requires MariaDB reachable — see README.md for env vars
- DB schema: upstream world seed under `../sneezymud-master/db/sneezy/` (unchanged); Tobin's own tables/migrations under `c_port/db/` (see `c_port/db/README.md`). Seed = `sneezymud-master/db/init-db.sh` then `c_port/db/apply-tobin-schema.sh`.
- **Two dev/test environments as of Session 20** — which one to use depends on where
  the user says they are ("at work" vs "at home"):
  - **Work**: db.kullit.com (10.0.0.12), Fedora Linux 44, `root` login (a matching
    `mud` user is planned), key-based SSH already set up (same box `../talker.c` runs
    on). Copy of the tree at `~/NewMUD/`.
  - **Home**: VirtualBox VM "NUDServer" on the user's Windows machine, Fedora 44,
    bridged virtio NIC, static IP 192.168.254.200, user `mud`, key-based SSH.
    Copy of the tree at `~/NewMUD/`. 12 GB RAM / 4 CPUs. Server on port 4000,
    log at `~/NewMUD/tobin_c.log`.
  - **Sync is now via git** (Session 20): private GitHub repo
    `github.com/sculpy/tobin-mud`, repo root at the top of the whole tree (one level
    above `c_port/`). Commit+push when leaving a location, pull on arrival. The two
    Linux boxes' `~/NewMUD` copies predate the repo and are still plain copies —
    convert them to clones when convenient; until then sync them with `scp` (and run
    `sed -i 's/\r\$//'` on `.sh` files copied from Windows).
  - Server is a plain foreground process; run detached with
    `setsid nohup ./build/tobin_c > /root/NewMUD/tobin_c.log 2>&1 < /dev/null &`.
  - **Gotcha**: `pkill -f build/tobin_c` will also kill whatever *other* shell command
    happens to contain that substring in its own command line (e.g. a `stat build/tobin_c`
    in the same script!), including the SSH session running it. Use `pkill -x tobin_c`
    (exact process-name match) instead.
  - `tests/smoke_test.py` / `tests/smoke_test_login.py` / `tests/smoke_test_accounts.py` are
    scripted raw-socket sessions (not real `telnet`) that exercise the full account/character
    menu → point-buy attrs → look/who/score flow; run with e.g.
    `python3 tests/smoke_test.py [host] [port] [name]` once the server's up. Use a fresh
    name/account each run (or pass one as an arg) -- these aren't idempotent against
    leftover data from a previous run, since account/character names are unique in the DB.
- Full plan this session worked from: was written to the Claude Code plan
  file for this conversation (`mutable-frolicking-gosling.md`) — not
  copied into this repo. If you need the original design rationale beyond
  what's summarized below, ask for it to be re-derived; the key decisions
  are captured in the table below regardless.

## Architecture decisions (locked — do not re-litigate without discussion)

| Decision | Choice | Notes |
|---|---|---|
| Inheritance replacement | first-member struct embedding + `kind` tag enum | `thing_t base;` as literal first member of `being_t`/`room_t`/(future) `obj_t`. Mirrors `TThing::getKind()`, which the original already does by hand. |
| Virtual dispatch replacement | plain C functions by default; small per-domain dispatch tables (`const T_vtable_t TABLE[KIND]`) only where genuinely varying, e.g. future `obj_vtables.c` | Most of `TThing`'s 199 / `TBeing`'s 148 virtuals are never overridden by more than 1-2 kinds (confirmed by reading `thing.h`/`being.h`) — not real polymorphism, don't build tables for it. |
| `obj/` 98 classes → | plan: collapse to ~15 generic categories, tagged union in `obj_t.data`, populated straight from the DB's `val0..val3` generic payload | **Not started yet** — Phase 1 has no `obj_t` at all. |
| `disc/` 69 classes → | plan: trim to a core ~8-10 disciplines | **Not started.** Proposed defaults: `basic_combat`, `basic_adventuring`, `advanced_defense` + 2-3 flavor (one caster, one melee-specialist) — not finalized, pick when `disc/` work starts. |
| `task/` (crafting) | plan: trim to 1-2 professions | **Not started**, professions not yet chosen. |
| `game/` (card minigames) | **cut entirely**, user-approved | No port needed, ever. |
| Data tables (obj categories, disciplines) | compiled `.c` designated initializers, NOT JSON/text config | Structural/recompile-worthy, small (~15-25 entries total). Bulk content (rooms/objs/mobs) stays in MariaDB unchanged. |
| DB access | plain `mariadb/mysql.h` C API via `db.h`/`db.c`, no ORM/wrapper | 1:1 port of `sys/database.cc`'s `TDatabase` (which was already just RAII+vtable ceremony over the same C API). Same `%s`/`%r`/`%i`/`%f` query mini-language. |
| Password hashing | `crypt()` via `<crypt.h>`, **random SHA-512 salt** (deviation from original) | Original salts new accounts with the account NAME (`crypt(arg, account->name)`) — weak/guessable. This port generates a random `$6$...` salt instead. Self-contained hardening, not a redesign. |
| Networking | raw POSIX sockets + `select()`, non-blocking fds | Direct port of `TMainSocket`/`TSocket`, which were already raw fds under the hood. |
| Telnet handling | server sends `IAC WILL ECHO / WILL SGA / DO SGA` on connect; line buffering + IAC/backspace handling done manually in `descriptor.c` | Known gap: an `IAC SB ... SE` subnegotiation split exactly across two TCP reads can be mis-parsed (see Open Questions). Unlikely with a plain `telnet` client since we never request subnegotiation-based options. |
| Outbound line endings | `descriptor_send()` (the single choke-point for all outbound writes) normalizes every bare `\n` (not preceded by `\r`) to `\r\n` before writing to the socket -- see Session 9. | Room descriptions are DB-sourced from the original SneezyMUD dump and use Unix-style `\n` line endings internally; a bare `\n` doesn't reset a real telnet client's cursor to column 0, producing a "staircase" effect. Text we compose ourselves already uses `\r\n`, so this is a no-op for those. |
| Command parsing | `cmd_dispatch()` (`cmd_table.c`) matches the typed verb against each registered command's name by PREFIX, not exact string -- "sc"/"sco"/"score" all reach `cmd_score`, same idea as classic DikuMUD abbreviation matching. First match in table order wins; keep command names prefix-distinct. | Added Session 9, replacing the old approach of hand-listing every alias (`"l"`, `"sc"`, ...) as separate table rows. |
| `quit!` exclusion | `quit` is deliberately NOT in the `cmd_table.c` COMMANDS array, so it never participates in abbreviation matching. `cmd_dispatch()` special-cases the exact, full literal `"quit!"` before the abbreviation loop even runs. The account-menu and character-creation `quit!` checks (handled directly in `descriptor.c`, not through `cmd_dispatch`) were changed to require the same exact `"quit!"` literal, for consistency. | Added Session 9 (user requirement): a mistyped or abbreviated command (e.g. "qu", "q", or even the bare word "quit") must never accidentally leave a character or disconnect. |
| Target platform | Linux/Fedora (assumed, per established pattern from `../talker.c`) | Not natively buildable on Windows as-is (glibc/libxcrypt `crypt()`, POSIX sockets). Flag if this assumption is wrong. |
| Boost removal | `program_options` → env vars (`config.c`); `filesystem`/`system` → not needed yet; `regex` → not needed yet (no callers ported); `shared_ptr` → N/A, `Comm` hierarchy not ported yet | Revisit `regex` replacement (POSIX `<regex.h>` vs. vendoring PCRE) once a caller (e.g. spell_parser) is actually ported. |
| Project name | **"Tobin"** — this C port's own branding, code identifiers, and env-var names (`TOBIN_DB_*`, `TOBIN_PORT`, header guards, `tobin_c` binary/CMake target, `DB_TOBIN` enum) | Scope was deliberately `c_port/` only — the untouched original engine (`code/`, `lib/`, 493 files) keeps the real "SneezyMUD" name, since it's a separate open-source project we're porting *from*, not renaming. The literal MariaDB database name (`sneezy`) and `db/sneezy/*.sql` paths are unchanged too, since `db/` is out of scope — renaming those strings would silently break connectivity to the real, still-`sneezy`-named database. Attribution (the "(SneezyMUD, a DikuMUD-derived...)" description of the upstream project, in README.md) was deliberately left alone. |
| Account/character model | One account owns many characters (`player.account_id`, already in the original schema); every player lookup (`player_load`, `player_load_room`, `player_delete`, `player_list_by_account`) is scoped by `account_id` to enforce ownership | Fixes the previously-noted "character-name ownership isn't enforced" gap. Cap of `MAX_CHARS_PER_ACCOUNT` (10, in `player_repo.h`) enforced at creation time. |
| Attribute set | Simplified 6-stat set (Strength/Dexterity/Constitution/Intelligence/Wisdom/Charisma), **not** the original's 12-stat STR/BRA/CON/DEX/AGI/INT/WIS/FOC/PER/CHA/KAR/SPE system (`misc/stats.h`) | User-approved simplification for a manageable text menu. `attrs_t` in `being.h`. |
| Point-buy rules | Every attribute starts at `ATTR_BASE` (120). `<stat> <amount>` sets that stat's *delta* from base (amount can be negative), individually capped at `ATTR_DELTA_CAP` (+/-30). The sum of all 6 deltas can't exceed `ATTR_POOL` (**30** as of Session 7, net) -- lowering one attribute frees up room to raise another. Since the pool now equals the per-attribute cap, a single maxed-out attribute exactly exhausts the whole pool by itself. `ATTR_MAX` (250) still exists as an absolute per-attribute ceiling, defense-in-depth beyond the delta cap, though it's unreachable under the current tuning (120+30=150 max). | **Revised in Session 4** (allocation-only → true trade-offs, +/-30 cap) **and Session 7** (pool 120 → 30, user said 120 felt too generous). |
| Attribute persistence | New table `db/sneezy/player_attrs.sql` (player_id PK/FK to `player.id`, `ON DELETE CASCADE`, one column per attribute, schema-only like `player.sql`/`account.sql` -- not in `SNEEZY_SEED_TABLES`) | The original doesn't persist attributes in the DB at all (uses a separate binary player-file system, not ported). This required touching `db/`, which was previously scoped out for the *rename* task specifically -- that boundary doesn't apply to genuinely new schema needed for a new feature; recorded here explicitly so it isn't second-guessed later. |
| Color codes | `<X>` tags (3 bytes: `<`, one letter, `>`), matching the original's exact syntax (`sys/{ansi,colorstring}.{h,cc}`) so any future ported content "just works." Translated centrally in `descriptor_send()` (`colorstring_translate()`, `net/colorstring.c`) -- the single choke-point that already did CRLF normalization (Session 9) -- into real ANSI escapes. Toggle is one `bool color_enabled` per `descriptor_t`, default on, **not** DB-persisted. | **Simplified vs. the original**: dropped the ~10-category color bitmask (rooms/mobs/objects/comm/etc, each independently toggleable) down to one on/off switch, since Tobin only has room-description text so far. Also dropped the original's immortal-only flash/background codes (`f`/`F`/`e`/`E`/etc) -- unrecognized tags pass through literally rather than vanishing, so nothing is silently lost once those get real handlers. |
| Levels | `progress_t` (`level`, `experience`, `hp`, `max_hp`) embedded in `being_t`. `MORTAL_LEVEL_MIN=1`, `MORTAL_LEVEL_MAX=50`, `IMMORTAL_LEVEL_MIN=51`, `IMMORTAL_LEVEL_MAX=60` -- directly mirrors the original's confirmed `MAX_MORT=50`/`GOD_LEVEL1=51`/`MAX_IMMORT=60` (misc/defs.h). `being_is_immortal()` = `level >= IMMORTAL_LEVEL_MIN`. Persisted in new `db/sneezy/player_progress.sql` (same schema-only pattern as `player_attrs.sql`). | **Simplified vs. the original**: single unified level, not per-class (the original is tied to a 9-class multiclass system Tobin doesn't have). Also no separate `PLR_IMMORTAL` flag bit -- reaching level 51 alone grants immortal status, since there's no staff-promotion workflow. **Consequence**: immortal status is currently unreachable through normal play (XP gain is hard-capped at level 50, see below) -- testing it requires hand-setting `level` in `player_progress` via SQL, as done in `tests/smoke_test_combat.py`. |
| XP curve | Placeholder: `progress_xp_for_level(level) = level*level*100`. `progress_add_xp()` auto-advances through crossed thresholds, hard-capped at `MORTAL_LEVEL_MAX` (50) -- no accidental immortal promotion via grinding. | The original's is a recursive kill-count formula tied to mob levels, which don't exist in Tobin yet (no NPCs). Explicitly a stand-in, revisit once mobs exist and a real kill-XP economy makes sense. Nothing currently calls `progress_add_xp()` in this pass -- combat defeat doesn't award XP yet either. |
| Pulse/task engine | `include/pulse.h`/`src/core/pulse.c`: a trimmed `TBaseProcess`/`TScheduler` equivalent -- fixed-size table of `{trigger_pulse, fn}`, `pulse_register()` at startup (`main.c`), `pulse_scheduler_run(pulse_num)` fires any process where `pulse_num % trigger_pulse == 0` (the original's exact modulus trigger). A pulse = 100ms, matching the original's literal `OPT_USEC`. `game_loop.c`'s `select()` timeout shrank from 1s to `OPT_USEC`; the scheduler runs once per loop iteration regardless of I/O readiness. | Trimmed to one process kind (global, no per-character/per-object process registry) since the only two recurring behaviors so far (wait-pulse decrement, combat rounds) both just iterate `g_descriptors` directly. Revisit if a third recurring per-character behavior (regen, crafting) shows up and the duplication stops being worth it. |
| Wait-state / immortal bypass | `int wait_pulses` on `being_t`. `being_get_wait()` returns 0 unconditionally for immortals, else the field -- direct port of the original's `getWait()`. `being_set_wait()` is a no-op for immortals -- direct port of `setWait()`'s guard, but centralized in one place instead of scattered per-command `if (!isImmortal())` checks throughout the original. `cmd_dispatch()` gates on `being_get_wait() > 0` (sends "You are still recovering!") right after the `quit!` special-case (so lag can never trap a player unable to quit) and before the abbreviation loop. | Commands that "take time" call `being_set_wait()` themselves (currently only `cmd_attack.c`); instant commands (`look`/`who`/`score`/`color`) never call it. **Important nuance found during testing**: `combat_process_run()`'s round-resolution trigger is a *global* pulse modulus, independent of any individual fight's start time or that fight's `wait_pulses` clock -- so the first round after an attack can land anywhere from 1 to `COMBAT_ROUND_PULSES` pulses later depending on global phase alignment, not a guaranteed fixed 1.2s. This matches the original's actual behavior (`perform_violence()` is also globally pulse-triggered), not a bug -- but it means the wait-clearing and round-resolution timers are deliberately independent clocks, not synchronized to each other. |
| Combat | Player-vs-player only (no `THING_MOB` instances exist yet). `being_t` gains `struct being *fighting` (opponent, NULL if none), a transient non-persisted `long last_combat_pulse` (dedup guard), and `limb_state_t limbs[LIMB_COUNT]` (6-limb HP breakdown, see the "Limbs" row below). `cmd_attack.c`/`cmd_kill.c` (mortals only, see "kill vs attack" row) set `fighting` on both sides + apply `COMBAT_ROUND_PULSES` wait; `combat_process_run()` resolves one strike-then-retaliate exchange per fighting pair per round (placeholder DEX/STR-based formula, now limb-aware). **Defeat (Session 14 change)**: no longer respawns the loser in-place while they keep playing -- HP is patched to `max_hp/2` first (so the next login isn't stuck at 0), then the loser is unloaded and dropped at the account menu (`descriptor_leave_to_menu()`, same path `quit!`-while-playing uses) with a `"You have been defeated by <winner>!\r\nYou are DEAD!\r\n"` message (or `"...slain by..."` for `combat_instakill()`). Not permadeath in the data sense -- the character record survives and is immediately replayable from the menu. | NPC combat, real weapon/armor-modified damage, and XP-on-kill are all explicitly future work (no `obj_t`/mobs exist). `being_destroy()` was extended to scan `g_descriptors` and clear any dangling `fighting` pointer aimed at the being being freed, before freeing it -- otherwise a disconnecting fighter would leave their opponent holding a use-after-free pointer for the next combat round. |
| `kill` vs `attack` | `cmd_kill.c`: for a mortal, `kill <target>` just calls `cmd_attack()` -- identical. For an immortal (`being_is_immortal()`, level ≥ 51), `kill <target>` instead calls `combat_instakill()` -- bypasses the multi-round process entirely, kills the target immediately, no wait-state cost. | Mirrors the original's `doKill()` (`misc/offense.cc`): normal attack unless the caller has the `POWER_SLAY` wiz-power (instant `rawKill()` then). Tobin has no wiz-power system, so this simplifies the gate to `being_is_immortal()` -- the original's `POWER_SLAY` holders are drawn from exactly that same level-51+ population anyway. No "can't slay a higher-level PC" guard yet (the original has one) -- not needed until immortal-vs-immortal `kill` is a real scenario. |
| Limbs | `limb_t` enum, 13 entries as of Session 19 (`LIMB_HEAD/NECK/LEFT_ARM/RIGHT_ARM/LEFT_FINGER/RIGHT_FINGER/BODY/WAIST/GENITALIA/RIGHT_LEG/LEFT_LEG/LEFT_FOOT/RIGHT_FOOT`, user-specified list/order -- was 12 in Session 17-18, 6 through Session 16) + `limb_state_t {hp, max_hp}` array on `being_t`. Each limb's max is `progress.max_hp / LIMB_COUNT` (placeholder even split -- ~1-2 on a fresh mortal with 13 limbs, so ordinary 1-6 damage hits destroy a limb outright in one blow almost every time). Every hit in `combat_strike()` rolls a uniformly-random limb and names it in the message. **Display is percentage-based (Session 15), not raw HP** -- `being_limb_pct()` (0-100). `score`'s `Limbs:` section only lists a limb once it's hurt (`limb_status_text()` non-NULL, `< 20%`); the dedicated **`limbs` command (Session 17, `cmd_limbs.c`)** always lists all `LIMB_COUNT` limbs unconditionally, healthy or not, each with its percentage and an injury-tier suffix when applicable. `limb_status_text(pct)` (being.h/being.c) returns a shared injury phrase used identically everywhere it shows up: `< 20%` "is hurt rather badly", `< 10%` "needs medical attention", `0%` "is destroyed and needs medical attention" (NULL/no line above 20%). Combat announces the phrase only when a hit crosses into a *worse* tier than the limb was in before that hit (edge-triggered). A destroyed limb (`being_has_destroyed_limb()`) applies a flat, non-stacking `DESTROYED_LIMB_HIT_PENALTY` (-15 to `hit_roll`) to that character's own attacks in `combat_strike()`. Not persisted (same precedent as `progress.hp`, only saved at defeat) -- there's no hospital system to repair a destroyed limb mid-game, so the only current cure is dying and respawning (`being_limbs_full_heal()` already runs at combat defeat). | As of Session 17, this was already a near 1:1 match of the original's real slot list (`wearSlotT` in `misc/limbs.h`: head/neck/arms/hands/body/waist/legs/feet/back), just without weighted hit-roll chances, equipment interactions, or `PART_USELESS`/`PART_BROKEN`/dismemberment gameplay effects (`misc/limbs.cc`) -- "finger" here in place of the original's "hand", no "back" slot. **`genitalia` (Session 19) is a user-requested addition beyond the original's actual slot list** (confirmed via `misc/limbs.h`: `wearSlotT` has no such slot) -- not a port of anything, purely Tobin-specific. The combat penalty is a flat single deduction regardless of how many limbs are destroyed (not compounding) -- a placeholder, not a real crippling-injury system. |
| Regen | `include/regen.h`/`src/core/regen.c`: `regen_tick_run()`, `pulse_register(REGEN_PULSES, ...)` (`REGEN_PULSES=50`, ~5s). Every playing character not `fighting` heals `1 + (CON above ATTR_BASE)/20` on overall HP and every limb (`being_heal()`). | Mirrors `TPerson::hitGain()` (`misc/limits.cc`, called every pulse via `addToHit(hitGain())`), which also explicitly zeroes gain while fighting -- same rule here. Placeholder rate, not the original's level/CON/hospital-room/drunk/camp-weighted curve. Gains aren't persisted between ticks (same precedent as combat HP). |
| `say` | New `src/cmd/cmd_say.c`. `say <message>` (and the `'` shorthand, see below) broadcasts to the speaker's room: speaker sees `You say, "<message>"`, everyone else sees `<Name> says, "<message>"`. Empty message rejected with `"Yes, but WHAT do you want to say?"`. No auto-added punctuation -- the message is used verbatim. The `'` one-character shorthand (no space required, e.g. `'hi` says `hi`) is handled directly in `cmd_dispatch()` (`cmd_table.c`): a leading `'` sets `verb = "say"` and `args` to everything after it (whitespace-trimmed), bypassing the normal whitespace-delimited verb split entirely so it isn't mangled by it. | Direct port of `TBeing::doSay()`'s message format and the original's `argument[0] == '\''` special-case in `TBeing::parseCommand()` (both `misc/talk.cc` / `misc/parse.cc`). **Not replicated**: the original's `garble()` (drunk/language distortion) and its green/cyan color-coding of the name and message -- plain text here, matching every other command's generated output so far (color is currently only used for room descriptions). The original also has `:`/`,` shortcuts (emote/similar) -- not ported, only `'`/`say`. |
| Objects (Phase 2C) | `obj_t` (`include/obj.h`/`src/core/obj.c`) collapses the original's 60 `itemTypeT` values into 16 `obj_category_t` buckets (`category_for_item_type()`, a single lookup table). Object PROTOTYPES are the upstream-seeded `obj` table (`db/sneezy/obj.sql`) read directly (`obj_repo.c`'s `obj_proto_load()`) -- no new prototype table. Containment (room floor / carried / worn / held) is the ONE existing `thing_t` chain (`stuff_head`/`stuff_next`/`parent`, `thing_move_to()`/`thing_remove_from_parent()` -- both pre-existing, unused until now); `being_t.equipment[LIMB_COUNT]`/`held[2]` are fast-lookup pointers into that same set, not separate storage. | `THING_OBJ` added to `thing_kind_t`. `obj.wear_flag` is stored verbatim in the original's upstream bit layout (not reinterpreted) so every already-seeded object works with zero migration; `wear_slot_for_flag()` translates to a Tobin `limb_t` only at wear time -- see the Limbs row for why HANDS/WRISTS/BACK/THROW have no mapping. Persistence (`db/sneezy/player_inventory.sql`) covers only player-carried/worn/held instances, saved immediately after every mutating command (not a generic save-at-quit) and loaded in `player_load()` but deliberately NOT `player_load_admin()` (would dangle a pointer through edplayer/set's snapshot-copy-then-destroy pattern once `being_destroy()` started freeing a populated inventory). Room-floor objects (via `oload`, `BUILD_MIN_LEVEL`) don't survive a restart -- no zone-reset system (2E) yet. `edobject` (the menu editor) is deliberately a separate future session. |
| Mobiles (Phase 2D) | A mob is just a `being_t` with `kind = THING_MOB`, `player_id`/`account_id = 0`, `desc` always NULL -- no new struct, matching the original's own `TMonster : TBeing` inheritance (confirmed by reading `misc/monster.h`). `being_create_mob(vnum)` (`being.c`) loads a prototype from the upstream-seeded `mob` table (`mob_repo.c`'s `mob_proto_load()`, no new Tobin table). | Mob `attrs_t` is derived from `level` (`ATTR_BASE + level`, capped `ATTR_MAX`), NOT the mob table's real 12-stat columns (a different, wider scale than Tobin's 6-stat system -- would unbalance `combat_strike()`). `max_hp` uses a placeholder formula built from `hpbonus` (the original's actual per-mob HP-scaling parameter). `combat_find_room_target()`/`combat_defeat()` widened (see decision row above the module table) rather than duplicated; `combat_process_run()` needed no changes at all. No mob-instance persistence (no owning player, no zone-reset system yet) -- an `mload`ed mob is lost on restart, like room-floor objects. `edmobile`, mob AI/aggression, zone resets, and XP-on-kill are all explicitly deferred. |
| `help`/`wizhelp` | New `src/cmd/cmd_help.c`, `cmd_entry_t` (moved from `cmd_table.c` into `cmd_internal.h`) gained `help` (one-line description) and `min_level` fields, plus a `cmd_table_entries(int *count)` accessor so `cmd_help.c` can enumerate `cmd_table.c`'s `COMMANDS[]` without duplicating it. `help` lists every command with its one-liner (plus a hardcoded `quit!` line, since that command is deliberately excluded from the dispatch table itself). `wizhelp` rejects a non-immortal caller (`"You are not privileged enough to use that command."`) and otherwise lists commands where `min_level > MORTAL_LEVEL_MAX` -- currently none, so it honestly prints `"(none yet -- no commands are currently immortal-only)"` rather than an empty or broken list. | **`wizhelp` is a genuine, direct port** of `TBeing::doWizhelp()` (`cmd/cmd_help.cc`): confirmed via source research that it really is a `commandArray[]` scan filtered by `minLevel > MAX_MORT`, not a file lookup -- Tobin's version does the exact same filter over its own command table. **`help` is a deliberate, documented simplification**: the original's `doHelp()` is a full file-based prose-topic system (separate `help/`, `help/_immortal`, `help/_skills`, `help/_spells` directories, a rebuildable index, per-topic `.ansi` variants, an external `bin/helpindex` binary for `help index`) -- entirely out of scope without a help-file content pipeline Tobin doesn't have. Tobin's `help` instead reuses the same command-list pattern `wizhelp` already needed for real, rather than attempting the file-based system. `min_level` is currently display-only metadata (drives the `help`/`wizhelp` split) -- no command's `min_level` is actually enforced by `cmd_dispatch()`, since nothing yet needs real access-gating (unlike the original's genuine `commandInfo::minLevel`-driven dispatch gate). |

## Module port status

| Module (orig) | Orig LOC | C port location | Status | Notes |
|---|---|---|---|---|
| sys/database.* | small | `src/db/db.c` | **Done, verified live** | 1:1 port |
| sys/socket.* | (part of sys/ 31K) | `src/net/main_socket.c`, `socket.c` | **Done, verified live** | |
| sys/connect.cc (`nanny()`) | (part of sys/ 31K) | `src/net/descriptor.c` | **Partial, verified live** | Account name → password → **account menu → create/play/delete character → point-buy attrs (new char) → playing**. Original has ~15 `CON_*` states (MOTD paging, wizlock, typed-password delete confirmation, etc) — those specific ones still deferred. |
| misc/thing.h, being.h | (part of misc/ 178K) | `include/thing.h`, `being.h`, `src/core/thing.c`, `being.c` | **Partial** | `attrs_t` (6-stat point-buy set) and `progress_t` (level/xp/hp) now on `being_t`, plus `fighting`/`last_combat_pulse`/`wait_pulses`. Still no `equipment`/`specials`/etc — those land with the future objects phase. |
| misc/account.cc | (part of misc/ 178K) | `src/db/account_repo.c` | **Done** (login-relevant slice only) | |
| player persistence | n/a in original (part of charfile/DB flow) | `src/db/player_repo.c` | **Done, verified live** | Multi-character-per-account: `player_load`/`player_load_room` scoped by account_id, plus `player_list_by_account`, `player_delete`, `player_attrs_load`/`player_attrs_save`, `player_progress_load`/`player_progress_save`. |
| room persistence | n/a (part of DB flow) | `src/db/room_repo.c` | **Done** (name/description/sector/exits only) | |
| sys/{ansi,colorstring}.{h,cc} | (part of sys/ 31K) | `include/colorstring.h`, `src/net/colorstring.c` | **Done, verified live** | `<X>` tag → ANSI translation, hooked into `descriptor_send()`. Immortal-only flash/background codes deferred. |
| sys/process.{h,cc} | (part of sys/ 31K) | `include/pulse.h`, `src/core/pulse.c` | **Done (trimmed scope), verified live** | Global-process-only `TBaseProcess`/`TScheduler` equivalent; no per-character/per-object process registry yet (see decisions table). |
| misc/combat.cc | (part of misc/ 178K) | `include/combat.h`, `src/core/combat.c`, `src/cmd/cmd_attack.c`, `src/cmd/cmd_kill.c` | **Partial, verified live** | Round-based combat with 6-limb HP + limb-named hit messages, passive regen when not fighting (PCs only), an immortal-only `kill` instant-slay, defeat ejecting a PC loser to the account menu (Session 14) or permanently destroying a mob loser (Session 35). Now supports PC-vs-mob as well as PC-vs-PC. No weapon/armor damage modifiers, no XP-on-kill, no mob AI/aggression yet. |
| misc/monster.h, sys/db.cc (`read_mobile`) | (part of misc/ 178K, sys/ 31K) | `src/db/mob_repo.c`, `being_create_mob()` in `src/core/being.c` | **Partial, verified live** | Mobs are `being_t` instances (`kind=THING_MOB`), not a separate struct -- matches the original's own `TMonster : TBeing`. Prototypes read straight from the upstream-seeded `mob` table; `attrs`/`max_hp` are placeholder formulas (level-derived, not the original's real 12-stat/dice system -- see the Mobiles decision row). No AI, no zone-reset spawning, no persistence. |
| misc/limbs.{h,cc}, misc/body.h | (part of misc/ 178K) | `being.h`/`being.c` (`limb_t`, `limb_state_t`), `combat.c` | **Simplified, verified live** | 6-limb placeholder set, not the original's real 13-slot equipment-aligned system -- see the "Limbs" decision row. |
| misc/limits.cc (`hitGain()`) | (part of misc/ 178K) | `include/regen.h`, `src/core/regen.c` | **Simplified, verified live** | Flat placeholder regen rate, not the original's level/CON/room-weighted curve -- see the "Regen" decision row. |
| obj/ (98 classes) | 28K | `include/obj.h`, `src/core/obj.c`, `src/db/obj_repo.c` | **Partial, verified live** | Collapsed to 16 `obj_category_t` buckets (not a per-class port). Prototypes read straight from the upstream-seeded `obj` table; instances use the existing `thing_t` containment mechanism. No object special-procs, no containers-holding-sub-items, no weapon/armor stat effects on combat yet -- see STATUS.md's Objects decision row. |
| disc/ (69 classes) | 40K | — | **Not started** | |
| spec/ | 36K | — | **Not started** | Already near-C in the original, low risk. |
| cmd/ (66 files) | 27K | `src/cmd/` | **109 handler files / 152 registered verbs** (audited 2026-07-19; stale "11/66" count was from Session 9-ish) | Dispatch table (`cmd_table.c`) does prefix/abbreviation matching, not exact-string lookup (Session 9) -- see the "Command parsing" decision row. `cmd_dispatch()` returns `bool` (every `cmd_*` handler's signature changed from `void` to `bool` to match) -- `quit!` returning `true` means "leave the character, back to the account menu"; only the account menu's own `quit!` (handled directly in `descriptor.c`, not through `cmd_dispatch`) returns `false` to actually disconnect. Every `CONN_PLAYING` reply ends with a trailing `\r\n> ` prompt (Session 9, blank line added Session 14). As of Session 10, `cmd_dispatch()` also gates on the wait-state before allowing any command through. As of Session 16, `cmd_dispatch()` also special-cases a leading `'` (see the "`say`" decision row) before the normal whitespace-delimited verb split. As of Session 18, `cmd_entry_t` (moved to `cmd_internal.h`, shared with `cmd_help.c`) carries a `help` one-liner and `min_level` per command -- display metadata only, not enforced by `cmd_dispatch()`. **"N/66 ported" is not a meaningful fraction and is retired as of this audit** -- a straight filename cross-check against the original's 66-file `cmd/` dir found only 17 direct name matches (`bash`, `consider`, `disarm`, `egotrip`, `help`, `kick`, `look`, `news`, `quest`, `save`, `score`, `set`, `show`, `stat`, `who`, `wiznews`, `zonefile` -- reimplemented/simplified, not 1:1 code ports). The other ~92 Tobin command files are new-to-Tobin *relative to `cmd/` specifically*: some have no original equivalent anywhere (account/character menu commands, `edit`-family building tools, `wiznet`, etc.); others port functionality the original kept **outside** `cmd/` entirely -- e.g. `cmd_attack.c`/`cmd_kill.c`/`cmd_hit.c`/`cmd_flee.c` correspond to the original's `fight.cc`, not any `cmd/` file, and movement (`cmd_move.c`) corresponds to `act.movement.cc`. 42 of the original's 66 files (`attribute`, `bodyslam`, `bonebreak`, `charge`, `chop`, `compare`, `deathstroke`, `dissect`, `doorbash`, `drive`, `feigndeath`, `focus_attack`, `fortify`, `get`, `grapple`, `headbutt`, `innate`, `jump`, `kneestrike`, `low`, `low_shop`, `mend_limb`, `message`, `orient`, `pracInfo`, `quivpalm`, `rally`, `rename`, `rescue`, `run`, `slam`, `spin`, `stab`, `steal`, `stomp`, `testcode`, `testfight`, `trip`, `trophy`, `visible`, `whirlwind`, `zones`) still have no Tobin counterpart by name -- mostly combat maneuvers and the `low`/shop subsystem. |
| game/ | 8.7K | — | **CUT** | No port needed, ever (user-approved). |
| task/ | 10.8K | — | **Not started** | Professions to keep not yet chosen. |
| world.c (new, not in original) | — | `src/world.c` | **Done (current scope)** | Lazy per-vnum room registry, no boot-time bulk load yet. |

## Open questions / needs decision

- [x] **Build-verified** on db.kullit.com (10.0.0.12, Fedora Linux 44) via SSH: `cmake -B build && cmake --build build` compiles clean, zero warnings.
- [x] **Live end-to-end verified**: seeded `sneezy`/`immortal` DBs via `db/init-db.sh` (19,209 rooms loaded), ran `tobin_c`, drove it with `tests/smoke_test.py` and `tests/smoke_test_login.py` (raw-socket scripted sessions, not real `telnet`, but exercise the identical wire protocol). Confirmed: new account+character creation, landing in room vnum 1 ("Imperia") with correct description, `look`, `who`, unknown-command handling, returning login with password verification against the stored crypt() hash, wrong-password rejection, loading an *existing* character (not re-creating it), and two concurrent connections each correctly showing both players in `who` (and the second player's auto-`look` correctly listed "Fred is here." — confirms the room contents linked-list works for concurrent occupants).
- [x] **Account/character menu built + verified live**: `tests/smoke_test_accounts.py` covers new-account → empty menu → create character → point-buy (including overspend rejection) → play → `score` shows the persisted allocation → reconnect → menu shows the character → create a second character → delete the first with `YES` confirmation → reconnect again and confirm the deletion persisted and the surviving character is unaffected. All passed. `tests/smoke_test.py` and `smoke_test_login.py` updated to go through the new menu flow and re-verified (including the multi-user `who` and wrong-password checks from before).
- [x] **Character-name ownership is now enforced** (was an open item) — every `player_repo.c` lookup/mutation is scoped by `account_id`.
- [ ] Verified with scripted raw-socket sessions, not yet with a real interactive `telnet`/Mudlet/etc client — the byte-level protocol is identical, but worth a manual pass too (this now matters more, since the menu/point-buy UX is exactly the kind of thing that benefits from an actual human trying it).
- [x] `IAC SB ... SE` telnet subnegotiation split across two reads — **fixed Session 20**: resumable parser state (`in_subneg`/`subneg_prev` on `descriptor_t`), verified by the new `tests/smoke_test_telnet_iac.py` (deliberately splits a TTYPE subnegotiation mid-payload across two TCP sends, plus regression guards for the already-working split WILL/DO and lone-IAC rewind paths). Mudlet/MUSHclient-class clients are now safe.
- [x] Account-creation password confirmation step (type it twice) — done: the `CONN_CONFIRM_PASSWORD` state re-prompts and must match before the account is created.
- [x] Delete-time password reconfirmation — done: after the typed `YES`, `CONN_CHAR_DELETE_PASSWORD` re-verifies the account password (`account_verify_password()`) before `player_delete()` runs; a wrong password cancels and the character survives (ported from the work-2026-07-06 Session 25 work). `smoke_test_accounts.py` (wrong-password-cancels + correct-password-succeeds) and `smoke_test_menu_letters.py` (`d` shorthand) cover it.
- [ ] If a character is deleted while a *different* session has it actively loaded/playing, that session isn't kicked or notified — not handled (edge case, unlikely at this scale, but noted).
- [x] Point-buy now supports true trade-offs (was an open item) — lower a stat down to `ATTR_BASE - ATTR_DELTA_CAP` (90) to free up room to raise another up to `ATTR_BASE + ATTR_DELTA_CAP` (150), still bounded overall by the net pool. Verified live with `tests/smoke_test_trade_attrs.py` (per-attribute cap in both directions, and the net-pool-exhaustion case specifically, distinct from the per-attribute cap).
- [ ] Which 1-2 `task/` professions to keep, and which ~8-10 `disc/` disciplines — proposed defaults in the decisions table above, not finalized.
- [ ] Windows build path intentionally not attempted (see "Target platform" decision above) — confirm this is still fine before anyone tries to build on Windows directly.
- [x] **Color, levels, pulse engine, and PvP combat built + verified live** — see Session 10 below for the full breakdown. All four raw-socket-verified, including at the byte level for color (ANSI escapes present/absent correctly) and for the wait-state/immortal-bypass timing.
- [x] **Immortal rank titles + bracketed/centered `who` display** — see Session 11 below. 51-60 now show a rank title (Immortal/God/Greater God/Administrator/Implementor) instead of a raw number in both `score` and `who`; `who`'s level field is centered in a fixed-width `[ ]` bracket.
- [x] `tests/smoke_test_color.py` is **self-contained as of Session 20**: color tags are injected via `say` (discovered during the first interactive client pass -- say messages pass verbatim through `descriptor_send()`'s translation), so no hand-staged DB content is needed. The suite is fully green (18/18) for the first time.
- [ ] Immortal promotion has no in-game path (see the "Levels" decision row) — the only way to create a level 51+ character right now is a manual `UPDATE player_progress SET level=51 ...`. A `promote <name>` (or similar) command is future work.
- [ ] HP is only persisted at combat defeat, not after every exchange — a mortal who disconnects mid-fight (without losing) will reload at whatever HP was last saved (full, from creation), not their actual HP at disconnect time. Minor gap, not addressed this pass.
- [ ] No XP is awarded for winning a fight yet (`combat_defeat()` doesn't call `progress_add_xp()`) — deliberately deferred as a one-line follow-up once a reward number is chosen.
- [ ] `perform_violence()`-style sub-round attack staggering (spreading multiple blows within one combat round for a "roundless" feel) was explicitly not replicated — Tobin resolves exactly one strike-then-retaliate exchange per round, simpler than the original.
- [x] **`kill` command, limb-based combat, passive regen, death-ejects-to-menu built + verified live** — see Session 14 below. All 13 regression tests pass, including 3 new ones.
- [ ] Limb HP and regen gains aren't persisted between exchanges/ticks — same already-accepted gap as overall combat HP (previous bullet), just extended to the new state. A character who disconnects mid-regen or mid-limb-damage reloads at whatever was last saved (only written at combat defeat).
- [x] **A limb reaching 0 HP now has a real gameplay effect** (was an open item) — see Session 15 below. A destroyed limb applies a flat hit-chance penalty (`DESTROYED_LIMB_HIT_PENALTY`) to that character's own attacks. Still doesn't end a fight on its own (only overall HP ≤ 0 does), and the original's real `PART_USELESS`/`PART_BROKEN` effects (equipment interactions, etc) aren't replicated.
- [x] **Limb health is now shown as a percentage with narrative injury tiers** (was implicit in Session 14's raw hp/max_hp display) — see Session 15 below. `score` and combat both use the same `limb_status_text()` wording.
- [ ] No hospital system exists to repair a destroyed limb mid-game — `limb_status_text()`'s "needs medical attention" wording is currently just descriptive flavor. The only actual cure right now is dying and respawning (`being_limbs_full_heal()` already runs at combat defeat, confirmed Session 15). A real hospital mechanic (a room/command that heals a destroyed limb without requiring death) is future work if that flavor text should become literal.
- [ ] `combat_instakill()` (immortal `kill`) has no "can't slay a PC of equal or higher level" guard, unlike the original's `doKill()` — not needed until immortal-vs-immortal `kill` is a realistic scenario (currently the only way to reach level 51+ at all is a manual SQL `UPDATE`, so two immortals meeting in the wild isn't yet a normal-play situation).
- [ ] **Future direction (user-stated, not yet designed)**: once an `obj_t`/equipment system exists, a defeated character's carried equipment should fall to the ground in the room they died in (not follow them to the account menu) — they'd need to return to that room to retrieve it. Worth designing in from the start when `obj/` work begins, rather than retrofitting onto `combat_defeat()`'s account-menu ejection.

## Session log

### Session 10 — 2026-07-02 — Color codes, 50/10 levels, pulse-based task engine, PvP combat
Large multi-part addition, researched from the original SneezyMUD source before designing (three parallel Explore-agent passes over `code/code/sys/{ansi,colorstring,comm,process}.{h,cc}` and `code/code/misc/{being,person,defs,gaining,combat}.{h,cc}`), then a grounded technical design pass reading Tobin's actual current files, then a full plan presented and approved before any code was written.

- **Color**: new `include/colorstring.h`/`src/net/colorstring.c` (`colorstring_translate()` -- scans for `<X>` 3-byte tags, emits the matching ANSI escape or strips it, unrecognized tags pass through literally). Hooked into `descriptor_send()` (`src/net/descriptor.c`) ahead of the existing CRLF pass. New `bool color_enabled` on `descriptor_t` (default true, not persisted). New `color on`/`color off`/bare `color` command (`src/cmd/cmd_color.c`).
- **Levels**: new `progress_t` (`level`, `experience`, `hp`, `max_hp`) embedded in `being_t` (`being.h`). Constants `MORTAL_LEVEL_MIN/MAX` (1/50), `IMMORTAL_LEVEL_MIN/MAX` (51/60) -- directly mirrors the original's confirmed `MAX_MORT`/`GOD_LEVEL1`/`MAX_IMMORT`. `being_is_immortal()`, placeholder `progress_xp_for_level()`/`progress_add_xp()` (capped at 50) in `being.c`. New `db/sneezy/player_progress.sql` (same schema-only pattern as `player_attrs.sql`) + `player_progress_load`/`_save` in `player_repo.*`, wired into `player_load()`/`player_create()`. `score` now shows Level/Experience/HP; `who` now prefixes each name with `[level]` (both per explicit follow-up requests) and marks immortals with `(immortal)`.
- **Pulse engine**: new `include/pulse.h`/`src/core/pulse.c` (trimmed `TBaseProcess`/`TScheduler` equivalent, global-process-only, modulus-triggered). `game_loop.c`'s `select()` timeout shrank from 1s to `OPT_USEC` (100ms, matching the original's literal pulse unit); the scheduler now runs every loop iteration regardless of I/O readiness. New `being_t.wait_pulses` + `being_get_wait()`/`being_set_wait()` (both immortal-bypassing, centralizing what the original scatters as per-command `if (!isImmortal())` checks) in `being.c`. New `src/core/wait_tick.c` (decrements every playing character's wait by 1 per pulse), registered `pulse_register(1, wait_tick_run)`. `cmd_dispatch()` (`cmd_table.c`) gained a wait-check gate right after the `quit!` special-case.
- **Combat**: `being_t` gained `fighting`/`last_combat_pulse` (both transient, not persisted). New `src/cmd/cmd_attack.c` (same-room player targeting, reusing `cmd_look.c`'s room-walk pattern) sets `fighting` on both sides and applies `COMBAT_ROUND_PULSES` (12, ~1.2s) wait. New `include/combat.h`/`src/core/combat.c`: `combat_process_run()`, registered `pulse_register(COMBAT_ROUND_PULSES, combat_process_run)`, resolves one strike-then-retaliate exchange per fighting pair per round (placeholder DEX/STR-based formula). Defeat: no permadeath, loser reset to `max_hp/2` HP and returned to their `load_room` (reusing `enter_world()`'s exact lookup), shown it via `cmd_dispatch(loser->desc, "look")`. `being_destroy()` extended to scan `g_descriptors` and null out any dangling `fighting` pointer aimed at the being being freed, before freeing it -- otherwise a disconnecting fighter leaves their opponent holding a use-after-free pointer.
- `main.c` gained `srand()` (for combat's `rand()` calls) and the two `pulse_register()` calls before `game_loop_run()`.
- **Testing**: new `tests/smoke_test_combat.py` covers the wait-state block/clear cycle, confirms rounds resolve HP changes over real time (not synchronously in the attack command), and hand-promotes a test character to level 51 via direct SQL to verify the immortal bypass (never blocked, even immediately after attacking) -- since there's no in-game promotion path yet. Hit a real timing bug while writing it: `combat_process_run()`'s round trigger is a *global* pulse modulus, not a per-fight timer, so a round can land anywhere from 1 to 12 pulses after an attack depending on phase alignment -- the test's own generous `recv_all()` idle-timeout was accidentally eating enough wall-clock time for a round to sneak in before the "immediately blocked" assertion ran. Fixed by sending the follow-up command back-to-back with the attack instead of waiting on a slow read first. This matches the original's actual behavior (global-pulse-triggered `perform_violence()`), not a bug in Tobin.
- New `tests/smoke_test_color.py` verifies ANSI translation at the raw-byte level in both directions (color on → real escapes, no raw tags; color off → tags stripped, no escapes). Since no seed content currently contains `<X>` tags, this required temporarily hand-editing room vnum 1's description via direct SQL (captured the exact original bytes first, restored them exactly afterward, restarted the server both times since `world.c` caches loaded rooms in memory and a bare DB `UPDATE` wouldn't otherwise take effect) -- documented as a testing limitation in Open Questions above.
- Rebuilt clean (zero warnings, first try) on db.kullit.com, re-seeded the DB with `player_progress.sql`, ran the full 8-test regression suite (all prior tests plus the two new ones) -- all pass. (`smoke_test_color.py` specifically requires the manually-staged tagged content and was not re-run against the final, restored DB state -- it already passed once against the staged content, see above.)
- Next: a real interactive `telnet` pass; the several open items listed above (immortal promotion path, mid-fight HP persistence, XP-on-kill); then either NPC/mob support (which combat and levels are both clearly designed to extend into) or Phase 2 of the original roadmap (`spec/`/`cmd/` near-C ports, or the `obj/` category-collapse design).

### Session 11 — 2026-07-02 — Immortal rank titles, bracketed/centered `who` display
Two follow-up requests refining Session 10's level display.

- **Rank titles**: new `being_level_title(int level)` in `being.h`/`being.c` -- returns `"Immortal"` (51-53), `"God"` (54-57), `"Greater God"` (58), `"Administrator"` (59), `"Implementor"` (60+), or `NULL` for a mortal level (caller falls back to showing the raw number). `cmd_score.c` and `cmd_who.c` both call it: `score`'s `Level:` line shows the title in place of the number for immortals; `who` shows the title, or `"Level: N"` for mortals (previously just the bare number).
- **Bracketed/centered `who`**: `cmd_who.c` gained a `center_pad()` helper and `WHO_LEVEL_FIELD_WIDTH` (13, matching `"Administrator"`, the longest title) -- each row's level/title field is centered within that fixed width and wrapped in `[ ]`, so every row lines up regardless of whether it's showing a short title (`"God"`), a long one, or a `"Level: N"` fallback. Example: `[     God     ] Playername`, `[Administrator] Playername`.
- Set player **Jesus** to level 60 (`Implementor`) in `player_progress` for testing, via a self-verifying `INSERT ... SELECT id FROM player WHERE name='Jesus' ... ON DUPLICATE KEY UPDATE` (avoids a bare hardcoded `player_id`).
- New `tests/smoke_test_level_titles.py`: creates one character per tier boundary (51, 53, 54, 57, 58, 59, 60) plus mortal levels 1 and 50, hand-sets each via SQL, reconnects to force a DB reload, and checks both `score`'s title text and `who`'s exact bracketed/centered string. All 22 checks passed.
- `tests/smoke_test_combat.py` had a stale assertion from before this change (`"(immortal)" in out`, checking for a marker Session 10's `who` line used but that never existed in `score`'s output) -- updated to check the `Level:` line for the `"Immortal"` title text instead.
- Rebuilt clean (zero warnings), restarted `tobin_c`, reran the full 8-file regression suite (`smoke_test.py`, `smoke_test_accounts.py`, `smoke_test_quit.py`, `smoke_test_quit_menu.py`, `smoke_test_trade_attrs.py`, `smoke_test_quit_creation.py`, `smoke_test_parser_display.py`, `smoke_test_combat.py`) plus the new `smoke_test_level_titles.py` -- all pass.
- Next: same open items as Session 10 (immortal promotion path, mid-fight HP persistence, XP-on-kill); consider color-coding rank titles by tier now that the color system exists, and whether the bracket/centered style should extend to other list-style displays.

### Session 12 — 2026-07-02 — Plain-`make` build, standalone README, take-home package
User wanted a copy of the codebase to take home and experiment with independently.

- New `c_port/Makefile` — a plain-make alternative to `CMakeLists.txt` for anyone without CMake. Verified it needed `-std=gnu11`, not `-std=c11`: CMake's `CMAKE_C_EXTENSIONS` defaults ON, so the actual flag the working CMake build has always used is `-std=gnu11` (confirmed via `VERBOSE=1 cmake --build build`) — a strict `-std=c11` Makefile failed with an `implicit declaration of localtime_r` error (POSIX extensions hidden under strict ISO C). Fixed and verified a clean, zero-warning build with the same flags/libraries as CMake (`libmariadb`, `libcrypt`).
- Rewrote `c_port/README.md` as a standalone setup guide (prerequisites table for Debian/Ubuntu + Fedora, DB seeding steps, both build paths, run instructions, test-suite instructions) and corrected two stale claims: the "no combat yet" line (false since Session 10) and the point-buy description (still described the old 120-point/max-250 pre-trade system from before Session 4).
- Packaged `c_port/` + `db/` (seed data, required to boot) + `LICENSE.txt` into `tobin_takehome.zip` at the repo root via PowerShell `Compress-Archive`.
- One process note: testing the Makefile in isolation involved `rm -rf build` on the box, which deleted the on-disk binary the live `tobin_c` process was running from (Linux kept it running fine via the open file handle) — rebuilt `build/` via CMake afterward so a future restart wouldn't fail. No live impact, but worth remembering before repeating this kind of isolated-build test against a running deployment.
- Next: same as Session 11.

### Session 13 — 2026-07-02 — Character names normalized to proper case
User asked that character names always display in proper case (e.g. "Testguy", not "testguy") regardless of how they're typed.

- New `being_normalize_name(char *name)` in `being.h`/`being.c` (`<ctype.h>` `toupper`/`tolower`) — uppercases the first character, lowercases the rest, in place. Applied once, at the single choke point where a new character's name is captured (`CONN_CHAR_CREATE_NAME` in `descriptor.c`), right before it's stored in `d->new_char_name` and handed to `player_create()` — mirrors the original SneezyMUD's approach exactly (`sstring(parsed_name).cap()`, applied once in `sys/create_character.cc`, confirmed by reading that file). Because the name is canonicalized once at creation and the DB is the single source of truth from then on, every later display (the account menu, `score`, `who`, `look`'s room listing, `attack`/combat messages) already shows the correctly-cased name with no per-call-site formatting needed.
- Checked whether any already-existing player names in the DB needed a retroactive fix: a SQL query comparing each `name` against its own proper-cased form found zero mismatches — every pre-existing name (including ones created by earlier test runs, e.g. `AttrTester14259`) already happened to be properly cased, so no data migration was necessary.
- New `tests/smoke_test_name_case.py`: creates three characters via all-lowercase, ALL-CAPS, and MiXeD-case typed input, and confirms `score`, `who`, and `look`'s room-occupant listing all show the same proper-cased form regardless of input case (6 checks, all pass).
- Fixed 4 existing tests whose fixture names happened to contain a mid-word capital letter (e.g. `QMChar`, `RealChar`, `CombatA`/`CombatB`/`CombatImmortal`/`CombatTarget`, `TitleL51`/`TitleMortal1`) and asserted the raw as-typed string against server-echoed output — normalization now lowercases everything after the first letter, so e.g. `"CombatB12345"` displays as `"Combatb12345"`. Added a small `proper(name)` test helper (mirroring `being_normalize_name()`) in `smoke_test_quit_menu.py`, `smoke_test_quit_creation.py`, `smoke_test_level_titles.py`, and `smoke_test_combat.py`, applied only to the output-text assertions — command arguments (`attack <name>`) and SQL `WHERE name=...` lookups were untouched and still work as-typed, since `cmd_attack.c` matches with `strcasecmp` and the `player.name` column's collation (`utf8mb4_general_ci`, confirmed via `SHOW CREATE TABLE`) is case-insensitive.
- Rebuilt clean (zero warnings), restarted `tobin_c`, ran the full regression suite (all 10 test files, including the new one) — all pass.
- Next: same open items as Session 10/11.

### Session 14 — 2026-07-02 — `kill` command, limb-based combat, passive regen, death now ejects to the account menu
A large batch of related combat requests in one pass, researched from the original before designing (an Explore-agent pass over `misc/limbs.{h,cc}`, `misc/body.h`, `misc/offense.cc`'s `doKill()`, and `misc/limits.cc`'s `hitGain()`, confirming all three concepts genuinely exist in the original — not inventions for this port).

- **`kill` command**: new `src/cmd/cmd_kill.c`. For a mortal, `kill <target>` just falls through to `cmd_attack()` — identical behavior, identical messages. For an immortal (`being_is_immortal()`, level ≥ 51), `kill <target>` instead calls the new `combat_instakill()` (`combat.c`) — kills the target immediately, bypassing `combat_process_run()`'s multi-round resolution entirely, no wait-state cost. Mirrors the original's `doKill()` (`misc/offense.cc`): calls `doHit()` (normal attack) unless the caller has the `POWER_SLAY` wiz-power, in which case it's an instant `rawKill()`. Tobin has no wiz-power system, so this simplifies that gate to `being_is_immortal()` — the original's `POWER_SLAY` holders are drawn from exactly the same `GOD_LEVEL1 == 51` population anyway. Extracted the room-target lookup both `cmd_attack.c` and `cmd_kill.c` need into a shared `combat_find_room_target()` (`combat.c`/`combat.h`).
- **Limb-based combat**: new `limb_t` enum + `limb_state_t` (`hp`/`max_hp`) in `being.h`, `limb_state_t limbs[LIMB_COUNT]` on `being_t`. 6 limbs (head, torso, left/right arm, left/right leg) — a deliberate simplification of the original's real per-slot system (`bodyPartsDamage body_parts[MAX_WEAR]` in `misc/being.h`, driven by `wearSlotT`'s 13 equipment-aligned slots and race-specific `slotChance()` weighting in `misc/limbs.cc`), same simplification philosophy as `attrs_t`'s 6-stat collapse of the original's 12. Each limb's `max_hp` is an even placeholder split of `progress.max_hp / LIMB_COUNT` (`being_limbs_full_heal()`), not the original's per-slot-weighted `hitLimit()`. `combat_strike()` (`combat.c`) now rolls a uniformly-random limb per hit (not the original's `slotChance()`-weighted `getPartHit()`) and applies damage to both that limb's HP and the defender's overall HP via `being_hurt_limb()`; messages name the limb ("You hit X's left arm for 4 damage!"), and a limb crossing 0 for the first time gets an extra "goes limp and useless!" message (flavor only — no gameplay penalty yet, unlike the original's real `PART_USELESS` effects). `score` gained a `Limbs:` breakdown showing each limb's current/max HP.
- **Passive regen**: new `include/regen.h`/`src/core/regen.c`, `regen_tick_run()` registered via `pulse_register(REGEN_PULSES, regen_tick_run)` (`REGEN_PULSES = 50`, ~5s). Every playing character not currently `fighting` heals a small placeholder amount (1 + 1 per 20 CON above `ATTR_BASE`) on both overall HP and every limb, via the new `being_heal()`. Mirrors the original's `TPerson::hitGain()` (`misc/limits.cc`), called every pulse via `addToHit(hitGain())`, which also explicitly zeroes the gain while fighting (`if (fight()) gain = 0;`).
- **Defeat now ejects to the account menu** (behavior change, user-requested): `combat_defeat()` (`combat.c`) no longer respawns the loser in-place at their load room while they keep playing. The loser's message changed to `"You have been defeated by <winner>!\r\nYou are DEAD!\r\n"` (or `"You have been slain by <winner>!\r\nYou are DEAD!\r\n"` for `combat_instakill()`'s victims — `slain` only picks this first line, both paths now end the same way), followed by `descriptor_leave_to_menu()` — the same path `quit!`-while-playing uses. This is **not** permadeath in the data sense: the character record isn't deleted, HP is patched to half max first (so the next login isn't stuck at 0), and the player can immediately pick the same character back up from the account menu, create another, or leave — matching the user's literal spec ("presented with the account menu so they can connect a character or leave the game"). Simplified `combat_defeat()` considerably: no longer needs to look up/re-enter the loser's load room or trigger an auto-`look`, since they're not staying in-world.
- **`\r\n` before every prompt**: 4 spots in `descriptor.c` (`enter_world`'s post-look prompt, `show_account_menu`, `show_attr_screen`, the `CONN_PLAYING` trailing prompt) changed from sending a bare `"> "` right after informational output to `"\r\n> "`, so there's always a blank line separating displayed information from the input prompt.
- **Testing**: new `tests/smoke_test_kill.py` (mortal `kill` == `attack`; immortal `kill` produces the instant-slay message, no wait-state, and the target is properly ejected — sees "You are DEAD!" and the account menu, character still listed), `tests/smoke_test_limbs.py` (score's `Limbs:` section lists all 6 with HP/max pairs; at least one combat exchange names a specific limb), `tests/smoke_test_regen.py` (hand-sets a fresh character's HP low via direct SQL — deliberately not chained off combat, since defeat no longer leaves a "damaged but still playing" character in-world — reconnects, confirms the low value was picked up, waits ~11s, confirms HP increased on its own without fighting). The death/ejection change broke 3 existing tests' assumptions and required fixes: `smoke_test_kill.py`/`smoke_test_regen.py` (see above) and `smoke_test_combat.py` (Part 2's HP-progression check now tolerates a participant actually dying mid-test — checks for a live score sheet OR a proper "You are DEAD!" + account-menu ejection, since either outcome proves rounds resolved over real time, which was the actual point of that check).
- One test iteration bug during verification: `smoke_test_regen.py` initially asserted `hp_before == low_hp` (exact) right after reconnecting, which failed — not because the DB write was wrong (confirmed correct via direct query), but because this test suite's `recv_all()` helper blocks for a full `timeout` (1s) *after* the last byte of a reply arrives (it only returns on an idle gap, not on message completion), so ~10 such calls deep into the script, several seconds had already elapsed — comfortably enough for a `REGEN_PULSES` tick (~5s) to have already nudged HP up before the check ran. Loosened to a range check (`low_hp <= hp_before < max_hp`).
- Rebuilt clean (zero warnings) at every step, restarted `tobin_c`, ran the full 13-file regression suite (all prior tests plus the 3 new ones) — all pass.
- Next: the equipment/`obj_t` system doesn't exist yet, but the user's stated direction is for a defeated character's equipment to fall to the ground in the room (not follow them to the account menu), so they'd have to return and retrieve it — worth designing in from the start once `obj_t` lands, rather than retrofitting. Also: no XP-on-kill still deferred; limb HP and regen gains still aren't persisted between exchanges/ticks (same already-accepted precedent as overall HP, see Open Questions); a limb crossing 0 has no real gameplay effect yet (no attack penalty, can't be a killing blow on its own — only overall HP ≤ 0 ends a fight); `combat_instakill()` has no original-style "can't slay someone higher-level than you" guard (the original's `doKill()` has one) — worth adding if immortal-vs-immortal `kill` ever comes up.

### Session 15 — 2026-07-02 — Percentage-based limb display, injury-tier messages, destroyed-limb combat penalty
Follow-up refining Session 14's limb-HP feature: raw numbers replaced with a percentage + narrative injury tiers, plus a first real gameplay consequence for a destroyed limb.

- **Percentage display, healthy limbs hidden entirely**: new `int being_limb_pct(const being_t *b, limb_t limb)` (`being.h`/`being.c`) — `(hp * 100) / max_hp`, clamped 0-100. Per explicit user follow-up ("no other messages should appear about limb health until they reach 20% limb health"), `score`'s `Limbs:` section (`cmd_score.c`) doesn't list every limb unconditionally — a limb only appears at all once it's actually hurt (`limb_status_text()` returns non-NULL, i.e. `< 20%`), as one line per injured limb: `"Your left arm is hurt rather badly! (18%)"`. A fully healthy character shows no `Limbs:` section at all.
- **Injury tiers**: new `const char *limb_status_text(int pct)` — returns a sentence fragment completing `"Your <limb> ___"`: `< 20%` → `"is hurt rather badly"`, `< 10%` → `"needs medical attention"`, `0%` → `"is destroyed and needs medical attention"`; `NULL` (no message) at `>= 20%`. `score` prints one `"Your <limb> <text>!"` line per limb below full health, right after the percentage table. `combat_strike()` (`combat.c`) computes each hit's limb percentage before and after, and announces the phrase to both sides (`"%s's %s %s!"` / `"Your %s %s!"`) only when the hit pushed the limb into a *worse* tier than it was already in — this replaces the old single "goes limp and useless" message from Session 14, subsumed by the `0%` tier's text now that there are three tiers instead of one. Since a fresh mortal's limb max is only ~4 HP (`progress.max_hp / 6`) against 1-6 damage per hit, a single solid blow routinely destroys a limb outright in one strike rather than working through the tiers gradually — confirmed live during testing, not a bug (see below).
- **Combat penalty for a destroyed limb**: new `bool being_has_destroyed_limb(const being_t *b)` (`being.h`/`being.c`) — true if any limb is at 0 HP. `combat_strike()` applies a flat, non-stacking `DESTROYED_LIMB_HIT_PENALTY` (-15) to the attacker's own `hit_roll` if they have any destroyed limb, before the miss/hit check — a real (if simple/placeholder) consequence, not just flavor text. Matches the user's literal spec ("penalized in combat"). No hospital system exists yet to repair a destroyed limb mid-game (`"needs medical attention"` is currently just descriptive) — the only actual cure right now is dying and respawning, since `combat_defeat()` already unconditionally calls `being_limbs_full_heal()` on the loser (confirms the user's last point — "once they've died completely and choose to respawn, their limbs are fully restored to max health" — was already true from Session 14's implementation, no new code needed for it).
- **Testing**: extended `tests/smoke_test_limbs.py` — Part 1 confirms a fresh, undamaged character shows zero injury lines and no `Limbs:` section at all; Part 2's combat loop (8 rounds) confirms both a limb-naming message and at least one injury-tier phrase appear (near-certain given how fragile placeholder limb HP is), tracking per-socket which specific character got hurt; Part 3 then checks that same character's `score` reflects the injury with matching wording and a percentage (skipped gracefully if that character was fully defeated and ejected to the account menu in the same window, rather than failing).
- One design iteration during this session: initially `score` printed every limb's percentage unconditionally (a compact table) plus separate narrative lines only for injured ones. User follow-up ("no other messages should appear about limb health until they reach 20% limb health") prompted a clarifying question (`AskUserQuestion`) on whether that also meant hiding the percentage table entries for healthy limbs, not just suppressing the narrative messages -- user chose "hide healthy limbs entirely," so the two-pass table+narrative design was collapsed into the single injured-only-lines format described above.
- Rebuilt clean (zero warnings) at each iteration, restarted `tobin_c`, reran the full 13-file regression suite twice (once before, once after the hide-healthy-limbs revision) — all pass both times.
- Next: same as Session 14's "Next" list, plus consider whether the destroyed-limb hit penalty should scale with the *number* of destroyed limbs rather than being a flat one-time deduction, once real playtesting shows whether the flat version feels right.

### Session 16 — 2026-07-02 — `say` command + `'` shorthand
User reported `say`/`'` as "no longer functioning" -- checked first (`grep` across `src/`) and confirmed neither had ever existed in Tobin at all (no `cmd_say.c`, nothing registered in `cmd_table.c`), so this was a missing feature, not a regression. Researched the original before building, same as every other command this project has ported.

- New `src/cmd/cmd_say.c`: `say <message>` sends `You say, "<message>"` to the speaker and `<Name> says, "<message>"` to everyone else in the room (skipping the speaker and anyone without a live `desc`). Empty message rejected with `"Yes, but WHAT do you want to say?"`. No auto-added punctuation. Registered as `{ "say", cmd_say }` in `cmd_table.c`, so normal abbreviation matching applies (e.g. `sa hello`) same as every other command.
- **`'` shorthand**: `cmd_dispatch()` (`cmd_table.c`) now special-cases a leading `'` *before* the normal whitespace-delimited verb split -- sets `verb = "say"` and `args` to everything after the `'` (leading spaces trimmed, none required), so `'hello there` says "hello there" rather than being mis-split as a malformed verb token. Mirrors the original's exact special-case in `TBeing::parseCommand()` (`misc/parse.cc`): `if (argument[0] == '\'') { arg1 = "'"; argument.erase(0, 1); }`, bypassing that function's normal whitespace-based split for this one case. The original also shortcuts `:`/`,` the same way (other commands) -- not ported, only `'`/`say`.
- Both routes (`say ...` and `'...`) flow through the exact same wait-state check and command lookup as every other command, since the `'` case is normalized into `verb`/`args` up front rather than handled as a separate code path -- no duplicated logic.
- New `tests/smoke_test_say.py`: confirms `say` and `'` produce identical speaker/room message pairs, confirms the empty-message guard fires for both `say` (bare) and `'` (bare), all 6 checks pass.
- Rebuilt clean (zero warnings), restarted `tobin_c`, ran `smoke_test_say.py` plus the full existing 13-file regression suite (14 total) — all pass.
- Next: the original's `:`/`,` shortcuts (likely emote and a third command) aren't ported -- pick up if/when those commands themselves get built. `say` has no color-coding yet (the original colors the name cyan and the message default), consistent with every other command's plain-text output so far.

### Session 17 — 2026-07-02 — 12-limb set + dedicated `limbs` command
User asked for a `limbs` command showing every limb's health percentage, and specified a 12-limb set by name: head, neck, left/right arm, left/right finger, body, waist, right/left leg, left/right foot -- replacing Session 14's placeholder 6-limb set (head/torso/two arms/two legs).

- **12-limb expansion**: `limb_t` in `being.h` grew from 6 to 12 entries in the user's exact order (`LIMB_HEAD, LIMB_NECK, LIMB_LEFT_ARM, LIMB_RIGHT_ARM, LIMB_LEFT_FINGER, LIMB_RIGHT_FINGER, LIMB_BODY, LIMB_WAIST, LIMB_RIGHT_LEG, LIMB_LEFT_LEG, LIMB_LEFT_FOOT, LIMB_RIGHT_FOOT`), `LIMB_TORSO` renamed `LIMB_BODY` to match the user's wording. `being.c`'s `LIMB_NAMES[]` updated to match, same order. Confirmed via `grep` before touching anything that no code anywhere referenced the old individual enum constants by name outside their own definitions (`combat.c`/`cmd_score.c` only ever iterate generically via `LIMB_COUNT` and `limb_name()`), so the expansion was a clean, contained change -- no logic needed touching in either file, they automatically picked up 12 limbs instead of 6. `being_limbs_full_heal()`'s even split (`progress.max_hp / LIMB_COUNT`) now yields ~2 HP per limb on a fresh mortal (was ~4 with 6 limbs) -- limbs are even more fragile now, a single ordinary hit (1-6 damage) destroys one outright almost every time.
- **New `limbs` command**: `src/cmd/cmd_limbs.c`, registered in `cmd_table.c`. Unlike `score`'s `Limbs:` section (Session 15, which only lists a limb once it's hurt), `limbs` always shows the full `LIMB_COUNT` unconditionally -- every limb's percentage every time, with the same `limb_status_text()` injury suffix appended when applicable (e.g. `right leg       0%  -- is destroyed and needs medical attention`). Reuses `being_limb_pct()`/`limb_status_text()` as-is, no new being.c logic needed.
- **Testing**: `tests/smoke_test_limbs.py`'s hardcoded 6-name list updated to the new 12; new `tests/smoke_test_limbs_cmd.py` confirms a fresh character's `limbs` output lists all 12 at 100% with zero injury phrases, and that after combat damage, `limbs` still lists exactly 12 lines (never a partial set) with the injured one(s) flagged alongside untouched limbs still at 100%.
- Rebuilt clean (zero warnings), restarted `tobin_c`, ran `smoke_test_limbs_cmd.py` plus the full 14-file existing regression suite (15 total) — all pass.
- Next: with 12 limbs now closely matching the original's real slot list (see the "Limbs" decision row above), the natural next step if equipment ever lands is wiring these same slots to `wearSlotT`-equivalent gear placement, rather than inventing a separate slot enum later.

### Session 18 — 2026-07-02 — `help` + `wizhelp` commands
User asked for `help` (all available commands) and `wizhelp` (immortal-only commands). Researched the original first, same as every command this project has built -- and the two turned out to be architecturally very different from each other in the original, which shaped the design (see the "`help`/`wizhelp`" decision row above for the full breakdown).

- `cmd_entry_t` (previously private to `cmd_table.c`) moved to `cmd_internal.h` and gained `help` (one-line description) and `min_level` fields; new `cmd_table_entries(int *count)` accessor lets `cmd_help.c` read `cmd_table.c`'s `COMMANDS[]` without duplicating it or exposing it as mutable global state.
- New `src/cmd/cmd_help.c`: `cmd_help()` lists every registered command with its one-liner, plus a hardcoded `quit!` line (that command is deliberately excluded from `COMMANDS[]` itself, so it isn't reachable by abbreviation -- see Session 9 -- and therefore wouldn't show up automatically). `cmd_wizhelp()` rejects non-immortal callers outright, otherwise lists any command with `min_level > MORTAL_LEVEL_MAX` -- a genuine, direct port of the original's real `doWizhelp()` mechanism (confirmed via source research: a `commandArray[]` scan filtered by `minLevel`, not a file lookup, unlike `help`). Currently prints `"(none yet -- no commands are currently immortal-only)"` since nothing in Tobin is min_level-gated yet -- an honest empty state, not a broken or silently-omitted one.
- Registered both in `cmd_table.c`'s `COMMANDS[]` with `min_level = MORTAL_LEVEL_MIN` (like every other current command) -- `min_level` exists as metadata ahead of the first command that will actually need immortal-gating (a future `promote`, for instance), not enforced by `cmd_dispatch()` yet.
- **Testing**: new `tests/smoke_test_help.py` -- confirms `help` lists all 11 commands (including the hardcoded `quit!`); confirms a mortal calling `wizhelp` gets rejected with a clear message, not silence; hand-promotes a test character to level 51 (same DB pattern as every other immortal-only test) and confirms `wizhelp` then shows the immortal-only header and the honest "none yet" line.
- One process note: kicked off a background regression run, then rebuilt/restarted the server mid-run for this session's own changes -- the restart correctly killed the in-flight run's last two tests with "Connection refused" (not a real bug, just bad timing on my part). Confirmed by re-running both individually against the final build before the full suite re-run below.
- Rebuilt clean (zero warnings), restarted `tobin_c`, ran `smoke_test_help.py` plus the full 15-file existing regression suite (16 total) — all pass.
- Next: the original's real `commandInfo::minLevel` dispatch-time enforcement isn't replicated (Tobin's `min_level` is display-only) -- worth adding a generic enforcement check in `cmd_dispatch()` once a real immortal-only command exists to test it against, rather than building it speculatively now.

### Session 19 — 2026-07-02 — 13th limb: genitalia
User pointed out the Session 17 limb list was missing genitalia.

- Added `LIMB_GENITALIA` to `limb_t` (`being.h`), inserted between `LIMB_WAIST` and `LIMB_RIGHT_LEG` (anatomically adjacent to the waist) -- `LIMB_COUNT` is now 13. `being.c`'s `LIMB_NAMES[]` updated to match, same position. Checked the original's real slot list (`wearSlotT`, `misc/limbs.h`) first, same as every limb-related change so far -- confirmed it has no genitalia-equivalent slot, so unlike the rest of the 12-limb set (a close match to the original), this one is a genuine Tobin-specific addition, not a port.
- No other code changes needed -- same as the 6→12 expansion in Session 17, nothing references individual limb enum constants outside their own definition, so `combat.c`/`cmd_score.c`/`cmd_limbs.c` all picked up the 13th limb automatically via their existing `LIMB_COUNT`-driven loops.
- Updated `tests/smoke_test_limbs.py` and `tests/smoke_test_limbs_cmd.py`'s hardcoded limb-name lists and the `== 12` exact-count assertion (now `== 13`).
- Rebuilt clean (zero warnings), restarted `tobin_c`, ran the full 16-file regression suite — all pass.
- Next: same as Session 17/18's "Next" items.

### Session 20 — 2026-07-02 — Home dev environment (VirtualBox VM), private git repo, IAC SB resumable parser fix
First session run from the user's home machine (sessions now declare "at work" vs "at home" to pick the environment — see Quick orientation above). Mostly infrastructure, plus one real code fix.

- **Home VM stood up end-to-end**: VirtualBox VM "NUDServer" (Fedora 44) on the user's Windows machine. Fixed its unreachability (guest had a static LAN IP while the VM was on NAT — switched the VM to bridged networking over the host's Wi-Fi, live via `VBoxManage controlvm`), set up key-based SSH (user `mud`), verified all build packages present, copied the tree to `~/NewMUD/`, seeded the DBs (19,209 rooms, same as work), built clean (zero warnings), and ran the full suite as a baseline: 16/17 pass, with `smoke_test_color.py` failing exactly as documented for any fresh seed (needs hand-staged `<X>` content — reconfirmed, still an open item).
- **VM performance work**: diagnosed dnf updates crawling at ~60 KB/s while the host got 3.6 MB/s — root cause was VirtualBox's default e1000 (`82540EM`) NIC emulation. Switched to **virtio-net** (needs poweroff): ~9.5 MB/s after, ~150x. Also bumped the VM to 12 GB RAM / 4 CPUs while it was down, and ran a full `dnf upgrade` (647 packages, new kernel 7.0.14; Guest Additions survived). `max_parallel_downloads=5` set in dnf.conf. MariaDB/sshd/server all verified healthy after reboot.
- **Private git repo**: `github.com/sculpy/tobin-mud` (GitHub, private), repo root at the top of the whole tree. `.gitattributes` forces LF on `.sh`/`.sql`/`.py`; `core.autocrlf=false` repo-local. This replaces manual scp as the home↔work sync mechanism. First commits: full tree (3,741 files), TODO update, then this session's fix.
- **`TODO.md` added** (`c_port/TODO.md`): running checklist complementing STATUS.md — STATUS records what happened, TODO tracks what's next.
- **IAC SB split-across-reads fix** (the real code change): `drain_lines()` lost its place if an `IAC SB ... IAC SE` subnegotiation arrived split across two `read()`s — the "inside SB" state wasn't preserved, so the tail of the subnegotiation leaked into the player's line buffer as typed garbage. Plain telnet never triggers it; Mudlet/MUSHclient-class clients proactively send TTYPE/NAWS subnegotiations and would have. Fix: `bool in_subneg` + `unsigned char subneg_prev` persisted on `descriptor_t` (calloc'd, so zero-init), with the resume check at the top of the parse loop; scan semantics for complete sequences unchanged. New `tests/smoke_test_telnet_iac.py` (6 checks): whole NAWS subnegotiation, TTYPE subnegotiation deliberately split mid-payload across two sends with a 0.4s gap, and split WILL/DO + lone-IAC regression guards. Rebuilt clean, new test passes, full 18-file suite re-run: 17 pass + the one documented color expected-fail.
- **Interactive client pass happened** (same session, continued into 2026-07-03): the user connected with a real client, created account/characters, relogged, and play-tested. First human findings:
  - `<n>` reported as "not releasing color" in a tagged `say` — investigated against the original: **`<n>` was never a color reset** (it's a name-substitution tag in `sys/colorstring.cc`); the reset is `<z>`/`<Z>`/`<1>`. Tobin's literal pass-through of `<n>` is faithful. User educated, no code change for that.
  - But the report exposed **color bleed**: a message that sets a color and never resets leaks it into the prompt and all subsequent output. Fixed (deliberate deviation, same spirit as the password-salt hardening): `colorstring_translate()` now tracks the last emitted code and appends an ANSI reset to any message that ends still-colored. Explicit `<z>` unaffected.
  - The `say`-carries-tags observation made `smoke_test_color.py` **self-contained** (rewritten to inject tags via say, 10 checks incl. auto-reset + no-bleed-into-next-message): the suite is now **fully green, 18/18**, with zero expected-fails, for the first time.
  - Verified live by the user in their client ("it works as you've said"), plus DB-promotion → relog → level-reload confirmed working interactively (Jesus → level 60 Implementor on the home VM, same upsert as Session 11).
- **Three more interactive-pass findings, all fixed same session** (2026-07-03):
  - **Account menu now shows each character's level** — `1. Jesus (Implementor)` / `2. Testdummy (Level 1)`, same title-vs-number convention as `who`. `player_list_by_account()` gained a parallel `levels[]` out-param (LEFT JOIN on `player_progress`, COALESCE to 1).
  - **Combat target abbreviation** — `kill clau` reaches Claudius. `combat_find_room_target()` does exact-match-first then prefix (`strncasecmp`), so a player literally named "Clau" is never shadowed by "Claudius". Same convention as Session 9's verb abbreviation and the original's `is_abbrev()` targeting. New `tests/smoke_test_target_abbrev.py` covers the exact-beats-prefix worst case (one name a prefix of another).
  - **Character name validation** — new names must be 3-15 letters, `isalpha` only (rejection re-prompts). Direct port of the original's `_parse_name_safe()` rule (`misc/parse.cc`: min 3, max 15, isalpha loop); its illegal-name/mob-name blocklists were NOT ported (no such lists in Tobin). Pre-existing names with digits stay playable — only creation is gated. **Test-suite blast radius**: every test generated unique names with numeric suffixes; all 20 files converted to a base-26 alphabetic suffix, over-long tags shortened (`CombatImmortal`→`CombatImm` etc: 15-char cap), digit-bearing fixture names renamed, and 6 new validation checks added to `smoke_test_name_case.py`.
  - Full suite after all three: **20/20 green**.
- Also this session: player Claudius (bot-driven from the host) fought the user's characters live — first human-vs-scripted PvP; limb destruction, miss-rate degradation from destroyed limbs, and defeat-to-menu all eyeballed by an actual player in a real client.
- **Phase 2 direction chosen (user)**: immortal/builder tools — in-game room/object/mob editing, zone management, object persistence. Sequenced roadmap in TODO.md (A: immortal command infra → B: room editing → C: objects → D: mobs → E: zone resets).
- **Phase 2A shipped same session**: `min_level` now ENFORCED in `cmd_dispatch()` (an over-level command is skipped during matching — invisible to mortals, exactly like the original's `commandInfo::minLevel` gate, and never abbreviation-matched); `promote <name> [level]` (`cmd_promote.c`, backed by new cross-account `player_set_level_by_name()` — the deliberate exception to player_repo's account-scoping rule; live-applies to online targets including their command access, told who did it; can't exceed the promoter's own level; exact names only) — the manual-SQL promotion workflow is dead; `goto <vnum>` (`cmd_goto.c`, enter_world's lazy room-load pattern); `help` lists only caller-usable commands; `wizhelp` shows each immortal command's minimum level. New `tests/smoke_test_immortal_cmds.py` (18 checks); `smoke_test_help.py` updated (real immortal commands exist now).
- **Phase 2A2 — DB-backed help topics + in-game editor (user idea, same session)**: new `db/sneezy/help_topic.sql` (name PK / body / updated_by / updated_at; seeded with topics for all 14 current commands, re-runnable without clobbering in-game edits); `help <topic>` (`cmd_help.c`) shows the body, exact-then-prefix, with an anti-leak check (a topic whose name matches an over-level command reads as "no help" to a mortal); `hedit <topic>` (`cmd_hedit.c`, **level 56+ gate, user-specified**) starts a classic DikuMUD-style line editor handled in descriptor.c's CONN_PLAYING case (`d->editing_help`: existing text preloaded and shown, lines append, `.` saves via new `help_repo.c`, `~` aborts; mid-edit defeat/quit discards). First in-game content editor — deliberately the same pattern Phase B's room editor will use. New `tests/smoke_test_help_topics.py`.
- **`copyover` — hot reboot without dropping connections (user idea, same session)**: Erwin Andreasen's classic Diku copyover/hotboot, **not a port** (verified: the original SneezyMUD has none). `cmd_copyover.c` (gated at **level 59+, Administrator**): stops all fighting and wait-states, persists every playing character's progress, writes `copyover.dat` (listen fd + per-connection fd/account/character/room/color), announces, and `execl("/proc/self/exe", ..., "--copyover", ...)` — player sockets survive the exec (no CLOEXEC on them); login/menu-state connections get a "please reconnect" note and ARE closed by the exec via deliberately-set FD_CLOEXEC (nothing destroyed by hand, so a failed exec just clears the flag and carries on). `main.c` takes `--copyover <file>`; `game_loop.c`'s `copyover_recover()` adopts the listening socket and rebuilds each descriptor via `descriptor_copyover_adopt()` (reload account + character from DB, back to the room they were STANDING in — not load_room — no telnet re-negotiation needed). The user's "lock out commands" requirement is free: single-threaded, the whole copyover runs inside one command execution. **Deploy workflow upgrade**: rebuilds now ship via in-game `copyover` instead of cold restarts — nobody gets disconnected for a deploy again. `tests/smoke_test_copyover.py` (13 checks, including the actual exec survival, location preservation, fight-stays-stopped, login-conn drop, and new-connection acceptance).
- Next: Phase 2B — room editing (redit-style, DB-persisted, digging new rooms).

### Session 21 — 2026-07-03 — Movement commands, room builder (`edit`), copyover binary-path bug
Phase 2B: the room builder, with the user directing "take the interface from sneezy" — so `edit` is a port of the original's `TPerson::doEdit()` (misc/create_rooms.cc, "Original edit code from Silly, May 1992"), not an invented redit.

- **Movement first** (`src/cmd/cmd_move.c`): north/east/south/west/up/down — you can't build a world you can't walk. Directions are the original dirTypeT's first six slots IN ITS ORDER (N=0,E=1,S=2,W=3,U=4,D=5; `REV_DIR` ported from constants.cc's rev_dirs) — `DIR_NAMES`/`REV_DIR` now live in room.h/room.c, and room.h's old (wrong) N/S/E/W order comment is fixed. Movement sits at the TOP of COMMANDS[] like classic Diku so n/e/s/w/u/d always mean movement — **"s" is now south (say needs "sa"), "w" is west (who needs "wh")**; `smoke_test_parser_display.py` updated. Can't walk while fighting (original doMove rule); leave/arrive announced to both rooms; dangling exits read as "You can't go that way." `look` gained an "Obvious exits:" line.
- **`edit <field> <args>`** (`src/cmd/cmd_edit.c`, gate `BUILD_MIN_LEVEL` 56 — stands in for POWER_EDIT + per-builder room ranges): field names prefix-matched like the original's bisect_arg. Ported fields: `name <text>` ("New Room Title:"), `description` (drops into the shared line editor — the descriptor editor state was generalized from hedit-only to an `edit_kind` enum for this), `sector_type [n]` (numeric only — no TerrainInfo names yet), `exit <dir> <toroom>` with the original's two signature behaviors: a missing target room is auto-created as a "small duplicate" (sector copied, name "An unfinished room") and the reverse exit is auto-fixed ("Fixing opposite directions." / "Making new exit back into this room." / "...exits into incorrect room [N]."), `exit <dir> -1` deletes. Bare `edit` shows the room summary (name/number/sector/exits — the info block from update_room_menu, sans VT100). **Deliberate trims**: doors/locks/keys (the 7-arg exit form), flags, extra descriptions, river/teleport/height/capacity/spec, the VT100 CON_REDITING menu mode, and rsave/rload — **edits persist to MariaDB immediately** (the DB is the world; there is no zonefile step). New repo half: `room_repo_save`/`_save_exit`/`_delete_exit`/`_exists` (roomexit door columns written as none/zero).
- **Copyover binary-path bug, found by the new tests failing**: a rebuild replaces build/tobin_c, so the running process's /proc/self/exe points at the DELETED old inode — copyover was silently relaunching the OLD binary (and naming the process "exe", which also dodged `pkill -x tobin_c`; it held port 4000 and masqueraded until killed by pid). Fix: main() resolves argv[0] via realpath() into `tobin_binary_path()` at startup and copyover execs THAT PATH (argv[0] carries the full path across generations). One final cold restart was needed to ship the fix itself; rebuild-then-copyover genuinely works from here on (proven: `smoke_test_copyover.py` passes against a freshly rebuilt binary).
- New `tests/smoke_test_redit.py` (gate at 51 vs 56, summary, name/sector/description edits, exit auto-create + reverse-fix, walking the new link both ways with full names and single letters, wall rejection, exit deletion, and direct DB-row verification of persistence) — all editing happens in SQL-bootstrapped sandbox rooms at high vnums (900000+), never touching the seeded world. Help topics added for edit/movement/each direction.
- Morning note: `smoke_test_combat.py` flaked once on the freshly-booted VM (cold DB caches) and passed twice on rerun — recorded in TODO as a watch item.
- **Departure/death announcements (user request, same session)**: `quit!` echoes "%s has left the game." to the room (cmd_quit.c); a link-drop echoes "%s has lost their link." (descriptor_destroy); and a death broadcasts a random teasing taunt naming victim and killer **to every playing connection in the world** except the two involved (combat_defeat, 4-entry taunt pool). Movement's private room_echo was promoted to a shared `descriptor_room_echo()` for all of these. Tests: kill gains a bystander-receives-the-taunt check, quit gains a room-is-told check. **This batch was the first true hot deploy**: rebuilt, then shipped via in-game copyover with the binary-path fix — the new code demonstrably running without a restart.
- **Notification batch (user requests, same session)**: (1) movement announces "%s exits to the <north/east/south/west>." to the room ("exits upward/downward" for U/D, where the literal phrasing won't parse); (2) **the game loop is now the single prompt authority** — `descriptor_send()` marks `needs_prompt`, and the loop sends exactly one "> " per iteration to any playing, non-editing connection that received output, so unsolicited output (says, combat rounds, arrivals, broadcasts) always re-prompts; all per-site explicit prompts (command replies, enter_world, copyover adopt, editor save/abort) were REMOVED in favor of this — the editor's "] " continuation prompt is the one deliberate exception; (3) link-drops are logged AND the log line is repeated as "[LOG] ..." to every online immortal EXCEPT those mid-editor (screen protection — same idea as the original's vlogf-to-imms); (4) death taunts now arrive on an "[INFO]" channel prefix (cyan when color is on). New `tests/smoke_test_notify.py` (exit phrasing, prompt-after-unsolicited-output, [LOG] to immortal / not mortal / not editing-immortal); kill test asserts [INFO] + prompt. Shipped via hot copyover again.
- **Game log system (user request, same session)**: log.c now writes every line to `logs/<YYYY-MM-DD_HH-MM-SS>.game.log` (creation-time-named, Windows-safe filename, LOG_DIR under the server's cwd) in addition to the console; each copyover generation naturally opens its own file (`log_open()` runs in main()). New immortal command **`log` (level 59+, user-specified)**: bare/`log <n>` tails the current file (bounded 32KB read, ring buffer), `log search <text>` case-insensitive substring search showing the most recent 20 matches, `log rotate` starts a fresh file, `log list` shows the directory with the current file marked. `tests/smoke_test_logs.py` (gate at 58 vs 59, tail, search finds a caused link-drop line, rotate empties the current-file view, list). One self-inflicted build break en route (rewrote log.c and dropped log_info/log_error) gave the copyover exec-failure path an unplanned live test: with the binary missing after the failed link, the running server caught the failed exec and carried on with everyone connected — exactly as designed.
- **Process note**: two one-off test flakes (combat, limbs_cmd) both trace to sweeps running CONCURRENTLY with a copyover deploy — the 5-second freeze starves their recv windows. Rule going forward: never hot-deploy while a regression sweep is running.
- **Polish batch (user requests)**: room builder renamed `edit` → **`redit`** (fits the user-defined `*edit` family — redit/hedit now, oedit/zedit/medit/pedit/aedit planned, all distinct first letters — see TODO.md; help topic migrated, stale seed topic deleted); `help`/`wizhelp` now sort **highest min_level first** so privileged commands lead the list; `logs/` explicitly gitignored; root **CLAUDE.md** added so sessions opening at the repo root orient instantly (build/run/test, copyover deploy rule, house rules).
- **Tier 3 batch (user's Batch-2 roadmap, small items — all shipped 2026-07-03)**: (1) `include/damage.h` — the original's 58 damage-type constants ported verbatim as an enum, unused-for-now future expansion; (2) log gates split — `log` tail/search/list now 54+, `log rotate` isolated to 59+ (in-command check, honest "requires level 59" message); (3) `promote` raised to 58+ (PROMOTE_MIN_LEVEL); (4) new **`exits`** command (classic autoexit list with each destination's name, lazy room loads); (5) **colorized say** — `<c>You say, "<z>message<z>"` framing cyan, message as typed, closing-quote-uncolored invariant preserved; (6) **wizhelp hidden from mortals** (min_level 51 — "players only see help for what they can use"); (7) help topics updated in the same change (new `exits` topic; migrated log/promote/say topics) per the new CLAUDE.md upkeep rule. Gate ladder is now: 51 goto/wizhelp, 54 log read, 56 hedit/redit, 58 promote, 59 copyover/log-rotate, 60 (future gameedit). Tests updated across logs/immortal_cmds/help/say/color/redit; one test-side fix during verification (the say assertions had glued the closing quote to the message — the deliberate `<z>` reset sits between them).
- **loadroom + redit gate (user requests, same day)**: new `loadroom [vnum]` (51+) — an immortal sets their own login room (`player.load_room`, new `player_set_load_room()`), verified by relog-into-The-Void; `redit` dropped 56+ → **51+** (every immortal builds; hedit stays 56+). Help topics updated/migrated in the same change.
- **Batch 3 (user-specified, all 10 items shipped 2026-07-03 evening)**: TobinMUD ASCII banner + derivative tagline on connect; duplicate character names rejected cross-account at creation; attack/kill full aliases (immortals instakill on both); help alias resolution (nw/ne/se/sw/' land on canonical topics; alias rows unlisted via NULL help); IP logging (peer address captured at accept, carried across copyover in the recovery file, logged on connect/enter/leave/link-drop/death — immortal-visible via the log gate); mortal/immortal at-will toggle (true rank parked in player_progress.true_level; effective mortality is total; immort gates on stored rank and answers real mortals with the unknown-command Huh — the one deliberate exception to command invisibility); handedness at creation (hand left/right, default right, player.handed; combat alternates hands, primary +1/off-hand -1); prompt hp (player.prompt_flags bitmask rendered by the loop prompter); lettered account menu (C/N/D/Q case-insensitive, bare c connects an only character, the ONE place bare q quits; legacy inputs kept so no test flows broke); named sectors + flags in the immortal room header and redit (original 61-entry sectorTypeT + 22 ROOM_* bits ported — born of the user meeting "[sector 60] [flags 408009]" and asking what it meant: dead woods, and ten flags). Also: db/sneezy/tobin_migrations.sql (idempotent ALTERs) established for schema evolution; **copyover recovery now closes unparseable inherited fds** — the leak made an orphaned client look like a hung MUD (diagnosed live on the user's own session after the wave-A format change).
- **Evening follow-ups (all user-specified, all shipped 2026-07-03)**: `users` (58+ connection roster: character/account/IP/state, mid-editor marked); account-creation password confirmation (type twice, mismatch re-prompts, scratch zeroed — every test creation flow across 28 files updated to type twice); `(connected)` marker in the account menu for characters already in the world; say framing finalized as `<c>You say, "<z>msg<c>"<z>` (closing quote cyan too); default load rooms split — mortals to Center Square (100), immortals to Imperia (1) when unset, explicit loadroom overrides. Suite is 30 files, fully green.
- Next: Phase 2C (objects) per TODO.md; Batch 2 Tier 1 systems (vitality, terrain, typed logs, news) queued; consider doors and the remaining edit fields when they become real.

### Session 22 — 2026-07-04 — `redit <vnum>`: edit any room from anywhere
User report: "redit 100 said huh?" — they expected `redit <vnum>` to open room 100 for editing without walking there. Root cause: the original's `doEdit` (and Tobin's port) only ever edits `ch->roomp` (the room you're standing in) — the first token was always parsed as a *field name*, so `redit 100` matched no field. (Standing below level 51 would also make `redit` invisible and yield the literal "Huh?!"; the fix covers both.)

- **`cmd_edit.c`**: an optional leading room number now targets that room. If `args` begins with a digit, it's read as a vnum, the room is loaded via the existing `get_or_load_room()` (the same plumbing `goto` uses — cached-or-DB-loaded, then registered in the world), and all subsequent editing operates on it; the builder is **not** moved. A missing vnum is reported ("There is no room N to edit.") and **not** created (creation stays with `redit exit`/future `dig`). Bare `redit`, or any `redit <field>` (field names never start with a digit), still edits the current room exactly as before — a pure superset, no behavior change to existing usage. Remote description edits persist correctly because the line editor already keys its save off `d->edit_room_vnum` (set to the target room's vnum), and `world_get_room()` finds the just-registered room.
- **Deliberate deviation from the original** (documented here per house rule): the original requires `goto`-then-`edit`; Tobin lets you skip the goto. Justified — `goto` already resolves rooms by vnum, so this reuses that path and is a strict builder convenience.
- Help/one-liner updated in the same change: `cmd_table.c` redit description, the in-game `redit` summary now hints "Prefix a room number to edit it from anywhere", and the `redit` help topic (seed INSERT + a new idempotent migration in `db/sneezy/help_topic.sql`) documents `redit [<vnum>]`.
- **Test**: `tests/smoke_test_redit.py` gains a Part 6 — from the sandbox origin, `redit <BASE+2> name` renames a remote room, a follow-up `look` proves the builder didn't move, `redit <BASE+2>` (no field) shows the remote room's summary, walking there confirms the rename landed, and `redit 999999998` on a nonexistent room reports it without creating it.

**`/clear` in the shared line editor (same session, user request):** the editor (`descriptor.c`'s `CONN_PLAYING` case) that both `redit description` and `hedit` drive preloaded the existing text and only ever *appended* — there was no way to wipe it and retype. Added a `/clear` line command (alongside `.`=save and `~`=abort) that empties `edit_buf`/`edit_len` (including the preloaded content) and keeps editing; nothing is written until `.` saves. Both entry-point prompts (`cmd_edit.c`, `cmd_hedit.c`) now advertise it, and the `redit`/`hedit` help topics were updated (seed + migration). `smoke_test_redit.py`'s description part gained a clear-then-retype check that confirms the old preloaded text is gone from the saved result. Benefits `hedit` for free since the editor is shared.
- **Built + deployed + verified** on the home Fedora VM (SSH, 192.168.254.200): clean zero-warning build, help_topic migrations applied, cold-restarted, and the full sweep run green (26 assertion suites pass; `smoke_test.py`/`smoke_test_login.py` are legacy no-assertion scripts that print `=== done ===`, not the `ALL CHECKS PASSED` sentinel — not failures).

**Tree cleanup (same session, user-directed):** the repo root held an old,
scattered copy of the upstream SneezyMUD (`code/`, `lib/`, `docs/`,
`cmake/`, `scripts/`, `db/`, plus the root `CMakeLists.txt`/`Makefile`/
`CMakePresets.json`/`README.md`/`LICENSE.txt`, all byte-identical to
upstream). The user dropped in a fresh upstream clone at `sneezymud-master/`,
so all of that redundant copy — plus the unrelated `talker.c` chat server —
was deleted. Tobin's own 4 schema files (`help_topic`, `player_attrs`,
`player_progress`, `tobin_migrations`) moved out of the deleted `db/sneezy/`
into **`c_port/db/sneezy/`** (with a new `c_port/db/README.md` and
`c_port/db/apply-tobin-schema.sh`). `c_port/` has no build/runtime
dependency on anything outside itself (its `db/sneezy/*.sql` source-comment
references resolve relative to `c_port/`), so the build is unaffected; only
the two-step seed workflow changed (see Quick orientation above). Root now
holds just `CLAUDE.md`, `c_port/`, and `sneezymud-master/`. Not yet pushed
to the Fedora box — on pull there, the box's tree restructures too, but its
already-seeded live DB is unaffected (only a re-seed uses the new workflow).

**Menu-driven redit (same session, user request + wireframe):** the whole
`*edit` family is being converted to menu-driven editors "like the character
creation menu", starting with `redit`. This replaces the one-shot command
form (`redit name X`, `redit exit ...`) entirely — `redit [<vnum>]` now drops
into a **`CONN_REDIT_*` connection-state machine** in `descriptor.c`,
structured exactly like character creation. User decisions (locked): (1)
**working-copy model** — edits mutate `d->redit_work` only; **(S)ave** applies
the copy to the live room + DB; **(Q)uit** warns on unsaved changes
(Save/Discard/Cancel); (2) **menu-only** — the command form is retired; (3)
**(C)lear room out** blanks name/desc/terrain/flags **and** exits (removing
neighbours' reverse exits), behind a yes/no confirm. Menu shape is the user's
wireframe: (R)oom name, room (F)lags submenu (toggle by bit), (T)errain
submenu (61 sectors), (D)escription (the shared line editor, now with
`/clear`), (E)xits submenu (pick a direction → target vnum; missing rooms
auto-create on save, reverse exits auto-fixed), (C)lear, (S)ave, (Q)uit.
- Implementation: new `CONN_REDIT_*` states + `room_t redit_work`/`redit_dirty`
  working-copy fields on `descriptor_t`; `descriptor_redit_begin()` (loads +
  copies the room, shows the menu); render helpers `show_redit_{menu,flags,
  terrain,exits}`; `redit_save`/`redit_apply_exits` (diffs working vs live
  exits, auto-creates targets, fixes/removes reverse exits)/`redit_clear`. The
  shared string editor was factored into `editor_feed()` (`.`/`~`/`/clear`/
  append), now used by both the redit description sub-state and `hedit`. New
  `room_flag_count()`/`room_flag_name()` accessors for the flag submenu.
  `cmd_edit.c` shrank to a thin launcher. The old `EDIT_ROOM_DESC` line-editor
  path in `CONN_PLAYING` is gone (room descriptions edit through the menu now).
- **Subsumes** this session's earlier `redit <vnum>` and `/clear` work — both
  live inside the menu now (`redit <vnum>` = open the menu for that room;
  `/clear` = the description editor's buffer wipe).
- Test: `smoke_test_redit.py` fully rewritten to drive the menu (31 checks) —
  gate, menu render, each field submenu, working-copy save with auto-create +
  reverse-fix, walking the new link, quit-discard leaving the DB untouched,
  and clear-room-out blanking the room + removing the neighbour's reverse
  exit. Green, plus the full sweep green. Help topic rewritten (seed +
  migration) to describe the menu.
- **Next for the family:** this `CONN_REDIT_*` shell is the template for
  `oedit`/`zedit`/`medit`/`pedit`/`aedit` (TODO.md's `*edit` family) — build
  each on the same working-copy + submenu pattern.

**redit → Sneezy menu format + capacity/height + doors (same session, from
the user's `create_rooms.cc` reference):** reformatted the redit menu to
Sneezy's `update_room_menu` style — a `Room Name / Number / Sector Type`
header then a **numbered** field menu (1 Name, 2 Description, 3 Flags, 4
Sector Type, 5 Exits, 6 Max Capacity, 7 Room Height) with `C/S/Q` actions
(replacing the earlier lettered menu). New room fields **Max Capacity** and
**Room Height** wired to the existing `room.capacity`/`room.height` columns
(added to `room_t`, `room_repo` load/save). **Doors on exits**: the Exits
submenu is now two-level — pick a direction → per-exit menu (Target vnum /
Door type / Conditions / Remove). Door type is a numbered choose-one from
`door_types[]` (None/Door/Trapdoor/Gate/.../Hatch, 11); conditions are a
numbered `[x]` toggle from `exit_bits[]` (Closed/Locked/Secret/.../Jammed,
11). Both persist to the `roomexit.type`/`condition_flag` columns (previously
written as zero); `room_repo_save_exit()` gained `door_type`/`condition`
params. New `door_type_name/count`, `exit_cond_name/count/names` accessors in
room.c. Movement/look still key off `exits[]` only (doors have no gameplay
effect yet — open/close/lock and movement-blocking are queued in TODO). One
bug caught by the test: `redit_save()` initially didn't copy capacity/height
from the working copy to the live room (saved as 0) — fixed.
- Test: `smoke_test_redit.py` rewritten for the numbered menu (31 checks) —
  every field 1–7, the two-level exit editor with door type + condition, DB
  verification of dest/type/condition + capacity/height, walking the link,
  quit-discard, and clear-room-out. Green; full sweep green. `redit` help
  topic updated (seed + migration) to the numbered layout.
- **Queued to TODO** (from the same user reference drop): redit Extra
  Descriptions + Room Spec; door *mechanics* (open/close/lock, movement
  blocking, `doorIntentT`/`doorUniqueT`, keys); Positions (`positionTypeT`);
  health strings (`prompt_mesg`); Classes (`classInfo`); body/limb flags;
  Limbs→`wearSlotT` (with an open genitalia/HOLD/EX question); herald colors.

**All-caps flags/sectors + `news` system (same session, user requests):**
- **Display**: room flags and sector types now render in ALL CAPS straight
  from the enum name tables (`SECTOR_NAMES`/`ROOM_FLAG_NAMES` in room.c
  uppercased; flags now match the upstream `room_bits[]` exactly). The redit
  "Sector Type:" line shows the name only (e.g. `TEMPERATE HILLS`), no number.
  `cmd_look`'s immortal header picks this up for free (same accessors). The
  selection submenus keep their numbers (those are pick indices, not names).
  New CLAUDE.md house rule records this.
- **News**: new DB-backed `news` command (everyone) — `news.sql` table
  (id/created_at/author/title/body, UNIQUE title for idempotent re-seed),
  `news_repo.c` (`news_repo_recent`, newest-first), `cmd_news.c`. **No dates
  or numbers are rendered** — per the user rule, news items carry no numbers
  at all; ordering conveys recency. Seeded with three player-facing entries
  (room builder, doors, the news command itself). Help topic added.
- **House rule (user-directed)**: every code change that affects a player's
  ability to play, changes a command, or adds a zone gets a `news` entry
  appended to `news.sql` in the same change, player-facing prose with NO
  numbers. Recorded in CLAUDE.md and enforced by `smoke_test_news.py` (which
  asserts the rendered news contains zero digits).
- Tests: `smoke_test_news.py` (new — reads news, newest-first order, the
  no-digits guard, help topic); `smoke_test_redit.py` updated for the
  all-caps sector/flag display. Both green; full sweep green.

**News follow-ups + bracketed flag/sector display (same session, user
requests):**
- **`news [lines]`** — the `news` command now takes an optional line count
  (news 10 / 20 / 50 / 100, default 20, cap 100); `cmd_news` renders up to
  ~40 items then trims the output to the requested line count. No count is
  echoed (news carries no numbers).
- **`addnews <headline>`** (level 56+, new `ADDNEWS_MIN_LEVEL`) — posts a news
  item in-game: the argument is the headline, the body is typed into the
  shared line editor (new `EDIT_NEWS` edit_kind, `d->news_title` scratch), and
  on save is inserted via new `news_repo_add()` (author = poster's name; the
  UNIQUE title means a duplicate headline fails cleanly). This is the
  `newsedit`-style tool from the roadmap, named `addnews` per the user.
- **Bracketed flag/sector display** (user spec): `room_flag_names()` now
  renders each flag in its own bracket — `[ ALWAYS-LIT ] [ INDOORS ]` — and
  the display sites wrap them: flags in purple `<p>..<z>`, sector in cyan
  `<c>[ NAME ]<z>`. Applied in both the redit menu header and `cmd_look`'s
  immortal header (the old `[flags: x x x]` form is gone). A `news.sql` entry
  was added for the news changes (house rule).
- Tests: `smoke_test_news.py` extended (line-count, addnews gate + post +
  read-back); `smoke_test_redit.py` updated for the `[ NAME ]` bracket
  format. Both green.

**Output pager (same session, user request "just news should give the whole
thing with pagination options"):** `news` now shows the WHOLE feed a page at
a time instead of truncating. New reusable pager on `descriptor_t`
(`page_buf`/`page_len`/`page_pos`/`page_size`) + `descriptor_page_start()` /
`descriptor_page_next()`: long output is buffered and released `page_size`
lines at a time with a `[ ENTER for more, Q to stop ]` prompt. While a page
is pending, `CONN_PLAYING` routes each input line to the pager (ENTER = next,
Q = stop) before any command/editor handling, and the game-loop prompter is
gated on `page_len == 0` so no stray `> ` appears mid-page. `news [n]` sets
the page size (default 20, clamp 5–100). The pager is generic -- `log`/`help`
can adopt it later. `smoke_test_news.py` updated to page through the feed
(and fully drain the pager between commands, or the next command would be
swallowed by a pending more-prompt).

**Positions (same session, TODO item — first of the "work off the TODO"
pass):** sit / stand / rest / sleep / wake. New `position_t` on `being_t`
(the original's `positionTypeT` ladder; players use the standing/sitting/
resting/sleeping rungs), default STANDING, **not persisted** (you wake up
standing on login). New `cmd_position.c` (5 handlers + room echoes, guarded
against changing position mid-fight); `position_name()` + `POSITION_NAMES[]`
in being.c. Gates: movement requires STANDING (`cmd_move.c`); `look` is
blocked while asleep (`cmd_look.c`); attacking auto-stands from sit/rest and
is refused while asleep (`cmd_attack.c`). `score` shows a Position line --
**"Fighting" is derived from the `fighting` pointer**, never stored, so
combat.c was untouched. Regen weights by position (`regen.c`: sleeping x3,
resting x2, sitting x1.5). Help topics (positions + each command) + a news
entry added. **Zero combat.c changes.** `smoke_test_positions.py` covers the
transitions, move/look gates, score display, and the fighting-derived
position + can't-sit-while-fighting (the last two poll `score`/`sit` with a
retry to ride out global-pulse combat-round timing; fighters are HP-boosted
via SQL so the fight can't end mid-check). Full sweep green.

**Health strings + a stale-test fix (same session, next TODO item):**
`being_health_word()` (being.c, from the original's `prompt_mesg[]`) maps HP%
to a word; `score`'s HP line now reads `HP: 25/25 (perfect)` ... down to
`(near death)`. Help/news updated; `smoke_test_positions.py` asserts
`(perfect)` at full HP. **Caught a latent issue while deploying:** the score
sheet had an *uncommitted, improved* two-column format in the working tree
(`Name:` header + `You are left handed.`) that the box hadn't been running --
my Positions deploy carried it over, breaking `smoke_test_name_case.py` and
`smoke_test_accounts.py`, which still asserted the old committed format
(`-- Name --` / `Handedness: left`). Those three assertions were updated to
the current format (the improvement is kept; the HP-parsing regexes in
combat/regen tests already stop at `%d/%d`, so the `(word)` suffix is safe).
This session's "work off the TODO" pass so far: TODO.md fully reorganized;
Positions and Health strings shipped.

**Held messages -- no game interruptions while editing (same session, user
request):** anyone in an editor (the redit menu / hedit / addnews) no longer
gets game messages pushed at them; they buffer and are reviewed on demand.
New per-descriptor held buffer (`held[64]` of `{when, text}` + `held_count`)
and `descriptor_notify()` -- "deliver async, or hold if the recipient is in an
editor" (`descriptor_in_editor()` = any `CONN_REDIT_*` state or `edit_kind !=
NONE`). **Every asynchronous send-site was routed through it**: room echoes
(say/movement/quit/link-drop), combat `tell()` (all hit/miss messages), the
death-taunt broadcast, the `[LOG]`-to-immortals link-drop notice, and the
attack notice. The death-taunt and `[LOG]` loops previously *skipped*
non-playing/editing people (message lost) -- now they deliver-or-hold so
nothing is dropped. New `catchup` command replays and clears the buffer; on
leaving an editor you're told "N messages arrived ... type catchup". A pulse
(`descriptor_held_expire`, registered every ~10s in main.c) drops anything
older than `HELD_MSG_TTL` (5 min). `smoke_test_held.py`: a builder in redit
gets a roommate's `say` held (not shown), is told on exit, and `catchup`
replays then empties. Help topic + news entry added.
- **Refined (same session, user follow-up):** the feature is **immortal-only**
  (mortals never open an editor) -- `catchup` is now gated at
  `IMMORTAL_LEVEL_MIN`. And **`[LOG]` lines are never held** -- the
  `[LOG]`-to-immortals broadcast reverted to send-or-skip (skips editing
  immortals) rather than hold, since logs are always available via the `log`
  command and would only bury the say/tell messages that matter in catchup.
  The "Edit in Peace" news entry was removed (immortal-only, not player news).
  `smoke_test_held.py` gained a link-drop that confirms the `[LOG]` line is
  NOT in catchup.
- **Infra:** the user added a crontab on the `mud` account that restarts
  `tobin_c` if it isn't running (closes the "start script survives reboots"
  TODO). Deploys stay `pkill; sleep 1; restart` so they beat any cron tick.

**`ed*` editor rename + wiznews channel (same session, user requests, logged
to TODO first per the new TODO-driven workflow [[workflow-todo-driven]]):**
- **Rename:** the editor command convention flipped from `*edit` to
  **`ed<noun>`** -- `redit`→`edroom`, `hedit`→`edhelp`, `addnews`→`ednews`
  (command-table names, usage strings, the `[edroom]` menu prompt, all help
  topics, and every smoke test's command sends). Help topics renamed via seed
  + a DELETE migration for the old `redit`/`hedit`/`addnews` seed rows.
  Internal C identifiers (`show_redit_menu`, `redit_work`, `EDIT_ROOM_DESC`,
  the `cmd_edit`/`cmd_hedit`/`cmd_addnews` function names, `smoke_test_redit.py`
  filename) were left as-is -- only the user-facing command names changed.
- **wiznews:** a level-51+ immortal news channel, read exactly like `news`
  (paged, newest-first) but from a separate `wiznews` table (`wiznews.sql`),
  posted with **`edwiznews`** (56+). `news_repo_recent`/`news_repo_add` gained
  a `bool wiz` selecting the `wiznews` vs `news` table (two literal queries to
  stay warning-free); new `EDIT_WIZNEWS` edit_kind. Immortal news stays out of
  the public feed. `smoke_test_wiznews.py` covers the gate, read, post, and
  channel separation. Future editors follow the convention:
  `edobject`/`edmob`/`edzone`/`edplayer`/`edaccount`.
- **Workflow note:** the user established that their commands go into TODO.md
  first, then I work the list autonomously ([[workflow-todo-driven]] memory).

**Socials (same session, next TODO item):** 15 emotes (smile, grin, laugh,
nod, shake, wave, bow, wink, grovel, shrug, cheer, cackle, poke, comfort,
thank) in a compiled table (`socials.c`), checked in `cmd_dispatch()` **after**
the command table (classic Diku ordering, so a real command always wins).
Each has an untargeted form (self + room) and a targeted form (`smile <name>`
-> self/target/room), the target found by case-insensitive prefix among room
PCs. Room echoes use `descriptor_room_echo`/`descriptor_notify` so they're
held for anyone editing. New `socials` command lists the verbs. Help topic +
news entry; `smoke_test_socials.py` (two mortals in Center Square) covers
untargeted, targeted, the list, and the absent-target rejection. Full sweep
green.

**help/wizhelp 3-column + prompt newline (same session, user batch):**
`help` and `wizhelp` now list command names **alphabetically in three
columns** via a shared `send_columns()` (qsort) in cmd_help.c -- names only,
`help <cmd>` for details. `wizhelp` dropped the `[NN+]` level tag (user
request) and keeps its usable-only filter (a level-51 sees only their seven
51-level commands, confirmed against the user's example). The game-loop prompt
gained a leading blank line (`\r\n\r\n> ` / `\r\n\r\nHP: %d > `). Remaining in
the user's evening batch: keepalive (anti-idle), `wiznet` (immortal channel),
`system` (global echo), and socials-to-DB + full Sneezy set + `edsocial`.

**wiznet + system + keepalive (same session, user batch):** `wiznet <msg>`
(`cmd_wiznet.c`, 51+) broadcasts to all online immortals (a private staff
channel, held for editors); `system <msg>` (`cmd_system.c`, 51+) echoes a bare
atmosphere line to everyone (sender sees it prefixed `system <msg>`). Both via
`descriptor_notify` so editors get them held. Keepalive: `descriptor_keepalive`
pulse (main.c, ~30s) sends an IAC NOP to every connection so idle players
aren't dropped by NAT/router timeouts (verified live -- NOP received; not in
the sweep since a 30s timer would slow it). `smoke_test_wizcomm.py` covers
wiznet/system (reach, gates, sender-vs-recipient). **6 of the 7 evening-batch
items done; only socials-to-DB + full Sneezy set + `edsocial` remains** -- a
bigger feature (move the compiled social table to DB, port
`sneezymud-master/lib/actions`, add the 55+ menu-driven `edsocial`).

**Late user batch 2026-07-05 (partial):** `[wiznet]` tag removed (wiznet shows
`<Name>: <msg>` in purple); **`mudstats`** (`cmd_mudstats.c`) reports
room/mob/obj counts from the DB; **idle flag** -- `descriptor.last_active` set
on every input line (handle_line) + on create/copyover-adopt, and `who`
appends ` (idle)` when now-last_active > 300s, cleared by any command;
**character-delete logging** added (quit + link-drop were already logged;
account-delete will come with `wipe`). Tests: `smoke_test_mudstats.py`,
`smoke_test_idle.py`, updated `smoke_test_wizcomm.py`. **Still queued:** daily
log files + 21-day retention, `wipe` (59+, password-gated, lower-level targets
only), and the objects-blocked holdable-items + `point` social.

**Daily logs + `;` shorthand (same session):** logs are now one
`<YYYY-MM-DD>.log` per calendar day (log.c), appended across every
reboot/copyover/rotate instead of a new file each boot; `log_prune_old()`
drops any `*.log` not modified in 21 days at each open; `log rotate` (59+)
re-opens the day's file rather than starting a new one (`cmd_log.c` message +
`smoke_test_logs.py` updated; also cleaned ~90 old-format log files off the
box during the transition). `;` is a one-character shorthand for `wiznet`
(cmd_dispatch special-case like `'`→say), immortal-only. **Remaining user
items:** `alias` (account-scoped, tier-aware -- DB table + expansion in
dispatch), `wipe` (59+, password + lower-level guards), and objects-blocked
holdable-items + `point`.

**More user items 2026-07-05:** immortal **color tiers** in who/score
(`being_rank_color()`: 51-53 `<c>`, 54-56 `<C>`, 57-58 `<p>`, 59+ `<P>`);
**`goto <player>`** teleports to an online being's room (not just a vnum);
**`help edit`** is a live index of the `ed*` editors; **multiplay** control --
`multiplay <on|off>` (59+, persisted in a new `game_config` table), enter_world
refuses a mortal account's second connected character when off, immortals
exempt (`multiplay.c`/`multiplay.h`, loaded at boot in main.c); the **`point`**
social (no-arg "point around randomly", held-item form deferred to objects);
and a **`help colors`** topic listing every `<x>` color tag. Tests:
`smoke_test_immmisc.py` (goto-player, help-edit, rank color),
`smoke_test_multiplay.py`. A large **night batch** of ~20 more requests was
logged to TODO.md (buildable: titles/who-args, gender+pronouns, appearance,
color-at-creation, rules/edrules, bug/delbug, newbie channel; objects-blocked:
money/commodities, liquids, fill, switch, examine; classes-blocked: druid).
- Next: same queue as Session 21 (Phase 2C objects; Batch 2 Tier 1 systems).

### Session 9 — 2026-07-02 — Abbreviation parser, CRLF fix, command prompt, quit! hardening
Four related changes from user feedback in one pass:
- **Abbreviation-based command parsing**: `cmd_table.c`'s `cmd_dispatch()` now matches the typed verb as a PREFIX of each registered command name (`strncmp`), not an exact match -- "sc"/"sco"/"score" all reach `cmd_score`, same for "l"/"look". Removed the old hand-listed alias rows (`"l"`, `"sc"`) since prefix matching supersedes them.
- **CRLF fix**: discovered (via the earlier raw-byte inspection technique) that room descriptions -- DB-sourced from the original SneezyMUD dump -- use bare `\n` internally, not `\r\n`, which doesn't reset a real telnet client's cursor properly. Fixed centrally in `descriptor_send()` (the single choke-point for all outbound writes): normalizes every bare `\n` to `\r\n` before writing to the socket, so this is fixed everywhere at once, not just for room descriptions.
- **Trailing prompt**: `descriptor.c`'s `CONN_PLAYING` case now sends `"> "` after every command's reply, but only if the command didn't transition away from `CONN_PLAYING` (avoids double-prompting after `quit!`, which already shows the account menu's own prompt). `enter_world()`'s auto-`look` on login/character-select also got the same trailing prompt.
- **`quit!` hardening**: `quit` was pulled out of the `cmd_table.c` COMMANDS array entirely so it can never be reached via abbreviation matching; `cmd_dispatch()` now special-cases the exact literal `"quit!"` before the abbreviation loop runs. The account-menu and both character-creation `quit` checks (in `descriptor.c`, handled directly rather than through `cmd_dispatch`) were changed to the same exact `"quit!"` requirement, for consistency across every place quitting is possible. All user-facing prompts/help text updated to say `quit!`.
- Updated all three existing quit-related tests (`smoke_test_quit.py`, `smoke_test_quit_menu.py`, `smoke_test_quit_creation.py`) to use `quit!` and added negative checks confirming the bare word `quit` does nothing at every one of those checkpoints. New `tests/smoke_test_parser_display.py` covers abbreviation matching (`l`, `w`, `sc`, `sco`), confirms `qu` does NOT reach quit, confirms every `CONN_PLAYING` reply (including unknown commands) ends with a prompt, and -- critically -- inspects the raw response bytes of a `look` command to confirm there is no bare `\n` anywhere in the output (not just a substring check).
- Rebuilt clean (zero warnings), ran the full seven-test suite (`smoke_test.py`, `smoke_test_accounts.py`, `smoke_test_quit.py`, `smoke_test_quit_menu.py`, `smoke_test_trade_attrs.py`, `smoke_test_quit_creation.py`, `smoke_test_parser_display.py`) — all pass.
- Next: same as before (real interactive `telnet` pass, deferred login-flow polish, Phase 2 of the roadmap).

### Session 8 — 2026-07-02 — quit-to-menu during character creation
- `descriptor.c`: both `CONN_CHAR_CREATE_NAME` and `CONN_CHAR_CREATE_ATTRS` now accept `quit`, which discards whatever's been entered so far (name, and/or any attribute allocation) and returns to `CONN_ACCOUNT_MENU` without touching the DB -- nothing is persisted until `done` actually runs `player_create()`. Connection stays open throughout, same as the existing playing-state `quit`.
- Updated the name-entry prompt ("New character name (or 'quit' to cancel): ") and the attribute screen's command list/usage-error text to mention `quit`.
- New `tests/smoke_test_quit_creation.py`: cancels at the name-entry step, cancels immediately at a re-shown name prompt, cancels *after* allocating points (confirming the allocation is discarded, not partially saved), and finally completes a real character creation afterward to confirm the flow still works normally post-cancellation. All confirm the account menu's character list stays empty ("none yet") until a creation is actually finished.
- Rebuilt clean (zero warnings), ran the full six-test suite (`smoke_test.py`, `smoke_test_accounts.py`, `smoke_test_quit.py`, `smoke_test_quit_menu.py`, `smoke_test_trade_attrs.py`, `smoke_test_quit_creation.py`) — all pass.
- Next: same as before (real interactive `telnet` pass, deferred login-flow polish, Phase 2 of the roadmap).

### Session 7 — 2026-07-02 — Point-buy pool lowered to 30
- User feedback: the 120-point net pool felt too generous. Changed `ATTR_POOL` in `being.h` from 120 to 30 -- one line. `ATTR_DELTA_CAP` (still 30) is now equal to the pool, so a single attribute maxed out at +30 exactly exhausts the whole pool by itself (previously it took four such allocations to exhaust the 120-point pool).
- Updated two existing tests that hardcoded pool-exhaustion scenarios sized for the old 120-point pool: `smoke_test_trade_attrs.py` (rewrote the exhaustion sequence: one +30 allocation now exhausts the pool instead of four, added a step showing a second attribute becomes affordable again only after a compensating negative trade) and `smoke_test_accounts.py` (its creation walkthrough allocated four attributes at +30 each; now just one).
- Rebuilt clean (zero warnings), ran the full five-test suite (`smoke_test.py`, `smoke_test_accounts.py`, `smoke_test_quit.py`, `smoke_test_quit_menu.py`, `smoke_test_trade_attrs.py`) — all pass.
- Next: same as before (real interactive `telnet` pass, deferred login-flow polish, Phase 2 of the roadmap).

### Session 6 — 2026-07-01 — Two-tier quit: character -> account menu -> disconnect
- User feedback: `quit` while playing was disconnecting outright; wanted it to instead return to the account menu (like most MUDs' "quit to character select"), with a *separate* `quit` at the account menu to actually leave the game.
- New `void descriptor_leave_to_menu(descriptor_t *d)` (declared in `descriptor.h`, implemented in `descriptor.c`): frees the current character (`being_destroy`, which also removes it from its room), clears `d->character`, sets state back to `CONN_ACCOUNT_MENU`, and redisplays the menu. Does **not** touch the socket.
- `cmd_quit.c` (only reachable from `CONN_PLAYING` via `cmd_dispatch`) now calls `descriptor_leave_to_menu()` and returns `true` (stay connected) instead of `false`. Message changed from "Goodbye, X!" to "You leave X and return to the character menu."
- `CONN_ACCOUNT_MENU`'s line handling in `descriptor.c` (which parses `new`/`delete <name>`/a numeric choice directly, not through `cmd_dispatch`) gained its own `quit` check: sends "Goodbye!" and returns `false`, actually closing the connection. Menu prompt text and the "Huh?" fallback both updated to mention it.
- Side effect worth knowing (not a bug): quitting to the menu also drops you out of `who` immediately, even though the connection is still open -- `who` only lists descriptors in `CONN_PLAYING` with a character, and leaving to the menu clears both. You're "at the menu," not "in the world."
- Fixed `tests/smoke_test_quit.py`, which assumed a single `quit` disconnects (no longer true) -- rewrote it to test the `who`-visibility angle of the two-tier behavior (quitter vanishes from `who` after the *first* quit, while the connection is still alive). New `tests/smoke_test_quit_menu.py` covers the connection-state angle directly: quit-while-playing doesn't disconnect, the same character can be re-entered from the menu afterward, and quit-from-menu does disconnect (confirmed via EOF, not just silence).
- Rebuilt clean (zero warnings), ran the full five-test suite (`smoke_test.py`, `smoke_test_accounts.py`, `smoke_test_quit.py`, `smoke_test_quit_menu.py`, `smoke_test_trade_attrs.py`) — all pass.
- Next: same as before (real interactive `telnet` pass, deferred login-flow polish, Phase 2 of the roadmap).

### Session 5 — 2026-07-01 — Leading blank line before look/who/score output
- `cmd_look.c`, `cmd_who.c`, `cmd_score.c` all started their output with the room name / "-- Who's online --" / "-- <name> --" directly, no leading `\r\n` -- so the response ran together visually right after the echoed command line, with no blank-line separation. Every other multi-line screen in the port (account menu, attribute point-buy screen) already started with `\r\n`; these three were the inconsistent ones. Added a leading `\r\n` to all three.
- Fixed a resulting double-blank-line bug: `enter_world()` in `descriptor.c` sent `"Welcome, %s!\r\n\r\n"` (trailing blank line) immediately before auto-dispatching `look`, which now *also* adds its own leading blank line -- would have produced two blank lines between the welcome message and the room. Trimmed the welcome message to a single trailing `\r\n`.
- Verified via raw-byte inspection (`repr()` of the actual socket bytes, not just substring checks) that both the login-time auto-`look` and an explicit `look` command now show exactly one blank line before the room content.
- Reran the full smoke-test suite (`smoke_test.py`, `smoke_test_accounts.py`, `smoke_test_quit.py`, `smoke_test_trade_attrs.py`) — all pass; none of them asserted on exact leading whitespace so this was a purely additive formatting fix.
- Next: same as before (real interactive `telnet` pass, deferred login-flow polish, Phase 2 of the roadmap).

### Session 4 — 2026-07-01 — Trade-based point-buy (raise/lower within +/-30 per stat)
- `being.h`: replaced the allocation-only rule with a true trade rule. Added `ATTR_DELTA_CAP` (30); `ATTR_MAX` (250) kept as a now-mostly-symbolic absolute ceiling since it's unreachable under the new tuning.
- `descriptor.c`: `CONN_CHAR_CREATE_ATTRS` no longer rejects negative amounts -- the per-stat check is now a range check (`amount` must be in `[-ATTR_DELTA_CAP, +ATTR_DELTA_CAP]`) instead of the old "amount ≥ 0 and base+amount ≤ ATTR_MAX" pair. The pool-sum check (`attrs_allocated(...) > ATTR_POOL`) was already written in a way that naturally supports negative deltas (it sums `value - ATTR_BASE` across all 6 fields), so no change was needed there -- lowering a stat correctly frees up room elsewhere for free. `show_attr_screen()`'s help text updated to describe the trade mechanic.
- New `tests/smoke_test_trade_attrs.py`: covers the +30 cap, the -30 floor, rejection just past each cap (with the attribute confirmed unchanged after a rejected attempt), a trade that nets to zero pool spend, exhausting the pool exactly via 4 positive stats offset by one negative, and specifically distinguishes a **pool-exhaustion rejection** (a within-cap amount rejected only because the net pool is spent) from a **per-attribute-cap rejection** -- plus `reset` still working.
- `tests/smoke_test_accounts.py` needed a fix: it hardcoded `str 100` from the old +/-130-ish range, which now exceeds the +/-30 cap and would be rejected. Rewrote that section to allocate 4 attributes at +30 each (exactly exhausting the 120 pool) and test the overspend rejection with a valid in-range amount instead.
- Rebuilt clean (zero warnings), reran all four smoke tests (`smoke_test.py`, `smoke_test_accounts.py`, `smoke_test_quit.py`, `smoke_test_trade_attrs.py`) — all pass.
- Next: same as before (real interactive `telnet` pass, deferred login-flow polish, Phase 2 of the roadmap).

### Session 3 — 2026-07-01 — `quit` command
- `cmd_dispatch()` and every `cmd_*` handler's signature changed from `void` to `bool` (`include/cmd.h`, `src/cmd/cmd_internal.h`, `cmd_table.c`, `cmd_look.c`, `cmd_who.c`, `cmd_score.c`) so a command can signal "close this connection" through the normal dispatch path — `false` means close, `true` means keep going. `descriptor.c`'s `CONN_PLAYING` case now does `return cmd_dispatch(d, line);` instead of always returning `true`.
- New `src/cmd/cmd_quit.c`: sends a goodbye message, returns `false`. Registered in `cmd_table.c` as `quit`.
- New `tests/smoke_test_quit.py`: confirms `quit` sends the goodbye message, the server actually closes its end of the TCP connection (checked via a follow-up `recv()` returning `b''`/EOF, not just client-side silence), and the departed player disappears from a second connection's `who`.
- Rebuilt clean (zero warnings) and reran the full test suite (`smoke_test.py`, `smoke_test_accounts.py`, `smoke_test_quit.py`) — no regressions from the `cmd_*` signature change.
- Next: same as Session 2's "Next" list below (real interactive `telnet` pass, deferred login-flow polish, Phase 2 of the roadmap).

### Session 2 — 2026-07-01 — Multi-character accounts, account menu, point-buy attribute creation
- New table `db/sneezy/player_attrs.sql` (player_id PK/FK → `player.id` ON DELETE CASCADE, one column per attribute, schema-only, DEFAULT 120 each). Loaded into the live `sneezy` DB on db.kullit.com with `mariadb sneezy < db/sneezy/player_attrs.sql`.
- `being.h`: added `attrs_t` (strength/dexterity/constitution/intelligence/wisdom/charisma) + `ATTR_BASE`/`ATTR_POOL`/`ATTR_MAX` constants; `being_create_pc` now defaults them to `ATTR_BASE`.
- `player_repo.{h,c}`: `player_load`/`player_load_room` now take + enforce `account_id`; added `player_create`'s third `attrs` param, `player_delete` (ownership-checked, relies on the FK's `ON DELETE CASCADE` for the attrs row), `player_list_by_account`, `player_attrs_load`/`player_attrs_save` (upsert via `INSERT ... ON DUPLICATE KEY UPDATE`).
- `descriptor.h`/`descriptor.c`: replaced the old single `CONN_GET_CHAR_NAME` state with `CONN_ACCOUNT_MENU` → `CONN_CHAR_CREATE_NAME` → `CONN_CHAR_CREATE_ATTRS` (point-buy loop: `<stat> <amount>` / `reset` / `done`, with live remaining-points display and overspend/cap rejection) and `CONN_CHAR_DELETE_CONFIRM` (typed `YES`). Added a shared `enter_world()` helper (room load + welcome + auto-`look`) used by both "play an existing character" and "just finished creating one".
- New `score` command (`src/cmd/cmd_score.c`) to view a character's attributes — not a port of anything in the original (no equivalent for this simplified attribute set), added because point-buy would otherwise be write-only.
- Wrote `tests/smoke_test_accounts.py` (new) covering the full menu/create/point-buy/delete flow; updated `tests/smoke_test.py` and `smoke_test_login.py` (both broken by the new login sequence -- they used to expect a direct "character name" prompt right after password) to go through the account menu instead.
- Synced to db.kullit.com, rebuilt (`cmake --build build` — clean, zero warnings, `cmd_score.c` auto-picked-up by the `GLOB_RECURSE` source list), reseeded the new table, reran all three smoke tests. Everything passed — see the checked-off Open Questions above for the specifics verified.
- Next:
  1. A real interactive `telnet` pass on the account menu / point-buy dialog specifically (scripted tests don't catch UX rough edges like awkward line wrapping or confusing prompts).
  2. (Both password-safety items above — delete-time reconfirm and account-creation confirmation — are now done.)
  3. Pick up Phase 2 per the roadmap: `spec/`/`cmd/` (near-C, low risk) or start the `obj/` category-collapse design.

### Session 1 (part 3) — 2026-07-01 — Renamed the project to "Tobin"
- Renamed everywhere within `c_port/` (25 files): header guards, the `DB_TOBIN` enum (was `DB_SNEEZY`), the `db_name_tobin` config field, `TOBIN_DB_*`/`TOBIN_PORT` env vars (was `SNEEZY_DB_*`/`SNEEZY_PORT`), the `tobin_c` CMake target/binary name (was `sneezy_c`), the login banner ("Welcome to Tobin..."), and doc titles/prose in README.md and STATUS.md.
- **Deliberately left unchanged** (see the new "Project name" row in Architecture decisions above): the untouched original engine in `code/`/`lib/` (out of scope, user-confirmed); the literal MariaDB database name `sneezy` and all `db/sneezy/*.sql` path references (renaming the string would break DB connectivity, since `db/` itself wasn't renamed); the one attribution sentence in README.md describing the real upstream SneezyMUD project.
- Synced the renamed files to db.kullit.com, rebuilt (`cmake --build build`), reran both smoke tests against the new `tobin_c` binary — same results as the pre-rename verification pass, confirming the rename didn't break anything.
- Next: same as Session 1 part 2's "Next" list below — nothing rename-specific left to do.

### Session 1 (part 2) — 2026-07-01 — Built + verified live on db.kullit.com via SSH
- SSH access to db.kullit.com (10.0.0.12, Fedora 44, same box `../talker.c` runs on) was set up: generated a local ed25519 keypair, user added the public key to `root`'s `authorized_keys`.
- Installed missing build deps on the box: `cmake`, `mariadb-connector-c-devel` (gcc, pkg-config, and the MariaDB server/client were already present).
- Copied `db/` (19M, all seed data) and `c_port/` to `~/NewMUD/` on the box.
- Ran `db/init-db.sh root` — created and seeded `sneezy` + `immortal` databases (didn't touch the box's pre-existing unrelated `world`/`albums` DBs). Confirmed: 19,209 rooms loaded, room vnum 0/1 ("The Void"/"Imperia") present.
- `cmake -B build && cmake --build build`: **compiled clean, zero warnings** after two small fixes (see below).
- Fixed two bugs found only by actually building: (1) `strcasecmp` needs `<strings.h>`, not `<string.h>` — missing include in `db.c`. (2) A `/*` inside a prose comment in `cmd_table.c` tripped `-Wcomment` — reworded. Also added an `fflush()` in `log.c` since stdout is fully-buffered (not line-buffered) when redirected to a file, so log output wasn't appearing until process exit.
- Ran the server (`TOBIN_DB_HOST=localhost TOBIN_DB_USER=root TOBIN_DB_NAME=sneezy ./build/tobin_c`), confirmed it's listening on :4000, and wrote+ran two scripted smoke tests (`tests/smoke_test.py`, `tests/smoke_test_login.py`) that drive the raw wire protocol end-to-end. All checks passed — see Open Questions above for the full list of what was confirmed.
- Left `tobin_c` running in the background on the box (PID varies per restart — check with `pgrep -af tobin_c`) so it can be poked at interactively.
- Next:
  1. Do one manual pass with a real interactive `telnet` client (not just the scripted raw-socket tests) to sanity-check the actual user-facing feel (echo behavior, backspace, prompts).
  2. Enforce character-name-belongs-to-account (currently anyone can "log into" any existing character name once past *some* account's password — noted as a gap above).
  3. Pick up Phase 2 per the roadmap: either `spec/`/`cmd/` (near-C, low risk) or start the `obj/` category-collapse design now that there's a proven-working base to build on.

### Session 1 (part 1) — 2026-07-01 — Scaffolded c_port/, wrote the walking-skeleton login/look/who slice
- Built: full `c_port/` tree — `CMakeLists.txt`, `include/*.h`, `src/**/*.c` (log, config, db + account/player/room repos, net/main_socket/socket/descriptor, core/thing/being/room, cmd/cmd_table+look+who, world, game_loop, main). ~20 files per the session-1 plan.
- Verified: not yet at the time — no C toolchain available in that part of the session. Hand-reviewed against the original source instead. (Verified for real in part 2 above, same session.)
