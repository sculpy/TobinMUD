# Tobin — TODO

Last updated: 2026-07-02 (home session — VM setup). Companion to STATUS.md:
STATUS.md records what happened; this file tracks what's next. Check items off
here and log details there.

## In flight right now

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
- [ ] **B. Room editing** — in-game `redit`-style editing (name, description,
      sector, exits) persisted to MariaDB; digging new rooms/vnums. Zones are
      DB-backed (the world already lives in MariaDB), not original-style
      flatfile zonefiles.
- [ ] **C. Objects** — `obj_t` (the planned 15-category collapse), DB load,
      immortal `oload`, player get/drop/inventory, object persistence.
      Includes drop-equipment-on-death (user direction, Session 14) and
      wiring equipment slots to the existing 13-limb enum (no second enum).
- [ ] **D. Mobs** — `THING_MOB`, `mload`, mob editing, mob combat — unlocks
      the real kill-XP economy (replacing the placeholder curve).
- [ ] **E. Zone resets** — periodic respawn of mobs/objects per zone so
      built content stays populated.

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
