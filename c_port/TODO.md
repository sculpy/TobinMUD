# Tobin — TODO

Last updated: 2026-07-06. Companion to STATUS.md, which holds the full
session log, decisions, and history — **this file tracks only what's NEXT.**
Completed items are pruned from here as they land (find them in STATUS.md).

All in-game editors are menu-driven, like character creation — see the
[[editors-menu-driven]] memory. The user provides a wireframe for each.
Editor commands are named **`ed<noun>`** (user 2026-07-05): `edroom` (rooms),
`edhelp` (help), `ednews` (news), `edwiznews` (wiznews); future
`edobject`/`edmob`/`edzone`/`edplayer`/`edaccount`. Read-only viewers keep
plain names (`news`, `wiznews`).

## Buildable now (no blocked dependencies)

Self-contained — no need for the object/mob systems. Keep working through
these; each ships with a smoke test + (if player-facing) a news entry.

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
- [ ] **Colorize room name + description by sector** — tint the room display
      by its sector type: the room NAME may be BRIGHT-colored, but the room
      DESCRIPTION must use only lowercase (dim) color codes. Per-sector color
      map; applies in `look`.
- [ ] **Editor `format` option** — add a `format` command to every menu-driven
      `ed*` editor that reflows long text bodies to the game's column width
      (word-wrap to the display width). Shared helper across editors.
- [ ] **`set` + `@set` commands** — examine Sneezy's `set` / `@set` (immortal
      field-setter on players/objs/mobs/rooms) and implement equivalents in the
      c_port. Confirm the intended field scope + level gate before building.

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
- [ ] **`edplayer`** (player files) — menu-driven editor for a player's
      level/attrs/hp/location, replacing one-off SQL. Admin superset of promote.
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
- [ ] **Door mechanics** — the door type + condition data now persists but
      nothing uses it: open/close/lock/unlock (`doorIntentT`), movement
      blocking on closed doors, secret exits hidden from look/exits. The key
      half (locks) needs objects — defer that.
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

## Blocked on Objects / Mobs (Phase 2C/2D/2E)

- [ ] **Objects (2C)** — `obj_t` (planned 15-category collapse), DB load,
      `oload`, get/drop/inventory, persistence, drop-equipment-on-death,
      equipment slots wired to the existing 13-limb enum (no second enum).
- [ ] **`oedit`** — object editor (menu-driven). Sneezy's `update_obj_menu`
      has 21 fields (see STATUS / create_objs.cc).
- [ ] **Mobs (2D)** — `THING_MOB`, `mload`, mob combat — unlocks the real
      kill-XP economy. `medit` (Sneezy's 30-field `send_mob_menu`).
- [ ] **Zone resets (2E)** — periodic respawn per zone. `zedit` (zone table
      already in the DB).
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

- [ ] Create the `mud` user on the work box (db.kullit.com).
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
