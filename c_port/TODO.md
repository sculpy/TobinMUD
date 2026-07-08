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

- [ ] **`look <object>` doesn't work** — bug: `cmd_look.c`'s `look <name>`
      (`look_at_target()`) only matches a `THING_PC`; it needs to also find
      a `THING_OBJ` (room floor, carried, or worn/held) and show its
      `long_descr`/action description plus a condition line derived from
      `cur_struct`/`max_struct` (e.g. "is in perfect condition" down to
      "is falling apart"). A real gap left by the Objects (2C) pass.
- [ ] **`help color`/`help who`: list every color tag + mention `<N>`** —
      `help color`'s topic should enumerate all the `<x>` color tags
      (today only the separate `help colors` topic does that) and mention
      `<N>`/`<n>` name substitution (currently documented only under
      `title`); enrich `help who`'s topic the same way (titles shown in
      `who` can use both tricks).
- [ ] **`bamfin`/`bamfout`** — classic wizard commands: an immortal sets
      their own custom arrival ("bamfin") / departure ("bamfout") message
      shown when they `goto`, plus a customizable regular-movement message
      template (e.g. "Jesus drags his cross in from the <direction>.").
      Needs new persisted per-player fields (likely new `player` columns)
      wired into `cmd_goto.c`'s and `cmd_move.c`'s existing room-echo calls.
- [ ] **Colorize copyover messages** — `cmd_copyover.c`'s player-facing
      reboot messages are plain text; tint them (the standing "colorize
      tastefully" habit).
- [ ] **`@set` currently just falls through to Huh?!** — `cmd_dispatch()`
      doesn't special-case a leading `@` the way it already does `'`
      (say) and `;` (wiznet). Since a separate `@set` command isn't
      planned, make `@set` dispatch as an alias for `set` (same
      special-case mechanism `cmd_table.c` already uses for `'`/`;`).
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
- [ ] **Editors must get ABSOLUTE quiet** — audit every broadcast/echo
      call site (`game_log`, death taunts, wiznet, system, link-loss,
      newbie, etc.) to confirm every single one routes through
      `descriptor_notify()` (held until the editor exits, replayed via
      `catchup`) and NEVER `descriptor_send()` directly, whenever the
      target might be mid-edit. Treat any direct-send broadcast path
      found during the audit as a bug to fix, not just document.
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
- [ ] **`hit` command (real combat, never instakill)** — a new command
      that always engages the normal multi-round combat process (like a
      mortal's `attack`), even for an immortal caller -- lets an immortal
      actually fight something instead of instakilling it. `kill`/`attack`
      keep their current behavior unchanged (instakill for immortals).
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
- [ ] **Positions polish** — a hit bonus vs. a non-standing target (the
      original makes a sitting/sleeping foe easier to hit); we deferred the
      combat-formula change when Positions landed.
- [ ] **Personalized immortal log messages (57+)** — per-immortal flavor on
      log lines (`log.h` LOG_JESUS/LOG_PEEL/LOG_LOW inspiration).

## Small near-term gameplay follow-ups

- [ ] **XP on kill** — `combat_defeat()` → `progress_add_xp()`, one-liner once
      a reward number is chosen.
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
- [ ] **>>> NEXT: Zones / zonefiles (2E) <<<** (user 2026-07-07) — the whole
      zone system: **zonefile reset commands so mobs AND objects auto-load
      into rooms** (and objects into mobs / onto the ground) at boot and on a
      periodic per-zone reset timer -- the automatic counterpart to the manual
      `oload`/`mload`. This also makes room-floor objects and mloaded mobs
      survive a restart (currently lost, see STATUS.md). Includes: the zone +
      zone-reset-command tables (already in the upstream seed DB -- verify),
      loading/executing reset commands (the original's `zone_reset`/`ZCMD`
      "M/O/G/E/P/D" opcodes), a reset pulse, and `zedit` (menu-driven, needs a
      wireframe). Port from Sneezy's zone reset machinery for inspiration;
      confirm the reset-command opcode subset + `zedit` wireframe with the user.
- [ ] **Containers holding sub-items** (`put <item> <container>` / `get
      <item> <container>`) — containers exist as objects (Phase 2C) and can
      be carried/worn, but can't hold anything yet. Natural small follow-up.
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
- [ ] Immortal-vs-immortal `kill` guard (can't slay equal/higher level).

## Standing rules (learned)

- Never hot-deploy while a regression sweep is running (the restart/copyover
  freeze makes unrelated tests flake — burned us twice).
- Every player-facing change gets a `news.sql` entry (no numbers). See CLAUDE.md.
- Every new `db/sneezy/*.sql` file MUST use `CREATE TABLE IF NOT EXISTS`,
  never an unconditional `DROP TABLE IF EXISTS` + `CREATE TABLE` — the
  latter silently wipes live data every time `apply-tobin-schema.sh` re-runs
  it (which it always does; that script re-applies every file, every time).
  Burned us once for real (Session 36: `player_attrs.sql`/
  `player_progress.sql` wiped ~1338 players' progress this way).
