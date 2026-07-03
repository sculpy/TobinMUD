# Tobin (SneezyMUD C port)

The active project is **`c_port/`** — "Tobin", a plain-C port of SneezyMUD.
Everything else (`code/`, `lib/`, `db/` seed data) is the upstream C++
project being ported from; don't modify it except `db/sneezy/*.sql` when a
Tobin feature needs schema.

**Read first, always:** [c_port/STATUS.md](c_port/STATUS.md) — architecture
decisions (locked), module status, and a per-session log. Update it at the
end of every session. [c_port/TODO.md](c_port/TODO.md) tracks what's next.

## Where am I? (ask the user if unstated)

- **Home**: work from this tree; build/test on the VirtualBox VM
  `NUDServer` — Fedora 44, `ssh mud@192.168.254.200` (key auth set up),
  tree mirrored at `~/NewMUD/`, MariaDB local to the VM, server on port
  4000, logs in `~/NewMUD/c_port/logs/`.
- **Work**: box is db.kullit.com (10.0.0.12), historically `root` (a `mud`
  user is planned), same `~/NewMUD/` layout.
- **Sync between locations is git**: private repo
  `github.com/sculpy/tobin-mud` (repo root = this whole tree). Commit+push
  when leaving, pull on arrival. The Linux boxes' copies are plain copies,
  synced by `tar cf - ... | ssh ... tar xf -` from here.

## Build / run / test (on the Linux box, via ssh)

```
cd ~/NewMUD/c_port && cmake --build build     # zero warnings expected
TOBIN_DB_HOST=localhost TOBIN_DB_USER=mud TOBIN_DB_NAME=sneezy \
  setsid nohup ./build/tobin_c > ~/NewMUD/tobin_c.log 2>&1 < /dev/null &
for f in tests/smoke_test*.py; do python3 "$f"; done   # full suite
```

## Deploys: copyover, not restarts

When players are connected: rebuild, then trigger the in-game `copyover`
command (level 59+; `/tmp/deploy_copyover.py` on the VM scripts this).
Connections survive the exec. Cold-start only when the server is down.
**Never hot-deploy while a regression sweep is running** (the 5s copyover
freeze makes tests flake).

## House rules (learned, don't relearn)

- Verify against the original source (`code/code/`) before porting or
  "fixing" anything — several "bugs" turned out to be faithful behavior.
- Deviations from the original are allowed but must be deliberate and
  documented in STATUS.md's decisions table.
- Zero-warning builds; every feature ships with a smoke test; full suite
  before commit.
- Help topics are updated IN THE SAME CHANGE as the feature they document
  (new command → new topic; changed behavior → refreshed topic). Evaluate
  at every commit.
- Test fixture names: 3-15 letters only (name validation), unique via the
  base-26 time suffix pattern in any existing test.
- Immortal-tier gates: 51+ basics + building (goto, wizhelp, loadroom,
  redit), 54+ log reading, 56+ help editing (hedit), 58+ promote, 59+
  operations (copyover, log rotate). Commands above the caller's level
  are invisible, not refused.
- The `*edit` editor family: redit/hedit exist; oedit, zedit, medit,
  pedit, aedit planned (see TODO.md).
