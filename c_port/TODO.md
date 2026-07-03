# Tobin — TODO

Last updated: 2026-07-02 (home session — VM setup). Companion to STATUS.md:
STATUS.md records what happened; this file tracks what's next. Check items off
here and log details there.

## In flight right now

- [ ] Finish `dnf upgrade` on the home VM (user running it; parallel downloads
      now enabled). Afterwards: reboot, relaunch `tobin_c`, rerun one smoke
      test to confirm the updated system is healthy.
- [ ] If the update pulled a new kernel: verify VirtualBox Guest Additions
      still build/load (networking is unaffected either way).
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

## The long-standing one

- [ ] **Real interactive client pass** — everything to date is verified only by
      scripted raw-socket tests. Connect with real telnet/Mudlet, poke at the
      menus, point-buy, combat, color. Open since Session 1; now that the VM
      is on the LAN there's no excuse left.
  - [ ] While doing this with Mudlet/MUSHclient: the known `IAC SB ... SE`
        split-across-reads parsing gap in `descriptor.c` becomes a real risk
        (those clients proactively negotiate NAWS/TTYPE). Fix = resumable
        subnegotiation parser.

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
- [ ] Make `smoke_test_color.py` self-contained — it needs `<X>`-tagged
      content in the DB and fails on any fresh seed (expected-fail today,
      confirmed again on the home VM 2026-07-02). Add a small always-present
      tagged string somewhere testable.

## Phase 2 (pick a direction)

- [ ] **NPC/mob support** — combat and levels were designed to extend into
      this; unlocks a real kill-XP economy (replacing the placeholder curve).
- [ ] **`obj/` equipment design** — collapse 98 classes to ~15 categories,
      tagged union populated from DB val0..val3. Must include from day one:
      defeated characters drop carried equipment in the room where they died
      (user-stated direction, Session 14).
- [ ] Wire equipment slots to the existing 13-limb enum when `obj/` lands
      (don't invent a second slot enum).

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
