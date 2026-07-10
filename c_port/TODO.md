# Tobin — TODO

Last updated: 2026-07-07. Companion to STATUS.md, which holds the full
session log, decisions, and history — **this file tracks only what's NEXT.**
Completed items are pruned from here as they land (find them in STATUS.md).

All in-game editors are menu-driven, like character creation — see the
[[editors-menu-driven]] memory. The user provides a wireframe for each.
Editor commands are named **`ed<noun>`** (user 2026-07-05): `edroom` (rooms),
`edhelp` (help), `ednews` (news), `edwiznews` (wiznews), `edplayer`
(players); future `edobject`/`edmob`/`edzone`/`edaccount`. Read-only
viewers keep plain names (`news`, `wiznews`).

## Buildable now (no blocked dependencies)

Self-contained — no need for the object/mob systems. Keep working through
these; each ships with a smoke test + (if player-facing) a news entry.

### User batch 2026-07-09 (home session, post-NewMUD-migration) — working these now

- [x] **Corpse on death (mobs and players alike)** — done (Session 43,
      user: "make it so the corpse of a char loads into the room upon
      death. the corpse should be treated like a container and all
      inventory can be taken off said corpse... mobs and players alike"):
      `combat_defeat()` now creates an ephemeral "corpse of <name>" container
      (`obj_create_ephemeral()`, same primitive as the crit-hit severed
      limbs -- vnum 0, never persisted) and moves everything the loser had
      -- carried, worn, held -- INTO it instead of dropping loose on the
      floor. Not takeable as a whole (`get corpse` alone is refused) and
      never closed/locked, so `get <item> corpse` works immediately with
      no `open` needed. Applies to BOTH a PC's death and a mob's (a mob's
      corpse is empty today since mobs don't carry anything yet, but the
      object itself still appears). `smoke_test_corpse.py` (7 checks).
- [x] **Merge `mload`/`oload` into one `load <mob|obj> <vnum|name>`** — done:
      `cmd_load.c` replaces both; category is abbreviatable down to the bare
      letter (M/O), same as full words. Table-order gotcha (like set/setsev):
      `load` is a prefix of `loadroom`, so `loadroom` now needs `loadr`+ (was
      `loa`+). `R` (ride/follow?) never came up again -- still unimplemented,
      ask if it resurfaces. Old help topics removed + merged; 3 tests updated.
- [x] **Equipment display reformat** — done: right-aligned `label: value`
      columns (14-char field, matching "secondary hold"), replacing the old
      `<label> value` bracket form. Hand slots renamed to **`primary hold`**/
      **`secondary hold`**, now correctly tracking the caller's dominant hand
      (handed_right) instead of a fixed held[0]/held[1] (a latent bug for
      left-handed characters, fixed in passing). **Genitalia removed from
      the listing** -- never actually wearable, just cosmetically listed;
      becomes an object on decapitation instead (crit-hit item below, still
      unbuilt).
- [x] **`hold` vs `wield`, and a `switch` command** — done: `wear` now only
      covers body-slot equipment; a holdable item refuses `wear` and points
      to whichever of `hold` (non-weapons) / `wield` (weapons, gated on
      `obj_t.category == OBJ_CAT_WEAPON`) applies. `switch` swaps
      `held[0]`/`held[1]` in place. Table collisions resolved (documented
      inline): `switch` needs `swi`+, `wield` needs `wie`+, `hold` needs `ho`+.
- [x] **Gender-specific pronouns in ALL mud output** (user 2026-07-09,
      standing habit going forward like the colorize-tastefully rule) —
      uses `gender_subject/object/possess()` (being.c, Session 23), plus a
      new `gender_reflexive()` (himself/herself/itself, Session 43). DONE:
      the link-loss line, `stand`'s room echo (both earlier sessions), and
      now `src/core/socials.c` -- despite the "~15 pairs" estimate, only 3
      of the 16 socials actually used a gender-neutral pronoun once checked
      carefully (`shake`'s "their head" in all 3 echoed forms, `poke`'s
      "themselves", `comfort`'s "they need"): the table keeps its bare
      fallback text (so `social_names()`/the table stay the single source
      of truth for each social's shape) but `social_try()` now overrides
      those 3 specifically with the actor's real pronoun before display.
      New player-facing messages should stay gender-aware going in.
- [x] **Linkdead persistence** (user 2026-07-09) — losing link no longer
      destroys the character: `descriptor_destroy()` detaches (`desc=NULL`)
      instead, leaving the being in its room; `world_find_linkdead_pc()`
      (world.c) finds it on reconnect, `enter_world()` does a fresh DB load
      as always (so a concurrent promotion/edit still applies) but resumes
      it in the linkdead body's room, then discards the old body. Recovers
      via reconnect or process end only (copyover only restores
      descriptor-attached beings). Room listing tags "(linkdead)"; combat
      can't target a linkdead PC at all ("no one can manipulate a linkdead
      char") -- `combat_find_room_target()` skips them. `smoke_test_linkdead.py`
      (8 checks). Fixed 5 existing tests whose abrupt-close-right-after-
      creation pattern now goes linkdead instead of destroying, breaking
      their "SQL-set field takes effect on next login" assumption -- each
      needed an explicit `quit!` first (objects, mobiles, edplayer, set,
      sector_color).
- [x] **World death taunt: PC deaths only** (user 2026-07-09, "should only
      fire when a player dies, skip the mobs unless the mob is the killer")
      — `combat_defeat()` (combat.c) wraps the `[INFO]` broadcast block in
      `if (loser_is_pc)`; a mob's death (Phase 2D) is now silent world-wide,
      while a mob-as-killer still taunts normally (the taunt names the
      loser, not the winner). `smoke_test_mobiles.py` section 5 covers it
      (bystander confirms no `[INFO]` on a mob death).
- [ ] **Account menu: hide the character list until `C`** — currently
      `-- Your characters --` lists every character immediately on reaching
      the account menu. Change so the list is HIDDEN until the player types
      `C` (bare, no number/name yet); typing bare `C` then reveals the list
      and prompts for a number/name (or `N` to create) as a follow-up step.
      `C <number|name>` (already-known target) should probably still connect
      directly without the extra round trip -- confirm with user if that
      one-step form should stay.
- [x] **Port Sneezy's crit-hit system + decapitation object creation** (user
      2026-07-09) — done, scope confirmed with the user first: (1) no
      separate crit-roll -- triggers purely on a limb's HP crossing to 0%
      from ordinary combat damage (combat_strike() in combat.c); (2) ALL
      limbs sever into a lootable ephemeral object ("X's severed <limb>",
      obj_create_ephemeral() in obj.c/obj.h), not just the head -- genitalia
      included, per the user's example; (3) the HEAD specifically is a
      decapitation -- an instant kill routed through the existing
      combat_defeat() "slain" path; (4) PCs only for v1 -- a mob's limb
      reaching 0 HP does nothing extra (mobs still die the plain Phase-2D
      way). Fixed a real balance bug found while scoping this: a level-1
      character's limbs were splitting to 1 HP each (25 max HP / 13 limbs),
      so ANY landed hit already destroyed a limb -- added a `LIMB_MIN_MAX_HP`
      floor (15) in being_limbs_full_heal() so severing/decapitation takes a
      real run of hits even at level 1, not a first-swing coin flip. Added
      an immortal-only debug command `hurtlimb <target> <limb> <hp>`
      (cmd_hurtlimb.c) to test this deterministically instead of waiting on
      combat RNG. `smoke_test_crit.py` (18 checks, including the PCs-only
      scope guarantee).

### User batch 2026-07-07 (home session) — working these now

- [x] **Consistent editor slash-commands** (user 2026-07-07) — done: one set
      keyed to each action's first letter -- `/s` Save, `/a` Abort, `/b` Blank
      (clear), `/f` Format -- centralized in `editor_feed()` (descriptor.c) so
      it covers every ed* editor at once. The old `.`/`~`/`/clear`/`/format`
      keys were removed (user follow-up) -- a bare `.`/`~` is now literal text.
      All editor intro lines + the editor smoke tests updated;
      `smoke_test_editor_format.py` broadened to cover the whole key set.
- [x] **help/wizhelp list size + vnum pagination** — done: help/wizhelp list
      buffer 2048->8192 and name arrays 256->512 (no truncation as commands
      grow); `vnum` now pages the full list (descriptor pager, like `news`)
      instead of stopping at 40, with a 500-row safety cap.

- [x] **Port `scan`** — done 2026-07-07: `cmd_scan.c`, a faithful port of the
      original's `doScan()` (misc/range.cc). Ray-casts up to SCAN_MAX_RANGE (6)
      rooms deep down each exit and reports the players/mobs out there with a
      distance word + direction; `scan <dir>` scans one direction, `scan <name>`
      filters by name, a closed/secret door blocks the line of sight.
      (Skipped the original's move-point cost + blindness gate -- Tobin has
      neither.) Follows exit chains through unloaded rooms via a `roomexit`
      query; occupants come from active (`world_get_room`) rooms only.
      `smoke_test_scan.py`, help topic, news entry ("Cast Your Gaze Afar").
- [x] **`vnum <room|obj|mob> <pattern>`** — done 2026-07-07: `cmd_vnum.c`,
      builder tool (51+). Lists the vnums + names of rooms/objects/mobiles
      whose `name` contains a substring (direct DB_TOBIN query, per
      cmd_mudstats precedent), lowest vnum first, paged a screen at a time
      (descriptor pager, like `news`; 500-row safety cap). Category is
      abbreviatable. `smoke_test_vnum.py`, help topic (no news -- immortal-only,
      same precedent as oload/mload).

### User batch 2026-07-07 (reported during the mobiles session) — working these next

- [x] **`look <object>` doesn't work** — already done in Session 37 (this box
      was just never pruned; verified 2026-07-09). `look_at_target()` falls
      back from PC/mob to a room-floor-then-own-inventory `THING_OBJ` search
      (`find_obj_here()`), showing `long_descr` + a condition line from
      `cur_struct`/`max_struct` (`obj_condition_text()`). Worn/held covered too
      (same `stuff_head` chain). Covered by `smoke_test_objects.py`.
- [x] **`help color`/`help who`: list every color tag + mention `<N>`** —
      done (Session 43, help_topic.sql migration): `help color` now lists
      every `<x>` tag itself (previously only the separate `help colors`
      topic did) and mentions title `<N>`/`<n>` substitution; `help who`
      now mentions that a shown title can use both color tags and `<N>`/
      `<n>`.
- [ ] **`bamfin`/`bamfout`** — classic wizard commands: an immortal sets
      their own custom arrival ("bamfin") / departure ("bamfout") message
      shown when they `goto`, plus a customizable regular-movement message
      template (e.g. "Jesus drags his cross in from the <direction>.").
      Needs new persisted per-player fields (likely new `player` columns)
      wired into `cmd_goto.c`'s and `cmd_move.c`'s existing room-echo calls.
- [x] **Colorize copyover messages** — done (Session 43): the 3 player-
      facing reboot lines in `cmd_copyover.c` (5-second warning, mid-
      reborn, please-reconnect) now use `<c>...<z>`, matching the existing
      INFO/`system`-broadcast color convention. The immortal-only error/
      status lines (unavailable, write-failed, exec-failed) stay plain.
- [x] **`@set` currently just falls through to Huh?!** — done (Session 43):
      a leading `@` is now stripped before the normal verb parse in
      `cmd_dispatch()` -- simpler than a hardcoded `'`/`;`-style alias since
      the real verb ("set") already follows the `@`, no need to hardcode a
      target. Covers any other stray leading `@`, not just `@set`.
- [ ] **Verify multiplay-off actually gates a second mortal connection**
      — Session 21 claims `enter_world()` already refuses a mortal
      account's second connected character when the `multiplay` game flag
      is off, with immortals exempt; user flagged uncertainty ("not sure
      this is so") -- re-verify live (a quick two-connection test) before
      trusting it, fix if it's actually broken.
- [ ] **`gametog` (58+)** — split `toggle`: game-wide switches (currently
      `multiplay`, living inside the unified `toggle` command) move to a
      new `gametog` command gated 58+; `toggle` keeps only the
      mortal-settable personal switches (color, hp, ...). Needs
      `cmd_toggle.c`'s `TOGGLES[]` table split in two (or filtered by a
      new per-row "game-wide" flag).
- [x] **Editors must get ABSOLUTE quiet** — done (Session 43, user: "when
      in the editors, no messages to interrupt, no logs, no output at
      all. thats what catchup is for"). The audit found the REAL bug:
      `descriptor_in_editor()` (descriptor.c) only ever checked the
      `CONN_REDIT_*` range -- `edplayer` and `edzone` were never wired in
      at all, so `descriptor_notify()`'s hold-for-catchup silently never
      applied to them, even though every broadcast call site (game_log,
      death taunt, wiznet, system, newbie) already correctly called
      `descriptor_notify()`. One-line fix in the shared predicate, not a
      per-call-site chase. Also found and fixed the same-root-cause
      pattern elsewhere: `who`/`promote`/`set`/`copyover`/`users` all used
      `state == CONN_PLAYING` as an "is online" proxy, which excludes
      every editor sub-state -- an editing immortal was invisible to
      `who`, got stale live-sync from `set`/`promote`, lost their session
      entirely across `copyover`, and showed as "closing" in `users`. All
      fixed to check `it->character` (or the new range check) instead.
      `smoke_test_held.py` extended to cover edplayer/edzone, not just
      edroom (the gap the old test couldn't have caught).
- [ ] **`edbug`** — a way to annotate or resolve an already-filed bug
      report (so a player can be told their bug was fixed), instead of
      only being able to `delbug` (delete) it outright. Menu-driven or
      one-shot, matching the `ed*`/`set` precedent.
- [ ] **`mlist`/`olist`/`rlist` (builder list commands)** — list available
      mob/object/room prototypes; each accepts a bare vnum, a vnum range,
      or a name/keyword substring filter. Parallels the original's real
      list commands. Reads straight from the `mob`/`obj`/`room` tables
      (no new Tobin tables), same "prototypes already exist" precedent as
      `oload`/`mload`.
- [x] **`hit` command (real combat, never instakill)** — done (Session
      43): `cmd_hit.c` is a thin passthrough to `cmd_attack()` (which never
      special-cased immortals to begin with), so an immortal typing `hit`
      gets the normal multi-round combat process instead of `kill`/
      `attack`'s instant slay. Those two are unchanged. New help topic +
      `smoke_test_combat.py` Part 4.
- [ ] **General output pagination (20-line threshold)** — any command
      output longer than 20 lines should paginate automatically (a "more"
      prompt, ENTER for next page / Q to stop -- same UX `news`/`wiznews`
      already have), with one blank line before and after each page.
      Currently only `news`/`wiznews` paginate; this asks for a shared,
      reusable helper so every long output gets it for free, not just
      those two commands.
- [x] **Smoke tests still aren't logging start/finish to the MUD's log** —
      fixed 2026-07-07 (Session 36): the Session 32 `announce()` helper had
      in practice only ever landed in the one file that introduced it
      (`smoke_test_logging.py`); every other test was silently missing it.
      Retrofitted all 56 `tests/smoke_test_*.py` with a self-contained
      `announce()`/`announce_done()` pair; `descriptor.c`'s `@test` hook now
      also recognizes `@test done <name>` and logs `[TEST] finished %s`
      (distinct from `running %s`). See STATUS.md.

### User batch 2026-07-06 (morning queue) — working these next

- [x] **Port `setsev` log severity** — done 2026-07-06: `cmd_setsev.c`, a
      port of `misc/immortal.cc`'s `doSetsev()`. Bare `setsev` lists every
      log type (game/pio/combat/bug/db/edit) with on/off state; `setsev
      <type>` (abbrev ok) flips one, gating `game_log()`'s `[TAG]` echo via
      a new `being_t.severity` bitmask (default: everything on). The
      personalized `jesus` type is hidden from and unsettable by anyone but
      the immortal actually named Jesus, matching the original's per-name
      toggle. Deliberately simplified vs. the original: session-only, not
      persisted (the original's `wizdata` table isn't worth a migration for
      this) -- see the field comment in `being.h`. `smoke_test_setsev.py`
      (help topic added too; no news entry -- immortal-only, same precedent
      as `toggle`/`exec`/`wiznet`).
- [x] **Colorize room name + description by sector** — done 2026-07-06:
      `sector_color()` (room.c) buckets each of the 61 sector types by
      keyword (lava/fire->red, city/road/building->white, mountain/cave/
      solid rock->gray, ocean/river/beach->blue, arctic/atmosphere->cyan,
      desert->yellow, swamp/forest/jungle/grassland/plains/hills->green,
      astral->purple, else->white). `cmd_look.c` wraps the room NAME in the
      bright (uppercase) tag and the DESCRIPTION in the dim (lowercase) one,
      for both the mortal and immortal-builder-header display paths.
      `smoke_test_sector_color.py` (raw-byte ANSI checks, mortal + immortal
      paths); help topic updated.
- [x] **Editor `format` option** — done 2026-07-06: `/format` (matching the
      existing `/clear` slash convention) reflows the shared editor buffer
      (`editor_format()` in descriptor.c) to `EDITOR_FORMAT_WIDTH` (78)
      columns, joining/re-breaking words but preserving blank-line
      paragraph breaks. One shared implementation in `editor_feed()` covers
      every `ed*` editor (edroom's description field, edhelp, ednews,
      edwiznews, edrules) automatically, since they all route through it.
      `smoke_test_editor_format.py` (via edhelp); all editor intro
      messages + help topics mention it now.
- [x] **`set` + `@set` commands** — done 2026-07-06, as two commands: the
      menu-driven `edplayer` (see below) plus a one-shot `set <name> <field>
      <value>` (`cmd_set.c`, same 58+ gate) for quick scriptable single-field
      edits -- user confirmed both were wanted, not one instead of the other.
      `set` covers the same fields as `edplayer` (level/xp/hp/attributes/
      gender/title/loadroom/handed), same admin-wide-by-name reach and
      online-target live sync. The original's 1279-line `@set` covers
      classes/factions/objects/mobs/rooms Tobin doesn't have, so neither
      command attempts that. Note: `set` (exact 3 letters) had to be placed
      BEFORE `setsev` in `cmd_table.c` -- both start with "set", first match
      wins, and `set` needs to win that exact typo/abbreviation. Refactored
      the attribute-name-to-field lookup (`attr_field`, previously `static`
      in `descriptor.c`) into a public `attrs_field()` in `being.c`/`being.h`
      so `edplayer`, character creation, and `set` all share one copy.
      `smoke_test_set.py` (gate, validation, every field, persistence via
      reconnect, online live-sync) + help topic.

### User batch 2026-07-05 (late night, follow-ups #2) — working these next

- [x] **Idle disconnect: immortals immune** — done 2026-07-05 (built, pending
      deploy): "do both" per user. Added `descriptor_idle_timeout` pulse (60s)
      that disconnects playing MORTALS idle > IDLE_DISCONNECT_SECS (30 min)
      with a message; immortals never idle-dropped. Also made the keepalive
      NOP more aggressive (30s -> 12s) to survive tight NAT/router windows.
- [x] **Typed logs (LOG_GAME + personalized)** — done 2026-07-05 (built,
      pending deploy): `log_type_t` enum in log.h (LOG_SILENT, LOG_GAME
      generic, LOG_PIO, LOG_COMBAT, LOG_BUG, LOG_DB, LOG_EDIT, LOG_JESUS) +
      `log_type_name`/`log_type_personal_name`. New `game_log(type, fmt, ...)`
      (in descriptor.c): writes the file line tagged with the type and echoes
      a cyan `<c>[TYPE]<z>` line to non-editing immortals -- except LOG_SILENT
      (file only) and personalized types (LOG_JESUS -> only the immortal named
      Jesus). The link-loss broadcast now goes through `game_log(LOG_PIO,...)`
      (tag `[PIO]` instead of `[LOG]`; notify test updated). Other events
      (quit/delete/bug) can adopt game_log with their type as they land.
- [x] **Title `<N>` substitution** — done 2026-07-05 (built, pending deploy):
      `title_with_name()` in cmd_who.c replaces every `<N>`/`<n>` token with
      the character's name anywhere in the title; when present the title shows
      alone (name embedded), else `Name title` as before. Other color tags
      pass through untouched.
- [x] **Abbreviation → closest command (incl. socials)** — done 2026-07-05
      (built, pending deploy): `social_try` now prefix-matches like the command
      table (first match wins), so `poi`/`poin` -> `point`. Commands are still
      tried before socials, so a real command always wins.
- [x] **`exec` (level 60 only)** — done 2026-07-05: `cmd_exec.c` runs host
      shell commands (Implementor-only). Fenced 3 ways: a blocklist refuses
      dangerous commands (rm/kill/reboot/mkfs/dd/sudo/tobin_c/mariadb/etc.),
      every command runs under `timeout 10` so it can't freeze the game loop,
      and each use is logged (EXEC:/EXEC REFUSED:). Output capped at 8KB.
      `smoke_test_exec.py`.
- [x] **Help format: colorized Syntax/Minimum Level** — done 2026-07-05
      (built, pending deploy): `help <cmd>` now shows the description in
      magenta `<m>...<z>`, then `<c>       Syntax:<z> <syntax>` and
      `<c>Minimum Level:<z> <n>` (right-aligned cyan labels). Syntax is parsed
      out of the body's leading `Usage:` line (fallback: command name); level
      from the command table. Prose topics get just the magenta body.
- [x] **`flee`** — done 2026-07-05 (built, pending deploy): `cmd_flee.c` --
      while fighting, ~2/3 chance to bolt through a random real exit; on
      success both sides stop fighting and you move to a neighbouring room, on
      failure you stay locked in. `smoke_test_flee.py`, help topic.
- [x] **`toggle`** — done 2026-07-05: `cmd_toggle.c` with an extensible
      TOGGLES[] table. Bare `toggle` lists switches + values; `toggle <name>`
      (abbrev ok) flips one. Player toggles (color, hp) affect only you; game
      toggles (multiplay) are hidden from and locked to <55 and flippable by
      55+. New features add a row. `smoke_test_toggle.py`.
- [~] **Colorize displays tastefully (ongoing habit)** — standing guideline
      (saved to memory: tobin-colorize-habit). Applied to new output (toggle,
      flee, exec, help format) using lowercase dim codes; keep doing it.

  NOTE: added `<m>`/`<M>` as magenta aliases for `<p>`/`<P>` in colorstring.c
  (the user's help format uses `<m>`), and made `help <cmd>` render its body
  in magenta with cyan Syntax/Minimum Level labels.

### User batch 2026-07-05 (evening) — working these first

- [x] **wizhelp: usable-only + reformat** — done 2026-07-05: wizhelp already
      filtered to usable commands; removed the `[NN+]` level tag and made it a
      three-column alphabetical list of command names.
- [x] **help / wizhelp in three columns, alphabetical** — done 2026-07-05:
      shared `send_columns()` in cmd_help.c (qsort + 3-col); names only,
      `help <cmd>` for details.
- [x] **Prompt newline** — done 2026-07-05: the game-loop prompt is now
      `\r\n\r\n> ` (a blank line before each prompt). NOTE: this yields two
      blank lines when the preceding output ends in a newline -- confirm one
      is not enough if it looks like too much whitespace.
- [x] **Keepalive** — done 2026-07-05: `descriptor_keepalive` pulse (main.c,
      ~30s) sends an IAC NOP to every connection so idle players aren't dropped
      by NAT/router timeouts. Verified live (NOP received); not in the sweep
      (a 30s timer would slow it).
- [x] **`wiznet`** — done 2026-07-05: immortal-only broadcast to all online
      immortals (`cmd_wiznet.c`). `smoke_test_wizcomm.py`.
- [x] **`system`** — done 2026-07-05: immortal-only global echo -- sender sees
      `system <msg>`, everyone else the bare `<msg>` (`cmd_system.c`).
- [ ] **Socials → DB + full Sneezy set + `edsocial` (55+)** — move socials
      from the compiled table to a DB table; port the full social set from
      `sneezymud-master/lib/actions`; add `edsocial` (55+, menu-driven ed*
      editor) to edit them in game.

### User batch 2026-07-05 (late) — working these next

- [x] **Lose the `[ wiznet ]` prefix** — done 2026-07-05: wiznet shows just
      `<Name>: <msg>` in purple.
- [x] **`mudstats`** — done 2026-07-05: `cmd_mudstats.c` reports room/mob/obj
      counts from the DB. `smoke_test_mudstats.py`.
- [x] **Idle flag** — done 2026-07-05: `descriptor.last_active` (set on each
      input); `who` shows `(idle)` after 5 min, any command clears it.
      `smoke_test_idle.py` (active-not-flagged; the 5-min appearance is
      logic/manual-verified, too slow for the sweep).
- [x] **Log quit/deletes** — quit + link-drop already logged; character delete
      now logs too (2026-07-05). Account-delete logging lands with `wipe`
      (there's no account-delete flow yet).
- [x] **Daily log files + 21-day retention** — done 2026-07-05: one
      `<YYYY-MM-DD>.log` per day, appended across reboots/copyovers; `*.log`
      older than 21 days pruned (by mtime) at each open; `log rotate` now just
      re-opens the day's file. `smoke_test_logs.py` updated.
- [ ] **`wipe` (59+)** — wipe a pfile or an account; requires a password to
      execute; only *lower*-level characters may be targeted (a 59 cannot wipe
      another 59, etc.). Destructive -- confirm + password gate.
- [x] **`;` wiznet shorthand** — done 2026-07-05: `;<msg>` broadcasts to
      immortals (cmd_dispatch special-case, like `'` for say).
- [ ] **`alias` command** — players define their own aliases, stored on the
      ACCOUNT and shared across that account's characters. Scoped by tier: an
      immortal's aliases apply only to their immortal characters; a mortal's
      apply to all mortal characters on the account. Needs a DB table
      (account_id, tier, name, expansion), an `alias` command (add/list/remove),
      and alias expansion in cmd_dispatch before command matching.
- [x] **Immortal color tiers in who/score** — done 2026-07-05:
      `being_rank_color()` (51-53 `<c>`, 54-56 `<C>`, 57-58 `<p>`, 59+ `<P>`)
      tints the name in who and score.
- [x] **`goto <char>`** — done 2026-07-05: goto now accepts a player name and
      teleports to that online being's room (mobs too, once they exist).
- [x] **`help edit`** — done 2026-07-05: dynamic index of `ed*` commands the
      caller can use (auto-updates as editors are added), pointing to each
      one's `help <name>`.
- [x] **Multiplay control** — done 2026-07-05: `multiplay <on|off>` (59+)
      game flag persisted in `game_config`; enter_world refuses a mortal
      account's second connected character when off; immortals exempt.
      `smoke_test_multiplay.py`.
- [ ] **Holdable items + `point` social** (BLOCKED on Objects/2C) — players
      grab/hold items in hand (primary, then secondary); a `point` social
      shows "X points at you with his/her/its <primary-hand item>". Needs the
      object system. (Basic no-arg `point` -> "You point around randomly." is
      buildable now, in the night batch below.)

### User batch 2026-07-05 (night, follow-ups) — working these now

- [x] **who/score: color the bracket, not the name** — done 2026-07-05: the
      rank-tier color now wraps the `[ Implementor ]` bracket in who and the
      `Level:` field in score; the name is uncolored. `smoke_test_level_titles`
      updated to strip ANSI for format checks + assert the bracket (not the
      name) is colored.
- [x] **Help footer: Usage + Level** — done 2026-07-05: viewing `help <cmd>`
      auto-appends `Usage: <name>` and `Level: <min_level>` from the command
      table (cmd_help.c); prose topics with no command entry get no footer.
- [x] **Colored [LOG] tag** — done 2026-07-05: the immortal link-loss
      broadcast now carries a cyan `<c>[LOG]<z>` tag (verified live). Future
      log broadcasts (quit/delete display) route through the same single tag;
      a per-category taxonomy stays with the Typed-logs item.

  NOTE: also fixed `smoke_test_immortal_cmds` — `goto <non-numeric>` is now a
  player lookup ("No one named..."), so the stale "Usage: goto" expectation
  was updated (bare `goto` still shows usage).

### User batch 2026-07-05 (night) — buildable now

- [x] **Player titles + who args** — done 2026-07-05: `title <text>` sets a
      free-form title shown after the name in `who` (`title none` clears it),
      persisted in `player.title` (already an upstream column; `being.title`
      + `player_set_title()` load/save). `who` now takes an argument:
      `who imm[ortals]` / `who mort[als]` scope by rank, any other word is a
      case-insensitive name-substring filter, empty result prints a "No one
      matching" line. `cmd_title.c`, help topics for `title`/`who`,
      `smoke_test_title.py`.
- [x] **Gender + pronouns** — done 2026-07-05 (built, pending deploy): pick
      `gender male|female|neuter` on the creation screen (default neuter),
      stored in `player.gender` (migration). `gender_t` + `gender_name` /
      `gender_subject` (he/she/it) / `gender_object` (him/her/it) /
      `gender_possess` (his/her/its) helpers in being.c; shown on the score
      sheet. Socials/combat can adopt the pronoun helpers as their messages
      grow. `smoke_test_gender.py`.
- [x] **Appearance** — done 2026-07-05 (built, pending deploy): set with
      `appearance <text>` on the creation screen, stored in
      `player.appearance` (migration, varchar(255)). Shown on your own score
      sheet and to others via `look <player>` (a neuter/no-appearance target
      gives a gender-aware "nothing special about him/her/it"). Full `examine`
      stays with the objects batch.
- [x] **Color preference at account creation** — done 2026-07-05: a
      `CONN_GET_COLOR_PREF` step asks on/off during account creation and
      persists it in `account.color_pref` (migration + `account_set_color`);
      the login handshake is backward-compatible (only exact yes/no/blank is
      treated as the answer, else defaults ON and re-dispatches the line).
- [x] **`rules` + `edrules` (59+)** — done 2026-07-05: DB-backed numbered rules
      (like news/help). `rules` lists them, `rules <n>` shows rule n's body in
      magenta, `edrules <n> <title>` (59+) writes one through the shared line
      editor (`EDIT_RULES`). `rules` table + `rules_repo.c`, help topics,
      `smoke_test_rules.py`. Deployed + verified (8/8).
- [x] **Color/name tag help** — done 2026-07-05: `help colors` lists every
      `<x>` color tag with examples.
- [x] **`bug` + `delbug` (59+)** — done 2026-07-05: `bug <text>` files a report
      (stored with submitter + date, echoed to immortals as a typed `[BUG]`
      log); bare `bug` lists reports for immortals (usage for mortals);
      `delbug <id>` (59+) removes one. `bug` table + `bug_repo.c`, help topics,
      `smoke_test_bug.py`.
- [x] **Newbie channel + flag** — done 2026-07-05: `newbie <msg>` is a help
      channel reaching everyone with the `PLR_NEWBIE` flag (new `player.pflags`
      bitmask, default on so newcomers start on it). Toggle off/on with
      `toggle newbie` (persisted); you must be on the channel to speak.
      `cmd_newbie.c`, help topic, `smoke_test_newbie.py`.
- [x] **`point` (no arg)** — done 2026-07-05: basic `point` social ("You point
      around randomly." / "You point at X."). The held-item form ("...with his
      <item>") is objects-blocked above.
- [ ] **`wipe` master password** — the pending `wipe` command's password is a
      compile-time master password (settable in code).

### User batch 2026-07-05 (night) — BLOCKED on Objects (Phase 2C)

- [ ] **Money system** — gold-coin currency + commodities (ingots, nuggets,
      shards of gold/silver/obsidian/...). Repurpose Sneezy's talens/components
      for inspiration. Future: mobs drop them (economy), used in skills
      (repair, spell/prayer fuel). Needs objects.
- [ ] **Liquids** — drinkable liquids; pouring one out pools on the ground
      (from Sneezy). Needs objects/containers.
- [ ] **`fill`** — fill a container from a liquid pool. Needs liquids+objects.
- [ ] **`switch`** — swap primary/secondary held items. Needs holdable items.
- [ ] **`examine`** — look closer at things (extra descriptions). Needs room/
      object extra descriptions (partly objects, partly redit extra-desc item).

### User batch 2026-07-05 (night) — BLOCKED on Classes

- [ ] **Druid class** — add druid to the selectable classes (lands with the
      Classes system).


      the regen tick (weight by position, like HP already does). New
      `player_progress` column; show in score/prompt. Take from Sneezy.
- [ ] **Terrain movement cost** — each sector type modifies the vitality cost
      of moving into it (original `TerrainInfo`). Depends on Vitality.
- [x] **Socials/actions** — done 2026-07-05: 15 socials (smile/nod/wave/bow/
      cheer/poke/...) in `socials.c`, checked in dispatch after the command
      table; untargeted + targeted forms; `socials` lists them. Room echoes
      go through `descriptor_notify` (held for editors). More can be added to
      the table; a DB-backed/editable social set (`edsocial`?) is future work.
- [x] **Health strings** — done 2026-07-05: `being_health_word()` maps HP%
      to a word (near death ... perfect); shown in `score`'s HP line.
      Optional follow-up: also show it in the prompt (prompt-flag system).
- [ ] **PK opt-in flag** — player flag; BOTH players must have opted in for
      attack/kill between players. Toggle command + persistence + combat gate.
- [ ] **Tips system** — `tips` command + periodic tip echoes (pulse-driven),
      per-player newbie toggle, `tipedit` (53+). DB-backed like news/help.
- [ ] **Typed logs** — `log.h` log-type taxonomy; every log line gets a type,
      `log search` can filter by type.
- [ ] **`dig`** — builder-walk: moving into a nonexistent exit auto-creates
      the room + reverse exit (redit's exit machinery already does this).
      Needs a next-free-vnum strategy.
- [x] **`edplayer`** (player files) — done 2026-07-06: menu-driven editor
      (58+, matching `promote`'s tier) for level, experience, HP/max HP,
      attributes, gender, title, load room, and handedness -- an admin
      superset of `promote`. Works on any player by exact name, online or
      offline (`player_load_admin()`, not account-scoped). Unlike `edroom`
      the working copy is a DB snapshot, not a live pointer (players
      aren't kept resident like rooms are) -- (S)ave writes it back to the
      DB and, if the target happens to be online right now, syncs their
      live `being_t` too (no relog needed), matching `promote`'s own
      online-target courtesy. New `player_load_admin()` / `player_set_
      gender_by_name()` / `player_set_handed_by_name()` / `player_set_
      appearance_by_name()` in `player_repo.c`. `smoke_test_edplayer.py`
      (gate, every field, save-persists via reconnect, live sync to an
      already-connected session, and discard-truly-discards) + help topic.
- [ ] **`edaccount`** (accounts) — menu-driven: rename, password reset, list chars.
- [x] **wiznews** — done 2026-07-05: an immortal-only (51+) news channel like
      `news`; `edwiznews` posts items that concern immortals. Parallel to
      news/ednews.
- [x] **ed* rename** — done 2026-07-05: redit→edroom, hedit→edhelp,
      addnews→ednews (command names, help topics, tests, editor prompts).
- [ ] **Diseases** — modest list affecting players (immortals immune);
      pulse-driven affect/tick, cure path TBD (`disease.h` for inspiration).
- [ ] **News follow-ups** — edit/delete existing news in-game (addnews only
      creates); show unseen news at login (per-player last-seen).
- [ ] **redit Extra Descriptions** — keyword extra descs (`roomextra` table
      exists): list/add/edit/delete + delete-all (Sneezy redit items 6 & 10).
- [x] **Door mechanics** — done 2026-07-06: `open`/`close <direction>`
      (`cmd_open.c`), movement blocking on a closed door (`cmd_move.c`:
      "The door is closed."), and secret exits hidden from `look`'s
      Obvious-exits line and `exits` (still walkable if you know the
      direction). New `EXIT_COND_CLOSED`/`_LOCKED`/`_SECRET` bit constants
      in `room.h`. Door/condition state is per-exit, NOT mirrored to the
      reverse exit -- matches how `edroom`'s own auto-created reverse
      exits already work (independent door state per direction), not an
      oversight. `open` refuses a Locked door; unlock/lock commands are
      still deferred (need a key, which needs objects). `smoke_test_
      doors.py` + 3 new help topics (`open`, `close`, updated `exits`).
- [x] **Positions polish** — done (Session 43): a defender who isn't
      standing (sitting/resting/sleeping/any lower rung) takes a flat
      +15 hit-roll bonus against them in `combat_strike()` -- attacking
      only auto-stands the ATTACKER (cmd_attack.c), so this stays in
      effect for as long as the defender chooses to stay down.
- [ ] **Personalized immortal log messages (57+)** — per-immortal flavor on
      log lines (`log.h` LOG_JESUS/LOG_PEEL/LOG_LOW inspiration).
- [x] **`<d>` bold color tag** — done (Session 43, user: "investigate <d>
      and $$g tags from sneezy and implement in tobin"). Sneezy's `<d>`/
      `<D>` is a standalone BOLD toggle (`\033[1m`), distinct from the
      existing R/G/B/... tags (which already bundle bold into their own
      bright/uppercase variant) -- `<d>` stacks bold onto whatever color
      is already active, e.g. `<g><d>bold green<z>`. One-line addition to
      `colorstring.c`'s tag table.
- [x] **`$$g`/`$g` ground-surface token** — done (Session 43, same user
      request). Sneezy's `misc/show.cc` token, substituted in an object's
      description with the room's ground-surface word (`describeGroundType()`,
      misc/create_rooms.cc): "street" (city), "road", "water" (ocean/river),
      "mud" (swamp), "sand" (beach), "floor" (indoors flag), else "ground".
      New `room_ground_type()` (room.h/room.c, same sector-substring-
      bucketing style as `sector_color()`) + `obj_apply_ground_token()`
      (obj.h/obj.c), wired into both `long_descr` display sites in
      cmd_look.c (`look <object>` and the room-floor listing). Dropped the
      original's weather-prefix component ("snow-covered ground", "rain-
      slick street") -- Tobin has no weather system yet. Not present
      anywhere in the currently-migrated obj/objextra data (verified before
      building -- zero real usages), so this is forward-looking
      infrastructure for future hand-authored descriptions, not activating
      existing content.
- [x] **Time/day/date system** — done (Session 43, user: "implement
      time/day/date system from sneezys example"). Ported from Sneezy's
      `GameTime` class (sys/gametime.{h,cc}): 28-day months, 12-month
      years, the same weekday formula `(28*month + day + 1) % 7`, the same
      noon/midnight/new-month/new-year world announcements. New
      `gametime.h`/`gametime.c` + `time` command. Session-only (starts
      fresh at boot, no persisted game-time table). Ticks on a pulse
      (~60s, the same cadence `zone_process_run()` already established)
      advancing 15 mud-minutes per tick, rather than the original's real-
      seconds-per-mud-hour formula. Dropped: the weather-driven sunrise/
      sunset/moon tracking (no weather system) and the personal real-
      time-zone-offset sub-feature of Sneezy's `time <difference>` (a
      separate feature, not part of the day/date system itself). Found
      and fixed a related latent bug while adding this: `pulse_register()`
      silently no-op'd past `MAX_PULSE_PROCESSES` (was 8, exactly filled
      by this addition) -- bumped to 16 and made the overflow case log an
      error instead of vanishing silently.
- [x] **Personal time-zone offset** — done (Session 43 continued, user:
      "is the time based upon time zones? if so, make the mud EST" then
      "in account creation, ask the character to choose a time zone based
      on machine time zone, so for PST set timezone -3, etc"). The mud
      clock itself is fictional (28-day months, pulse-driven) and has no
      real-world timezone; confirmed the VM/MariaDB *are* both already
      America/New_York (EST/EDT), so nothing needed changing there. This
      is the separate real-time-offset sub-feature explicitly deferred
      when the gametime system was added above, now ported from Sneezy's
      `CON_TIME` prompt (sys/connect.cc) and `time <difference>`
      (misc/info.cc doTime()): a new `CONN_GET_TIMEZONE` account-creation
      state (right after the color prompt) asks the offset in hours from
      the server's Eastern clock (e.g. Pacific enters -3), range -23..23,
      blank = 0; persisted to `account.time_adjust` (a pre-existing,
      previously-unused column from the original schema -- no migration
      needed). `time` (bare) now shows a second line, the real-world clock
      shifted by that offset; `time <difference>` re-sets it later. New
      `account_set_timezone()` (account.h/account_repo.c), new
      `tests/smoke_test_timezone.py` (9 checks, including that shifting
      the offset by 2 hours shifts the shown real time by exactly 2
      hours).
- [x] **Pager held-messages + colorized MORE prompt** — done (Session 43
      continued, user: "silence all messaging like youve done for the
      editors, but for pagination. also colorize the [ ENTER for more, Q
      to stop ] line like my example"). `descriptor_in_editor()`
      (descriptor.c) now also returns true while `page_len > 0` (mid-
      pager, e.g. reading `news`), so `descriptor_notify()` holds
      messages for catchup instead of interrupting a paginated read --
      same mechanism as the editors, one extra condition. Since `news` is
      mortal-accessible, `catchup` was widened from immortal-only to
      mortal-level (cmd_table.c) -- otherwise a mortal held mid-pager
      would have no command to retrieve it with. The MORE prompt itself
      is now colorized and on its own line: `\r\n<c>[ <C>ENTER<c> for
      more, <C>Q<c> to stop ]<z>` (was a plain, uncolored trailing
      fragment). New `tests/smoke_test_pager_held.py` (5 checks).
- [x] **Fixed: regular-intensity color tags didn't clear a preceding
      bold** — done (Session 43 continued, user: "colorized pagination is
      incorrect. the intention was to highlight the available
      command/keys in bright. the rest regular"). Root cause in
      `colorstring.c`'s `ansi_for_tag()`: lowercase tags emitted a bare
      `\033[36m`-style code with no intensity reset, and SGR bold (`1`)
      and color are independent parameters that most terminals leave
      stuck on until explicitly cleared -- so `<C>ENTER<c>` (bright, then
      regular) rendered everything bright, since the plain `<c>` never
      actually turned bold off. Every lowercase color tag now leads with
      `0;` (`\033[0;36m`), forcing a full attribute reset before applying
      the color, so a regular tag really is regular regardless of what
      came before. This is a general color-engine fix, not pager-
      specific -- every `<x>` tag in the game benefits. Updated the
      hardcoded expected byte sequences in six existing smoke tests
      (`smoke_test_color.py`, `smoke_test_help_format.py`,
      `smoke_test_notify.py`, `smoke_test_say.py`,
      `smoke_test_sector_color.py`, `smoke_test_pager_held.py`) to match
      the new `0;`-prefixed codes; all still pass except
      `smoke_test_sector_color.py`, which failed for an unrelated reason
      (a stale hardcoded sector expectation for room vnum 100, flagged
      separately, not a regression from this fix).
- [x] **Three `look` bugs, found and fixed together** — done (Session 43
      continued, user reported all three against real seeded mob vnum
      33271, "a dirty refuse hauler"):
      1. **Capitalization sometimes ignored** ("A lamppost is here." /
         "a dirty refuse hauler is here." -- inconsistent). Root cause:
         this mob's `short_desc` is authored with a leading inline color
         tag (`<o>a dirty refuse hauler<1>`), and `cap_first()`
         (cmd_look.c) blindly uppercased byte 0 -- which was `<`, a
         no-op, leaving the real letter untouched. Fixed to skip any
         leading `<X>` tag(s) before capitalizing.
      2. **Wrong name in `look <mob>`** ("You look at man dirty refuse
         hauler." should read "You look at a dirty refuse hauler.").
         Root cause: `look_at_target()` displayed `thing_t.name` (the
         raw keyword-match list, e.g. "man dirty refuse hauler") instead
         of `short_descr` for mobs -- a PC's `name` IS its proper name,
         but a mob's `name` is just matching keywords. Fixed to use
         `short_descr` for mobs (uncapitalized -- it's mid-sentence
         here, not a cap_first() site).
      3. **Truncated long description** ("increase the buffer size so i
         can read the entire string"). Root cause: `BEING_APPEARANCE_LEN`
         was 256, sized for `player.appearance`'s real varchar(255)
         column, but shared with `mob.description` (mediumtext, real
         seeded max ~1200 chars) -- silently cut off mid-sentence on
         load (mob_repo.c's snprintf). Bumped to 2048; PC-authored
         appearance text is unaffected (MariaDB itself truncates on the
         rare overflow past the real 255-char column, same as any other
         varchar overflow -- previously this buffer coincidentally
         enforced that limit earlier, now the DB does). Also bumped two
         downstream buffers (`show_attr_screen` in descriptor.c,
         `look_at_target`'s `out` in cmd_look.c) that hit
         `-Wformat-truncation` once the source could be much longer.
      New `tests/smoke_test_look_capitalization.py` (6 checks, all
      against the real vnum 33271 mob rather than synthetic fixtures,
      since all three bugs only manifest on real authored content).
- [x] **`scan` ignores linkdead characters** — done (Session 43
      continued, user: "scan should ignore linkdead chars").
      `cmd_scan.c`'s room-occupant loop had no linkdead check at all
      (unlike the room-floor `look` listing, which shows a linkdead PC
      tagged "(linkdead)" but still visible, and combat's
      `combat_find_room_target()`, which already excludes them from
      being attacked) -- scan simply never checked. Fixed to skip a PC
      with no live `desc`, same "not a real target" treatment as combat.
      Extended `tests/smoke_test_scan.py` with a 6th check (abrupt
      disconnect -> still-present-but-unscannable).
- [ ] **Make `smoke_test_limbs.py`/`smoke_test_limbs_cmd.py`
      deterministic** — found while chasing an unexpected sweep failure
      (Session 43 continued): both rely on real combat RNG to eventually
      cross a limb status tier within a fixed number of rounds, but with
      `LIMB_MIN_MAX_HP` (15) and damage landing on a random one of 13
      limbs each hit, reliably crossing a tier in the test's round
      budget is statistically marginal -- confirmed via `hurtlimb` that
      the underlying mechanism itself is fine, this is a test-design
      gap. `hurtlimb <target> <limb> <hp>` (cmd_hurtlimb.c, added
      Session 42 for exactly this) already lets a test set a limb's HP
      directly instead of waiting on the dice -- migrate both tests to
      use it instead of a real fight.
- [x] **Get/drop item logging for dispute research** — done (Session 43
      continued, user: "anytime a char gets an item or drops an item i
      want those logged into the game log so we can research disputes
      with log search. these should not be reported via any log type,
      just inserted into the game log"). `LOG_SILENT` (log.h) already
      existed for exactly this -- recorded to the file, never echoed to
      online immortals -- so this needed no new mechanism, just call
      sites: `cmd_object.c`'s `cmd_get()` (both the plain and the
      get-from-container branches) and `cmd_drop()` each now call
      `game_log(LOG_SILENT, "%s gets/drops %s (vnum %d) in room %d", ...)`.
      Verified reachable via `log search` and confirmed a same-room
      online immortal sees nothing from it (the ordinary room-broadcast
      message is separate and still fires normally). Found and fixed a
      duplicated-helper bug while working nearby: `cap_first()`'s
      leading-inline-color-tag fix (from the earlier `look` bugfix) only
      ever landed in cmd_look.c's own copy -- cmd_scan.c and
      cmd_object.c each have their own independent copy of the same
      function, still with the old bug. Fixed both. New
      `tests/smoke_test_getdrop_log.py` (6 checks).
- [ ] **Mob AI: wandering + mob actions** — user: "in pulse, make sure
      that mob actions click and mobs that can wander will do so, look
      at mob ai from sneezy". Needs investigation of Sneezy's mob AI
      (misc/mobact.cc or similar -- movement, `actions` bitfield on the
      `mob` table already loaded-but-unused per mob_repo.h's comment,
      wander-if-no-players-nearby / stay-in-zone-bounds rules) and a new
      pulse-driven `mob_ai_tick()` alongside `zone_process_run()` /
      `gametime_tick()`. Likely needs the `mob.actions` bitfield
      actually read (mob_repo.h currently only loads
      name/short_desc/description/level/hpbonus/sex -- explicitly noted
      as "deferred to a future edmobile/AI session").
- [ ] **Cleaner mobs clean up randomly** — user: "i want cleaner mobs to
      clean up randomly, i believe this is also in mob ai". Likely a
      specific `actions` bit (Sneezy's ACT_CLEANER or similar) checked
      by the same mob AI pulse above -- a cleaner-flagged mob
      periodically removes/consumes loose trash objects in its room.
      Bundle with the mob AI item above rather than building a separate
      pulse for it.
- [ ] **Weapon-aware combat messaging + hit/dam bonuses** — user: "when
      in combat wielded items should modify messaging for example wield
      sword, you slice instead of hit. This should apply to all weapon
      types and add or subtract any hit bonuses placed on the weapon".
      Two gaps, confirmed by reading combat.c/obj.h: (1)
      `combat_strike()` is entirely attribute-based (STR/DEX only) and
      never looks at what's wielded at all -- the "You hit %s's %s for
      %d damage!" message (combat.c:122) is hardcoded regardless of
      weapon. (2) There is no weapon-subtype data model to key a verb
      off of -- `obj_category_t` only has one collapsed `OBJ_CAT_WEAPON`
      bucket (obj.h), the original's per-weapon-type itemTypeT distinction
      was flattened away during the port. (3) The `objaffect` table
      (vnum, type, mod1, mod2 -- presumably a Diku-style APPLY_* enum,
      e.g. hitroll/damroll) exists in the DB but is read by NO code
      anywhere in the port yet -- "hit bonuses placed on the weapon"
      needs this wired up from scratch (a new obj_repo function to load
      a vnum's objaffect rows, then combat_strike() applying them to
      hit_roll/dmg for whichever weapon is actually wielded). For the
      verb itself, likely cheapest path is a keyword-substring bucket
      function (same style already used for `sector_color()`/
      `room_ground_type()` in room.c) matching the weapon's name/
      short_descr against "sword"->slice, "axe"->chop, "mace"/"hammer"->
      bludgeon, "dagger"/"knife"->stab, "spear"/"pike"->pierce, bare-
      hand/unrecognized->hit, rather than restoring the original's full
      item_type subtype column. Needs a way to know what's wielded --
      check being_t's equipment/wield-slot fields (see the hold/wield/
      switch rework, cmd_object.c) for the hookup point.
- [x] **Persist the game clock across boots** — done (Session 43
      continued, user: "make time save so it continues on from boot to
      boot"). Reused the exact `game_config` key/value pattern
      multiplay.c already established: new `gametime_load()`
      (gametime.c, called from main.c right after `multiplay_load()`)
      restores hour/minute/day/month/year from five `game_config` rows;
      `gametime_tick()` now calls a new `gametime_save()` before every
      return path (there are several, one per rollover stage), so a
      crash or unclean restart never loses more than one tick (~60s) of
      progress. Caught a real bug while writing this: `db_query()`'s
      custom format parser only recognizes `%i` for integers, not
      libc's `%d` -- using `%d` would have silently failed every save
      (falls through to the "bad format specifier" error path). Verified
      end-to-end: set the clock via a real tick, restarted the live
      server, confirmed `time` resumed at the persisted value instead of
      resetting to the 8:00 AM default. New
      `tests/smoke_test_gametime_persist.py` (3 checks, verifies the
      `game_config` row matches `time`'s live output rather than
      requiring an actual server restart mid-test).
- [ ] **Half-hour real-time tick (blank line, no message)** — user:
      "every hour on the half hour send a blank line of uinput to the
      game so a tick becomes apparent to the player without any
      messages". Real wall-clock time (not the fictional mud clock
      above) -- a new pulse check (alongside `zone_process_run()` /
      `gametime_tick()` in main.c) using `time(NULL)`/`localtime_r`,
      firing once when the real minute crosses :30 past the hour (guard
      against firing every pulse for the whole minute, same one-shot-
      per-boundary concern `gametime_tick()`'s hour/day/month rollovers
      already handle). Sends just "\r\n" to every connection -- no log
      line, no [TYPE] tag, presumably still via `descriptor_notify()` so
      it's held like any other broadcast for someone mid-editor/pager
      rather than corrupting their screen.
- [ ] **Mobile_Attitude (mob AI emotional/opinion system)** — user:
      "class Mobile_Attitude in sneezy should be implemented into tobin.
      mobs should react to good vs evil and react accordingly". Read
      Sneezy's own docs (`sneezymud-master/docs/systems/critical/
      14-monster-ai-behavior.md`, source in misc/monster.cc/.h,
      misc/mobact.cc, misc/opinion.cc): `Mobile_Attitude` models four
      0-100 emotional attributes per mob -- suspicion, greed, malice,
      anger (not literally "good/evil") -- with an aggression formula
      `4*anger + 5*malice >= 450` (or the `ACT_AGGRESSIVE` flag), a
      `pissed()` minor-annoyance check, and hate/fear opinion bitfields
      keyed by sex/race/individual-char/class/vnum. Note: neither this
      class nor Tobin's current `being_t` (include/being.h, checked --
      no `alignment` field exists) has a literal good/evil alignment
      stat; a PC-alignment-driven reaction would need that stat added
      first as a separate, smaller piece before mob opinion can react to
      it. Natural pairing with the "Mob AI: wandering + mob actions" item
      above -- likely the same pulse-driven `mob_ai_tick()`, since
      opinion/aggression checks and wander/action checks both run per-
      mob per-pulse in the original.
- [x] **`idea` command (feature requests)** — done (Session 43
      continued, user: "add an idea command so a player can request new
      features, should work the same as reporting a bug also add an
      idea log message"). Direct mirror of the existing `bug`/`delbug`
      pair: `idea <text>` files one (stored with name + date), bare
      `idea` lists outstanding ones for immortals, `delidea <id>` (59+)
      removes a handled one. New `idea` table (db/sneezy/idea.sql,
      copies bug.sql's shape) + `idea_repo.{h,c}` (copies bug_repo's) +
      `cmd_idea.c` (copies cmd_bug.c's), new `LOG_IDEA` value in
      `log_type_t` (log.h, inserted before `LOG_TEST` so
      `LOG_SEVERITY_DEFAULT`'s derived bit width adjusts automatically)
      added to `cmd_setsev.c`'s toggle list. New
      `tests/smoke_test_idea.py` (9 checks, mirrors smoke_test_bug.py).
- [ ] **Drink/sip commands** — user: "add a drink/sip code from
      sneezymud and implement here". From-scratch, not a small addition:
      checked `obj.h` -- `OBJ_CAT_DRINK` exists as a category bucket but
      there's no liquid-type/capacity/current-amount modeling on obj
      instances at all yet, and `being_t` (being.h, checked) has no
      thirst/hunger stat either (the `nutrition` DB column referenced in
      player_repo.c's INSERT is vestigial -- never read or decremented
      anywhere). Needs, roughly: liquid type + capacity + fill-amount
      fields on drink-category objects (obj.h/obj_repo.h), a thirst (and
      maybe hunger, since Sneezy ties both together) stat on being_t,
      `drink`/`sip``/`fill``/`pour` commands (sip = small amount + no
      "full" message, matching the original's distinction), and messages
      for empty-container and over-full-from-drinking-too fast cases.
      Reasonable to scope drink/sip alone first and defer fill/pour.
- [ ] **`purge` command (51+, with a 58+ `purge linkdead`)** — user: "add
      a purge command that is 51+ that will purge the contents of a room,
      add a linkdead argument that a 58+ god can purge the game of all
      linkdead characters". Two distinct pieces:
      1. `purge` (bare, 51+): clears the caller's current room of
         everything except players -- iterate `room_t.base.stuff_head`,
         `obj_destroy()` (obj.h) every `THING_OBJ`, presumably
         `being_destroy()` (being.h) every `THING_MOB` too (mirrors the
         original's room-purge convention -- confirm scope with the
         user: mobs included or objects only?).
      2. `purge linkdead` (58+): a game-wide sweep, not room-scoped --
         needs a way to enumerate every linkdead PC across ALL active
         rooms, not just the caller's. Checked `world.h`: no existing
         "iterate every active room" function is exposed (only
         `world_get_room(vnum)`, a single lookup) -- this needs either a
         new world-level room-registry iterator, or a parallel tracked
         list of linkdead beings maintained wherever a PC actually goes
         linkdead (descriptor.c, same place that currently just leaves
         them in their room -- see the linkdead-persistence feature).
         `being_destroy()` presumably just frees the in-memory being_t
         (their DB player row stays intact so the account can log back
         in fresh) -- confirm that's the right semantics before wiring
         it to a bulk "purge everyone" command, since a mistake here is
         destructive across the whole game, not just one room.
- [x] **`test` command (58+): show the currently-running smoke test** —
      done (Session 43 continued, user: "add a test command that will
      list whatever smoke test is currently running 58+"). The
      `@test <name>` / `@test done <name>` loopback-only hook
      (descriptor.c) already existed but was fire-and-forget (only
      `game_log(LOG_TEST, ...)`, a transient log line) -- added
      `log_test_set_running()`/`log_test_clear_running()`/
      `log_test_current_name()` (log.h/log.c) so the hook now also
      persists the name, and a new `test` command (TEST_MIN_LEVEL=58)
      just prints it ("No smoke test is currently running." if empty).
      Doesn't touch the hook's existing localhost-only security gate.
      New `tests/smoke_test_test_cmd.py` (3 checks) -- relies on
      sweep.sh running tests strictly sequentially so there's no other
      test's announcement to race with.

## Small near-term gameplay follow-ups

- [x] **XP on kill** — done (Session 43): `combat_defeat()` awards
      `loser->progress.level * 50` XP (placeholder formula, same precedent
      as other placeholder combat/growth numbers) via the already-existing
      `progress_add_xp()`, and saves it. Only for a non-immortal PC winner
      -- covers a normal defeat and a decapitation, but not an immortal's
      `cmd_kill` instakill (that winner is always an immortal, who doesn't
      need XP).
- [ ] **Mid-fight persistence** — HP and limb HP are only saved at defeat; a
      mid-fight disconnect reloads at last-saved values.
- [ ] **`player_save()` + a `save` command** (user request, 2026-07-07) — a
      single function that persists everything about a character in one
      call (progress/attrs/inventory, and by extension HP/limb HP once
      those are added to it), plus a player-invokable `save` command that
      calls it on demand. Mirrors the original's real `TBeing::doSave()`
      (`cmd/cmd_save.cc`) -- a genuine port, not a Tobin invention. This
      would consolidate the currently-scattered save-at-mutation-point
      calls (`player_attrs_save`/`player_progress_save`/
      `player_inventory_save`, each called separately from `set`,
      `cmd_mortal.c`, `combat_defeat()`, and every object command) into one
      place, and directly close the "Mid-fight persistence" gap above.

## Blocked on Objects / Mobs (Phase 2C/2D/2E)

### >>> NEXT UP (work session): `edobject` and/or `edmobile` <<<

Objects (2C) and Mobiles (2D) are BOTH done as of 2026-07-07 (Sessions 34
and 35) -- see STATUS.md's decision rows. Both editors were deliberately
deferred to their own session(s) each (designing a system and its editor
at once serves neither well); the user already said they want wireframes
drafted (not provided) for both, from Sneezy's real menus:
- `edobject`: `create_objs.cc`'s `update_obj_menu` (21 fields), covering
  the real fields now in `obj_t`/the `obj` table: name/short/long/action
  desc, category (was `type`), wear_flag, action_flag, val0-3, weight,
  volume, price, can_be_seen, max_struct/cur_struct, material, decay,
  max_exist.
- `edmobile`: `create_mobs.cc`'s `send_mob_menu` (30 fields), covering the
  `mob` table's real columns -- note Tobin's `being_create_mob()` only
  uses 6 of ~40 columns today (name/short_desc/description/level/hpbonus/
  sex), so this editor's scope decision (edit only what's wired up, vs.
  edit the full row and leave most fields inert until AI/combat-stats work
  lands) is itself worth raising with the user before drafting the
  wireframe.
Same menu-driven working-copy pattern as `edplayer`/`edroom` either way
(see [[editors-menu-driven]]).

- [x] **Objects (2C)** — done 2026-07-07: `obj_t` (16-category collapse,
      not the originally-estimated ~15 -- close enough, see obj.c's
      lookup table comment), DB load (`obj_repo.c` reads the existing
      upstream-seeded `obj` table directly, no new prototype table),
      `oload`, get/drop/inventory/wear/remove/equipment, persistence
      (`player_inventory.sql`, carried/worn/held only), drop-equipment-
      on-death (`combat.c`), equipment wired to the existing 13-limb enum
      (no second enum, per this item's own original constraint).
      `smoke_test_objects.py` + 7 new help topics + a news entry.
- [ ] **`edobject` (oedit)** — object editor (menu-driven, DB prototype
      rows in the existing `obj` table). See NEXT UP note above. Sneezy's
      `update_obj_menu` has 21 fields (STATUS / create_objs.cc).
- [x] **Mobs (2D)** — done 2026-07-07: a mob is just a `being_t` with
      `kind=THING_MOB` (no new struct -- matches the original's own
      `TMonster : TBeing`), DB load (`mob_repo.c` reads the existing
      upstream-seeded `mob` table directly), `mload`, and full combat
      integration (`combat_find_room_target()`/`combat_defeat()` widened
      to handle mobs, permanent removal on defeat -- `combat_process_run()`
      needed zero changes). `attrs`/`max_hp` are level-derived placeholder
      formulas, NOT the mob table's real 12-stat/hpbonus system (see
      STATUS.md). `smoke_test_mobiles.py` + a help topic + refreshed
      `attack`/`kill`/`look` topics + a news entry. Still unlocks the real
      kill-XP economy once `progress_add_xp()` gets wired up (separate
      follow-up, see the small-gameplay-follow-ups list above).
- [ ] **`edmobile` (medit)** — mob editor (menu-driven, DB prototype rows
      in the existing `mob` table). See NEXT UP note above. Sneezy's
      `send_mob_menu` has 30 fields (STATUS / create_mobs.cc).
- [ ] **Mob AI / aggression** — `ACT_AGGRESSIVE` (and the rest of the
      `actions`/`affects` bitmask columns) is completely unused -- mobs
      are reactive-only today (never act until attacked). A real AI/pulse
      tick is a separate, larger follow-up.
- [~] **>>> Zones / zonefiles (2E) <<<** (user 2026-07-07) — in progress.
  - [x] **Part 1: convert zonefiles -> DB** — done 2026-07-07: `db/import-zones.py`
        parses the upstream `lib/zonefiles/*` into `db/sneezy/zone_reset.sql`
        (a new `zone_reset` table: zone_nr, cmd_no, command, if_flag, arg1-4,
        comment). 35,922 reset commands imported (M 11314 -> mob to room,
        O 6625 -> obj to room, E/D/G/P + Sneezy-specific opcodes stored too).
        Sneezy's `?`-conditional (6750x) and one stray `Wrench` skipped.
        Auto-loaded by `apply-tobin-schema.sh`. Data-only so far (no execution).
  - [x] **Part 2: execute resets** — done (Session 43, user: "zonefiles are
        not loading? i dont see anything in rooms or mobs wandering
        around"). New `zone.c`/`zone_repo.c`: covers the highest-value
        opcode subset -- M (load mob), O (load obj on the ground, boot-time
        only -- matches the original exactly), E (equip the last-loaded
        mob -- placement derived from the object's own wear_flag via the
        existing wear_slot_for_flag(), not the original's arg3 slot index,
        which has no Tobin-limb equivalent), G (give the last-loaded mob a
        carried item), P (place an item inside the last-loaded container),
        D (door open/closed/locked). Together ~84% of all real rows. The
        rest (Y/X/Z object "sets", A random-room, V/H/F/T/L/K/C/R/I/J) are
        skipped -- they need subsystems Tobin doesn't have yet (mob AI,
        object sets, loot tables, traps, grouping/charm/mounts); skipping
        one doesn't break the rest of a zone's chain, only `if_flag`-gated
        rows depending on it. **"Wandering" mobs specifically still needs a
        separate mob-movement/AI system** -- this only POPULATES rooms, it
        doesn't move mobs around afterward (see Mob AI/aggression below).
        Runs the FULL reset once at every process start (`zone_boot_all()`,
        main.c) -- both a cold boot AND a copyover-resume, since neither
        preserves room/mob/object state today (only player connection info
        survives a copyover, see cmd_copyover.c -- confirmed by reading it
        before building this, since the user flagged the copyover question
        directly). Then tops up periodically per-zone on its own `lifespan`
        (minutes) via a ~60s pulse tick. New immortal/builder command
        `zonereset <zone>` force-runs a zone's reset on demand (also the
        test hook, since waiting on a real lifespan timer isn't practical
        for a smoke test). Known simplification: no world-wide max_exist
        cap tracking (only a per-room cap, arg2) -- see the original's
        stat_mobs/stat_objs bookkeeping for what that would need.
        `smoke_test_zones.py` (verifies both a REAL seeded zone actually
        populated a real room, and `zonereset`'s M/E/G/P/D/unhandled-Y
        behavior via a sandbox zone).
  - [x] **Zone identity/ownership** — done (Session 43, user: "add
        identity to zones... builder gets assigned a zone then... a
        51-54 wants to edit gets rejected except for those assigned to
        that zone"). New `zone_owner` table (many-to-many: a zone can have
        multiple builders, a builder can own multiple zones). New
        `zone_can_edit()` (zone.h): 55+ edits any zone; a builder (51-54)
        only a zone they're assigned to; a room with NO zone (`room.zone`
        NULL) is unrestricted for everyone, since the boundary is per-
        zone. Wired into `edroom` and `edzone` (below) -- the only content
        editors that exist yet; **apply the same `zone_can_edit()` check
        to edobject/edmobile when those are built.**
  - [x] **Part 3: `edzone`** — done (Session 43, user pivoted from a one-
        shot `zoneassign` command to "make an edzone command to have a
        menu driven editor function like edroom etc"). Menu-driven, same
        snapshot-working-copy shape as `edplayer` (a zone isn't kept
        resident in memory like a room): name/enabled/lifespan/vnum range
        are Save/Quit-gated; assigning/un-assigning a builder (selecting
        an already-assigned name un-assigns them, same toggle as before)
        applies immediately, not deferred to Save; an `R`eset-now action
        force-runs the zone. Gated the same as edroom: 51-54 needs
        `zone_can_edit()` to pass for that zone_nr, 55+ always. Editing
        individual M/O/E/G/P/D reset-command rows is explicitly OUT of
        scope for this pass (user confirmed) -- still a future follow-up.
        Also kept a `zone reset <n>` one-shot shortcut (user: "keep zone
        reset as a quick shortcut") and added `zone list` (user: "dont
        forget a zone list so we can see whats been assigned and to
        whom") -- both in `cmd_zone.c`, paginated. `smoke_test_zone_identity.py`
        + `smoke_test_edzone.py`.
- [x] **Containers holding sub-items** (`put <item> <container>` / `get
      <item> <container>`) — done 2026-07-09 (Session 39, work). `cmd_put` +
      `cmd_get`'s two-arg form move items in/out of a container (carried/worn/
      floor); `look <container>` lists contents when open; `open`/`close` now
      act on containers via `CONT_*` bits in `val[1]`; weight capacity (`val[0]`)
      enforced. `smoke_test_containers.py`, news/wiznews, help topics.
      DEFERRED (see STATUS decisions): carried-container *contents* persistence
      (saved loose to avoid loss, reload un-nested — needs a `player_inventory`
      parent column); lock/unlock+keys (pairs with the doors/keys item below).
      **This unblocks Zones Part 2's `P` opcode** (put obj in container).
- [ ] **Keys unlocking doors** — `unlock`/`lock <direction> with <item>`,
      matching a KEY-category object's val[0] against the exit's key
      requirement. The object system this was blocked on (Session 31) now
      exists -- separate follow-up, not built this pass.
- [ ] **Shops + money** — shopkeeper buy/sell, shop editor, GOLD-COIN-ONLY
      currency (shop tables already in the seed DB). Needs objects.
- [ ] **Player-state logging** — log get/drop + pfile changes so `log search
      <name>` tells a player's story. Needs objects; design a uniform helper.
- [ ] **Body types** — `body.h` body-type concept (creatures have different
      limb sets). Pairs with mobs + limbs.

## Bigger systems (need design / a decision)

- [ ] **Classes** — warrior/cleric/thief/monk/mage, chosen at creation, shown
      in score/who. Stat affinities (user spec): mage high INT / low STR;
      warrior high CON+STR, dump CHA+WIS; thief high DEX / low STR; cleric
      high WIS / low STR+DEX; monk STR+CON / low CHA. Port Sneezy's class
      tables/formulas onto Tobin's 6-stat set. (Ignore DISC_* for now.)
- [ ] **Races** — curated player list + open mob list; race stat bonuses from
      Sneezy's race tables.
- [ ] **Game balance layer + `gameedit` (60 ONLY)** — race/class PERCENTAGE
      bonuses; a level-60 live tuner in 0.1% increments, exact-word like
      `quit!` (never by typo), DB-persisted. Needs classes+races.
- [ ] **Limbs → `wearSlotT`** — reshape the 13-limb enum toward Sneezy's
      `wearSlotT` (back, wrist, hand vs finger, HOLD, EX_* for mobs). **Open
      question:** keep the Tobin-added `genitalia` slot? include HOLD/EX for
      players or mobs only? Own focused pass; touches combat/score/limbs.
- [ ] **Limbs.h gap review** — weighted hit locations, PART_* flags, etc.
      (overlaps the wearSlotT item).
- [ ] **TobinMUD identity + DB rename** — rename DB `sneezy` → `tobin`
      (init-db.sh, config defaults, docs); rebrand credited as "Derivative of
      SneezyMUD and DikuMUD". Wide but mechanical; coordinate on all boxes.

## Chores / infra

- [x] Create the `mud` user on the work box (db.kullit.com) — done
      2026-07-06, along with a dedicated SSH keypair for it; root access
      to that box is retired (see ENVIRONMENT.md).
- [x] Auto-restart `tobin_c` if it dies — done 2026-07-05: the user added a
      crontab to the `mud` user that checks whether the MUD is running and
      starts it if not. (Deploys still `pkill; sleep 1; restart` fast, before
      any cron tick, so there's no double-launch.)
- [ ] Install a MUD client (Mudlet, for ANSI color) on the Windows machines.
- [ ] **docs/systems review** — read `sneezymud-master/docs/systems` for how
      the original stored things; apply the lessons. RULE: prefer the DB.
- [ ] **Systems documentation** — a doc/systems README for the TobinMUD base.
- [ ] **Function comment headers sweep** — a header comment per function (what
      it's for + cross-refs to what it affects / depends on), then a habit.
- [ ] **STATUS.md's "Module port status" table is stale** — the `cmd/`
      row still says "11/66 ported" (list: look/who/score/quit!/color/
      attack/kill/say/limbs/help/wizhelp); there are 40+ `cmd_*.c` files
      now. Needs a dedicated audit pass, not a quick fix mid-feature.

## Reference material (Sneezy enums, provided 2026-07-04)

Upstream enums the user pasted, staged for the features above (kept in this
conversation and in `sneezymud-master`): `positionTypeT` (done), `prompt_mesg`
(health strings), `classInfo` (classes), `body_flags` (limb conditions),
`wearSlotT` (limbs), `heraldcodes`/`heraldcolors` (immortal color/heraldry),
`doorTypeT`/`doorIntentT`/`doorUniqueT` + `exit_bits` (door mechanics).

## Deferred decisions (blocked on choosing, not on code)

- [ ] Which ~8-10 `disc/` disciplines to keep; which 1-2 `task/` professions.
- [ ] Hospital mechanic for destroyed limbs (only cure now is death/respawn).
- [ ] Whether the destroyed-limb hit penalty scales with count (flat -15 now).
- [x] Immortal-vs-immortal `kill` guard — done (Session 43): `cmd_kill.c`
      refuses to instakill a PC target whose TRUE rank (true_level if set,
      else level -- protects a target who's toggled mortal via `immort`
      too) is equal to or higher than the attacker's.

## Standing rules (learned)

- Superseded 2026-07-10 (user: "interrupt full sweeps to test new code, as
  a habit"): kill an in-progress full sweep to deploy/test new code now,
  don't wait for it. Full sweeps only run right before a repo push (see
  below), so one in flight is disposable. (Old rule: never hot-deploy
  mid-sweep, from an incident where it caused unrelated flakes — no longer
  applies now that sweeps aren't run casually between changes.)
- Every player-facing change gets a `news.sql` entry (no numbers). See CLAUDE.md.
- Every new `db/sneezy/*.sql` file MUST use `CREATE TABLE IF NOT EXISTS`,
  never an unconditional `DROP TABLE IF EXISTS` + `CREATE TABLE` — the
  latter silently wipes live data every time `apply-tobin-schema.sh` re-runs
  it (which it always does; that script re-applies every file, every time).
  Burned us once for real (Session 36: `player_attrs.sql`/
  `player_progress.sql` wiped ~1338 players' progress this way).
