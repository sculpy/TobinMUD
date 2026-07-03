# Tobin — TODO

Last updated: 2026-07-02 (home session — VM setup). Companion to STATUS.md:
STATUS.md records what happened; this file tracks what's next. Check items off
here and log details there.

## In flight right now

- [x] Copyover 5-second warning — **deployed + verified 2026-07-03 (morning)**:
      all 14 copyover checks pass including the warning; full suite green.

- [x] `dnf upgrade` on the home VM — **done 2026-07-02** (647 packages, kernel
      7.0.14; root-caused the slowness to e1000 NIC emulation and switched to
      virtio-net, ~150x faster; VM bumped to 12 GB RAM / 4 CPUs). Rebooted,
      rebuilt, relaunched `tobin_c`, full suite re-verified.
- [x] Guest Additions after kernel update — verified responding on 7.0.14.
- [ ] Install a telnet/MUD client on the Windows machines that will connect
      (`Enable-WindowsOptionalFeature -Online -FeatureName TelnetClient`,
      or Mudlet for proper ANSI color).

## Environment / workflow

- [ ] Create the `mud` user on the work box (db.kullit.com) to match the home
      VM (user's stated plan, 2026-07-02).
- [x] Home/work sync: **done 2026-07-02** — private GitHub repo
      `github.com/sculpy/tobin-mud`, repo root at the top of the tree
      (`E:\New MUD` at home). Workflow: commit+push when leaving a location,
      pull on arrival. First work-side step: clone it on the work box
      (replaces the scp'd copy).
- [ ] Consider a systemd unit (or at least a start script) for `tobin_c` on
      the VM so it survives reboots without manual relaunch.
- [x] **`copyover` hot reboot (user idea, 2026-07-03)** — in-game
      Administrator+ (59) command reboots the server binary in place with
      zero disconnects (fd inheritance across exec). Deploys are now:
      rebuild, then `copyover` in-game — no more cold restarts.

## The long-standing one

- [x] **Real interactive client pass** — **happened 2026-07-02/03**, Session 20:
      user connected with a real client against the home VM; account creation,
      character creation, relog, level reload, say, color tags, and PvP combat
      (limb destruction, hit penalties, defeat-to-menu) all exercised by hand.
      First finding (`<n>` vs `<z>`, color bleed) already fixed. Keep playing —
      more UX findings welcome — but the "never touched by a human" era is over.
  - [x] The `IAC SB ... SE` split-across-reads parsing gap — **fixed and
        verified 2026-07-02** (resumable parser state on `descriptor_t`,
        new `tests/smoke_test_telnet_iac.py`). Mudlet-class clients safe.

## Near-term gameplay follow-ups (small, well-defined)

- [ ] XP awarded on kill — `combat_defeat()` → `progress_add_xp()`, one-liner
      once a reward number is chosen.
- [ ] In-game immortal promotion path (`promote <name>` or similar) — today
      the only way to reach level 51+ is manual SQL.
- [ ] Enforce `min_level` in `cmd_dispatch()` once the first genuinely
      immortal-only command exists (promote is the natural candidate —
      do these two together).
- [ ] Mid-fight persistence: HP and limb HP are only saved at combat defeat;
      a mid-fight disconnect reloads at last-saved values.
- [ ] Account-creation password confirmation (type it twice) — original has
      it, Tobin doesn't.
- [ ] Delete-character password reconfirmation (original asks for the account
      password again, Tobin only asks for typed `YES`).
- [x] `smoke_test_color.py` self-contained — **done 2026-07-03**: rewritten to
      inject tags via `say` (no DB staging needed). Also added color auto-reset
      in `colorstring_translate()` (no bleed past a message that forgets `<z>`),
      found during the interactive pass. Suite fully green 18/18, first time.

## Phase 2: immortal/builder tools (user-chosen direction, 2026-07-03)

In-game world building: room/object/mob editing, zone management, object
persistence. Sequenced by dependency:

- [x] **A. Immortal command infrastructure** — **done 2026-07-03**:
      `min_level` enforced in `cmd_dispatch()` (over-level commands are
      invisible, not refused); `promote <name> [level]` (live-applies to
      online targets, works offline, can't exceed promoter's level);
      `goto <vnum>`; `help` filters by caller level; `wizhelp` shows
      real content with per-command level requirements.
- [x] **A2. DB-backed help topics + `hedit`** (user idea, same day):
      `help <topic>` shows prose from the new `help_topic` table (seeded
      for every current command); `hedit <topic>` is a level-56+ in-game
      line editor ('.' saves, '~' aborts) — Tobin's first in-game content
      editor, the pattern Phase B's room editor will reuse.
- [x] **B. Room editing — done 2026-07-03** (user direction: "take the
      interface from sneezy"): `edit` is a port of the original doEdit
      (name/description/sector_type/exit fields, prefix-matched; exit
      auto-creates missing rooms and fixes reverse exits; -1 deletes),
      level 56+, immediate MariaDB persistence. Movement commands
      (n/e/s/w/u/d, top of the command table) and look's "Obvious exits"
      landed with it. Deferred edit fields: doors/locks, flags, extra
      descriptions, river/teleport/height/capacity/spec, the VT100 menu
      mode, sector-name table.
- [ ] Watch item (downgraded 2026-07-03): the two one-off test flakes
      (combat, limbs_cmd) both happened in sweeps that were running while a
      copyover deploy froze the world for 5s. Rule: never hot-deploy during
      a sweep. If a flake ever happens WITHOUT that overlap, then widen
      recv windows.
- [ ] **C. Objects** — `obj_t` (the planned 15-category collapse), DB load,
      immortal `oload`, player get/drop/inventory, object persistence.
      Includes drop-equipment-on-death (user direction, Session 14) and
      wiring equipment slots to the existing 13-limb enum (no second enum).
- [ ] **D. Mobs** — `THING_MOB`, `mload`, mob editing, mob combat — unlocks
      the real kill-XP economy (replacing the placeholder curve).
- [ ] **E. Zone resets** — periodic respawn of mobs/objects per zone so
      built content stays populated.

### Gameplay roadmap (user-specified, 2026-07-03)

- [ ] **Log filename format change** — rename from
      `<YYYY-MM-DD_HH-MM-SS>.game.log` to
      `<2-digit day><2-digit month><2-digit year>.<hour12><minute><AM/PM>.log`
      (user spec 2026-07-03), e.g. `030726.0945AM.log`
      (strftime `%d%m%y.%I%M%p.log`). Ripple: log.c's name builder, `log
      list`'s ".game.log" filter (becomes ".log" — careful not to match
      tobin_c.log if it ever lands in logs/), smoke_test_logs.py's glob,
      and the .gitignore already covers it via logs/. Two rotations within
      the same minute now reopen the same file (append) — same benign
      behavior as today's same-second case, just a wider window.

- [ ] **Ten directions** — add Southeast, Southwest, Northeast, Northwest to
      the existing N/S/E/W/Up/Down (total 10). Abbreviations: n, s, e, w,
      u, d, se, sw, ne, nw ("ne" doesn't prefix-collide with "north", so
      plain table ordering works; keep north/south/east/west above their
      diagonal cousins). The original's dirTypeT has exactly these 10 (dirs
      6-9), rev_dirs already covers them, and the seed DB's roomexit rows
      for dirs 6-9 are currently DROPPED on load (room_repo.c) — this
      change restores real seed-world content, not just new commands.
      Touches: ROOM_NUM_EXITS 6→10, DIR_NAMES/REV_DIR, movement commands,
      redit's parse_dir, look's exits line.
- [ ] **`dig`** — create rooms just by walking: a builder-walk mode where
      moving into a nonexistent exit auto-creates the room and reverse exit
      (redit's exit machinery already does the create+reverse-fix; dig
      wires it to movement). Vnum selection strategy needed (next free in
      the builder's range?).
- [ ] **Builder access at 51+** — ALL immortals (51+) can edit rooms, mobs,
      objects, and zonefiles: lower redit's gate 56→51, and land oedit /
      medit / zedit at 51 too. **hedit (help topics) stays 56+ — user
      confirmed 2026-07-03.**
- [ ] **`news` (everyone) + `newsedit` (54+)** — announcements shown newest
      first. DB table (id, created_at, author, title/body), `news` shows
      recent items, `newsedit` uses the shared line editor. Consider
      showing unseen news at login later.
- [ ] **Positions** — sitting, standing, resting, sleeping, fighting, etc.
      for players AND mobs. Port from the original's positionTypeT
      (POSITION_DEAD..POSITION_STANDING ladder) — it gates what commands
      are legal and modifies combat (hitting a sitting target is easier).
      Commands: sit, stand, rest, sleep, wake.
- [ ] **Classes** — start with warrior, cleric, thief, monk, mage. Stat
      affinities (user spec): mage = high INT, lower STR; warrior = high
      CON+STR, can dump CHA+WIS; thief = high DEX, lower STR; cleric =
      high WIS, lower STR+DEX; monk = STR+CON, lower CHA. TAKE FROM SNEEZY:
      port the class definitions and use the original's formulas for
      class stat bonuses (misc/ class tables), mapped onto Tobin's 6-stat
      system. Chosen at character creation; shows in score/who.
- [ ] **Races** — for players and mobs. Players get a small curated list;
      mobs can be anything. TAKE FROM SNEEZY: race definitions and race
      stat bonuses from the original's race tables.
- [ ] **Game balance layer + `gameedit` (60 ONLY)** — race and class
      contribute PERCENTAGE bonuses; a level-60-only `gameedit` command
      tunes balance parameters live, in 0.1% increments. `gameedit` must
      BYPASS abbreviation matching (exact full word required, same
      mechanism as quit!) — a balance change must never happen by typo.
      Parameters persist in the DB so they survive copyovers/reboots.

### The `*edit` editor family (user-defined naming convention, 2026-07-03)

Every in-game editor follows the same name pattern and (so far) the same
line-editor/field-command interaction model. First letters are all
distinct, so each gets its single-letter abbreviation for those leveled
enough to see it:

- [x] `redit` — rooms (Session 21; renamed from `edit` to fit the family)
- [x] `hedit` — help topics (Session 20)
- [ ] `oedit` — objects (lands with Phase 2C)
- [ ] `zedit` — zones (lands with Phase 2E; zone table already in the DB)
- [ ] `medit` — mobs (lands with Phase 2D; assumed wanted to complete the
      set — confirm)
- [ ] `pedit` — player files (level/attrs/hp/location of any player,
      replacing one-off SQL; effectively an admin superset of `promote`)
- [ ] `aedit` — accounts (rename, password reset, list characters)

## Deferred decisions (blocked on choosing, not on code)

- [ ] Which ~8-10 `disc/` disciplines to keep (proposal in STATUS.md
      decisions table).
- [ ] Which 1-2 `task/` professions to keep.
- [ ] Hospital mechanic for destroyed limbs — "needs medical attention" is
      currently flavor text; only cure is death/respawn.
- [ ] Whether the destroyed-limb hit penalty should scale with the number of
      destroyed limbs (flat -15 today) — decide after real playtesting.
- [ ] Immortal-vs-immortal `kill` guard ("can't slay equal/higher level") —
      original has it; irrelevant until immortals can meet in normal play.
