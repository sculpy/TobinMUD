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

## Where am I?

**Single location now (2026-07-27+): the DigitalOcean droplet.** The
former Home VM (`192.168.254.200`) and Work box (db.kullit.com) are
retired — user is disabling the Home VM and consolidating everything,
dev and production alike, onto DO. Don't ask "Home or Work" anymore.

**No local development copies, anywhere, ever (user, 2026-08-04).** All
editing, building, testing, and running happens exclusively via `ssh` on
the droplet — never in a local checkout on whatever machine Claude
Code happens to be running from (Windows, laptop, etc.), even
temporarily or "just to sync a file over." A local machine's `git`
checkout of this repo (if one exists at all) is read-only reference,
kept in sync by `git pull`, nothing more — if a session finds itself
about to `Edit`/`Write` a source file outside the droplet, or accumulate
real uncommitted work there, stop and do the edit over `ssh` instead.
This was learned the hard way: a session that developed locally,
committed, and pushed while genuinely substantial parallel work already
existed as unpushed commits directly on the droplet produced a real,
messy git divergence (real content conflicts across `mob_ai.c`,
`skill.c`, `cmd_look.c`, and 15+ test files, both sides having
independently reimplemented the same recent features) that had to be
resolved by force-pushing the droplet's history as authoritative and
manually recreating the local session's one genuinely new fix on top.
The droplet is the one and only development copy — treat it that way
from the first command of every session, not just at sync time.

- **DO droplet**: `tobinmud.com` (DNS live, A record → `159.223.121.98`),
  hostname `TobinMUD`, user `mud` (key auth set up, passwordless `sudo`),
  tree at `~/NewMUD/`, MariaDB local, **this is the live production
  server** — `tobin_c` runs here for real players, telnet `tobinmud.com:4000`
  (bare IP still works too). Deploy changes via `copyover` (below), never
  a raw kill+restart, unless the user explicitly says no one's connected.
- Both `cmake` and a plain `Makefile` work here (`cmake --version` ->
  4.3.0 present); either build path is fine.
- **Sync is git**: private repo `github.com/sculpy/NewMUD` (repo root =
  this whole tree). Commit+push when leaving, pull on arrival. The
  droplet's copy can also be updated by `tar cf - ... | ssh ... tar xf -`
  from here for a quick pre-commit test build, same as before.
  **Full sync + deploy procedure: [SYNC.md](SYNC.md)** (written for the
  old two-box setup — the git/tar mechanics still apply, just one box now).

## Build / run / test (on the droplet, via ssh)

```
cd ~/NewMUD/c_port && make -j4                # or: cmake --build build
                                               # zero warnings expected
TOBIN_DB_HOST=localhost TOBIN_DB_USER=mud TOBIN_DB_NAME=tobin \
  setsid nohup ./build/tobin_c > ~/NewMUD/tobin_c.log 2>&1 < /dev/null &
for f in tests/smoke_test*.py; do python3 "$f"; done   # full suite
```

**Always run the server under gdb while testing/developing** (user
2026-07-20): after every restart, attach immediately —
```
setsid nohup gdb -p <pid> -batch -ex "set pagination off" \
  -ex "handle SIGPIPE nostop noprint pass" -ex "continue" \
  -ex "echo \n=== CRASH CAUGHT ===\n" -ex "bt full" -ex "info threads" \
  -ex "thread apply all bt" > ~/NewMUD/gdb_crash.log 2>&1 < /dev/null &
disown
```
No `sudo` needed (`ptrace_scope=0` on these boxes; `mud` owns the process
either way). A crash mid-sweep otherwise just looks like the watchdog
cron quietly restarting the server, and every test after that point fails
for a completely unrelated reason (garbled cross-test state on the
respawned server) — burning a full ~85-minute sweep before you even find
out something crashed, let alone where. Caught a real segfault in
`cmd_skills.c` this way (2026-07-20): a full 85-minute sweep produced 106
failures across totally unrelated tests, all traced back to one
overflow-and-underflow bug in the immortal `skills` view. Re-attach after
every rebuild+restart, not just once per session.

First-time DB seed (or to re-seed) is two steps — upstream world first,
then Tobin's schema on top:

```
~/NewMUD/sneezymud-master/db/init-db.sh mud   # fresh tobin + immortal DBs
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
  appended to `c_port/db/tobin/news.sql` in the same change. Keep it
  player-facing prose with **NO NUMBERS** (no vnums, levels, counts,
  versions) in the title or body. See `news.sql` for the pattern.
- **Wiznews entries** (user rule): every code change also gets a `wiznews`
  entry appended to `c_port/db/tobin/wiznews.sql` in the same change --
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
