# Tobin (SneezyMUD C port)

The active project is **`c_port/`** — "Tobin", a plain-C port of SneezyMUD.
The upstream C++ project it is ported *from* lives untouched under
**`sneezymud-master/`** (a fresh clone — the reference to verify against,
never modified; it also carries the world seed data under
`sneezymud-master/db/`). Tobin's own DB schema (the tables the upstream
seed doesn't have) lives in **`c_port/db/`** — see `c_port/db/README.md`.

**Read first, always:** [c_port/STATUS.md](c_port/STATUS.md) — architecture
decisions (locked), module status, and a per-session log. Update it at the
end of every session. [c_port/TODO.md](c_port/TODO.md) tracks what's next.

## Where am I? (ask the user if unstated)

- **Home**: work from this tree; build/test on the VirtualBox VM
  `NUDServer` — Fedora 44, `ssh mud@192.168.254.200` (key auth set up),
  tree mirrored at `~/NewMUD/`, MariaDB local to the VM, server on port
  4000, logs in `~/NewMUD/c_port/logs/`.
- **Work**: box is db.kullit.com (10.0.0.12), user `mud` (key auth set up),
  same `~/NewMUD/` layout; MariaDB local, server on port 4000.
- **Sync between locations is git**: private repo
  `github.com/sculpy/NewMUD` (repo root = this whole tree). Commit+push
  when leaving, pull on arrival. **Migrated 2026-07-09 from the old
  `sculpy/tobin-mud`** (which is now frozen; NewMUD contains its full
  history plus everything since). The build boxes authenticate to NewMUD
  over per-box **read-only GitHub deploy keys** (`~/.ssh/newmud_deploy`,
  scoped via each repo's `core.sshCommand`), since they hold no GitHub
  login. The Linux boxes' copies can also be updated by `tar cf - ... |
  ssh ... tar xf -` from here for quick pre-commit test builds.
  **Full sync + deploy procedure: [SYNC.md](SYNC.md).**

## Build / run / test (on the Linux box, via ssh)

```
cd ~/NewMUD/c_port && cmake --build build     # zero warnings expected
TOBIN_DB_HOST=localhost TOBIN_DB_USER=mud TOBIN_DB_NAME=sneezy \
  setsid nohup ./build/tobin_c > ~/NewMUD/tobin_c.log 2>&1 < /dev/null &
for f in tests/smoke_test*.py; do python3 "$f"; done   # full suite
```

First-time DB seed (or to re-seed) is two steps — upstream world first,
then Tobin's schema on top:

```
~/NewMUD/sneezymud-master/db/init-db.sh mud   # fresh sneezy + immortal DBs
~/NewMUD/c_port/db/apply-tobin-schema.sh      # Tobin tables + migrations
```

The second script alone is also the "apply new migrations to an existing
DB" step after pulling schema changes.

## Deploys: copyover, not restarts

When players are connected: rebuild, then trigger the in-game `copyover`
command (level 59+; `/tmp/deploy_copyover.py` on the VM scripts this).
Connections survive the exec. Cold-start only when the server is down.
**Never hot-deploy while a regression sweep is running** (the 5s copyover
freeze makes tests flake).

## House rules (learned, don't relearn)

- Verify against the original source (`sneezymud-master/code/code/`) before
  porting or "fixing" anything — several "bugs" turned out to be faithful
  behavior.
- Deviations from the original are allowed but must be deliberate and
  documented in STATUS.md's decisions table.
- Zero-warning builds (`-Wall -Wextra`); every feature ships with a smoke
  test; full suite before commit. **Toolchain parity (both locations):**
  keep the same gcc/cmake at both Home and Work (habit: run `sudo dnf update
  -y` as step 0 of every session on each box — `mud` is a password-sudoer,
  so a human runs it), and **the stricter one
  wins** — warnings vary by gcc version, so ALWAYS do a clean rebuild
  (`rm -rf build`) before committing and never trust an incremental or
  home-only build. (The Work box's newer gcc caught two format-truncation
  warnings a Home build had missed — 2026-07-13.) See SYNC.md's Toolchain
  parity note.
- Help topics are updated IN THE SAME CHANGE as the feature they document
  (new command → new topic; changed behavior → refreshed topic). Evaluate
  at every commit.
- **News entries** (user rule): every code change that affects a player's
  ability to play, changes a command, or adds new zones gets a `news` entry
  appended to `c_port/db/sneezy/news.sql` in the same change. Keep it
  player-facing prose with **NO NUMBERS** (no vnums, levels, counts,
  versions) in the title or body. See `news.sql` for the pattern.
- **Wiznews entries** (user rule): every code change also gets a `wiznews`
  entry appended to `c_port/db/sneezy/wiznews.sql` in the same change --
  the immortal-facing dev changelog. Write it in **plain English** for a
  human, NOT code-speak (no symbol names / file paths). Player-facing work
  gets both a news AND a wiznews entry; builder/immortal-only or internal
  changes get a wiznews entry only. Titles are UNIQUE (idempotent re-apply).
- Room flags and sector types display in ALL CAPS, straight from the enum
  name tables (`SECTOR_NAMES`/`ROOM_FLAG_NAMES` in room.c); the "Sector
  Type:" display shows the name only, no number.
- Test fixture names: 3-15 letters only (name validation), unique via the
  base-26 time suffix pattern in any existing test.
- Immortal-tier gates: 51+ basics + building (goto, wizhelp, loadroom,
  redit), 54+ log reading, 56+ help editing (hedit), 58+ promote/users, 59+
  operations (copyover, log rotate). Commands above the caller's level
  are invisible, not refused.
- **Editors are unified under `edit <noun> [args]`** (user 2026-07-11,
  superseding the earlier separate `ed<noun>`/`*edit` verbs): `edit room`,
  `edit zone`, `edit player`, `edit help`, `edit news`, `edit wiznews`,
  `edit rules`, `edit trigger` (dispatch in `cmd_edit.c`); `edit object`/
  `edit mob`/`edit account` planned. All **menu-driven**, like character
  creation — **the user provides a wireframe for each**. Read-only viewers
  keep plain names (`news`, `wiznews`).
- **Colorize tastefully with LOWERCASE color codes** — player/immortal
  output gets tinted with the lowercase (dim) tags by habit.
