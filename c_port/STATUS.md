# Tobin C Port — Status

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
| cmd/ (66 files) | 27K | `src/cmd/` | **11/66 ported** (`look`, `who`, `score`, `quit!`, `color`, `attack`, `kill`, `say`, `limbs`, `help`, `wizhelp`) | Dispatch table (`cmd_table.c`) does prefix/abbreviation matching, not exact-string lookup (Session 9) -- see the "Command parsing" decision row. `score`/`color`/`attack`/`kill`/`limbs`/`help` are new-to-Tobin (no direct equivalent for this simplified feature set, or a deliberate simplification of a much heavier original mechanism -- see `help` in the "`help`/`wizhelp`" decision row); `say` and `wizhelp` are real ports of the original's actual mechanisms. `cmd_dispatch()` returns `bool` (every `cmd_*` handler's signature changed from `void` to `bool` to match) -- `quit!` returning `true` means "leave the character, back to the account menu"; only the account menu's own `quit!` (handled directly in `descriptor.c`, not through `cmd_dispatch`) returns `false` to actually disconnect. Every `CONN_PLAYING` reply ends with a trailing `\r\n> ` prompt (Session 9, blank line added Session 14). As of Session 10, `cmd_dispatch()` also gates on the wait-state before allowing any command through. As of Session 16, `cmd_dispatch()` also special-cases a leading `'` (see the "`say`" decision row) before the normal whitespace-delimited verb split. As of Session 18, `cmd_entry_t` (moved to `cmd_internal.h`, shared with `cmd_help.c`) carries a `help` one-liner and `min_level` per command -- display metadata only, not enforced by `cmd_dispatch()`. |
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
