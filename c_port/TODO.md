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

### Batch 2 roadmap (user-specified, 2026-07-03) — ordered by engine impact + complexity

**Tier 1 — new engine systems (heaviest):**
- [ ] **Shops + money** — shopkeeper buy/sell, a shop editor, and a money
      system (GOLD COIN ONLY as the currency). Depends on Phase 2C objects
      existing first. Original's shop tables are already in the seed DB.
- [ ] **TobinMUD identity + DB rename** — rename the database `sneezy` →
      `tobin` (init-db.sh, config defaults, db/ paths, docs); rebrand as
      **TobinMUD**, credited as a "Derivative of SneezyMUD and DikuMUD" in
      README/LICENSE attribution. Everything else is original work.
      (Reverses the Session-1 decision that kept the DB name — now
      deliberate and user-directed. Wide but mechanical; coordinate the
      rename on all boxes at once.)
- [ ] **Vitality** — new stat: each MOVE costs 1 vitality; regenerates
      slowly alongside HP in the same regen tick. TAKE FROM SNEEZY (the
      original's move points). Persistence like hp (player_progress
      column), show in score/prompt.
- [ ] **Terrain** — room terrain (builds on the existing sector int) gets
      names in redit and MODIFIES VITALITY COST of movement per terrain
      type. Depends on vitality. Original's TerrainInfo table is the
      source.
- [ ] **Diseases** — modest list affecting players AND mobs, from the
      original's disease.h for inspiration; immortals are immune. Needs an
      affect/tick mechanism (pulse-driven), cure path TBD.
- [ ] **Body types** — port body.h's body-type concept (different
      creatures have different limb sets) — pairs with mobs (Phase 2D)
      and the limb system.
- [ ] **Socials/actions** — port the original's lib/actions file (smile,
      nod, wave, ... the classic socials) and its command machinery.

**Tier 2 — meaningful extensions of existing systems:**
- [ ] **Limbs.h gap review** — compare our 13-limb system against the
      original's limbs.h and implement whatever's missing and sensible
      (weighted hit locations, PART_* flags, etc).
- [ ] **Player-state logging** — log item get/drop (once objects exist)
      and any player-state/pfile change, so `log search <name>` tells a
      player's story. Design a helper so every mutation site logs
      uniformly.
- [ ] **Typed logs** — port the original log.h's log-type taxonomy
      (LOG_MISC, LOG_FILE, etc); every log line gets a type and `log
      search` can filter by type.
- [ ] **Tips system** — `tips` command + periodic tip echoes to players
      (pulse-driven), a per-player newbie toggle to receive them, and
      `tipedit` (53+) to edit tips in game. DB-backed like help topics.
- [ ] **PK opt-in flag** — player-file flag: opt out of PvP (mob kills
      only). BOTH characters must have opted IN for attack/kill to work
      between players. Toggle command + persistence + combat gate.
- [ ] **Personalized immortal log messages (57+)** — per-immortal flavor
      on log lines, inspired by the original's LOG_JESUS / LOG_PEEL /
      LOG_LOW channels in log.h.
- [ ] **docs/systems storage review** — read E:\New MUD\docs\systems for
      how the original stored things; apply the lessons. RULE: always
      prefer the DB for data storage.
- [ ] **Function comment headers sweep** — every function gets a header
      comment: what it's for + cross-references to functions it affects /
      that depend on it. Codebase-wide, then maintained as a habit.
- [ ] **Systems documentation** — implement the equivalent of the
      original's doc/systems README for the TobinMUD base.

**Tier 3 — small, well-bounded changes:**
- [x] **damage.h constants** — done 2026-07-03 (include/damage.h, verbatim enum).
      into Tobin for future expansion (constants only, no behavior yet).
- [x] **Help-file upkeep habit** — CLAUDE.md house rule; practiced in this batch.
      modified, update the help topics in the same change; evaluate at
      every commit. (Also added to CLAUDE.md house rules.)
- [x] **Log gates** — done 2026-07-03: log tail/search/list 54+, rotate 59+.
      functionality (tail/search/list) drops to 54+.
- [x] **promote gate → 58+** — done 2026-07-03.
- [x] **`exits` command** — done 2026-07-03 (destinations named).
- [x] **Colorized say** — done 2026-07-03 (cyan framing, message as typed).
      `<c>You say, "<z><message><z>"` (name/wrapper colored, message as
      typed).
- [x] **Hide wizhelp from mortals** — done 2026-07-03 (min_level 51).
- [ ] **Immortal room-info display** — all immortals see the room vnum and
      sector type in look's room-name line, e.g. `[43] Imperia [sector 0]`
      (format: `[room vnum] room name [other requested info]`, extensible
      as more room fields become interesting). Mortals see the plain name.
      Touch: cmd_look.c only (being_is_immortal on the viewer).
      listed at all (set its min_level to 51); general principle: players
      only ever see help for what they can use.

### Gameplay roadmap (user-specified, 2026-07-03)

- [x] **Log filename format change** — **done 2026-07-03**:
      `<DDMMYY>.<hour12><minute><AM/PM>.log` (e.g. `030726.0945AM.log`);
      a second open in the same minute gets a -2/-3 suffix so rotation
      always yields a genuinely fresh file.

- [x] **Ten directions** — **done 2026-07-03**: NE/NW/SE/SW added (original
      dirTypeT dirs 6-9, rev_dirs ported); the seed DB's diagonal exit rows
      now load instead of being dropped — real world content restored.
      Correction to the original note: "ne" is NOT a prefix of
      "northeast", so the two-letter forms (ne/nw/se/sw) are explicit
      alias rows in the command table, classic Diku style.
- [ ] **`dig`** — create rooms just by walking: a builder-walk mode where
      moving into a nonexistent exit auto-creates the room and reverse exit
      (redit's exit machinery already does the create+reverse-fix; dig
      wires it to movement). Vnum selection strategy needed (next free in
      the builder's range?).
- [ ] **Builder access at 51+** (redit part DONE 2026-07-03) — ALL immortals (51+) can edit rooms, mobs,
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
