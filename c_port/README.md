# Tobin — C port

A from-scratch C rewrite of the upstream `../sneezymud-master/` (SneezyMUD, a
DikuMUD-derived C++ MUD server). See [`STATUS.md`](STATUS.md) for the architecture decisions,
per-module port status, and full session-by-session log — **read that first**
if you're picking this up fresh.

Tobin boots, accepts telnet connections, logs a player in against the real
`db/tobin` schema, and supports multi-character accounts, point-buy
attributes, 60 levels (50 mortal + 10 immortal, with rank titles for
immortals), `<X>`-tag color codes, a pulse-based task/wait-state engine, and
minimal player-vs-player combat. No objects, disciplines, or spec-procs yet.

## Getting a copy of the codebase

Everything needed to build and run Tobin is two directories, taken together:

- `c_port/` — this directory: all source code, the build files, the tests,
  and Tobin's own DB schema under `c_port/db/`.
- `sneezymud-master/` (one level up) — the upstream SneezyMUD clone, which
  carries the MariaDB world seed data under `sneezymud-master/db/` (world/room
  definitions, schema-only base tables for accounts/players/etc.). Tobin will
  not boot without a database seeded from it.

If you were handed a standalone copy (e.g. a zip) rather than the full repo,
make sure both `c_port/` and `sneezymud-master/` are present as siblings —
the build/run instructions below assume that layout.

Tobin targets **Linux only** (POSIX sockets, `select()`, glibc/libxcrypt
`crypt()`) — it isn't built or tested on Windows or macOS. If you're on
Windows, use WSL2 with a Linux distro (Ubuntu is the easiest path); if you're
on macOS, a Linux VM or container is the simplest option.

## Prerequisites

On a fresh Linux box (Ubuntu/Debian or Fedora), install:

| Need | Debian/Ubuntu | Fedora |
| --- | --- | --- |
| C compiler + make | `build-essential` | `gcc make` |
| CMake (optional, see Build below) | `cmake` | `cmake` |
| MariaDB client dev headers | `libmariadb-dev` | `mariadb-connector-c-devel` |
| MariaDB server (to host the DB locally) | `mariadb-server` | `mariadb-server` |
| `pkg-config` | `pkg-config` | `pkgconf-pkg-config` |
| `libcrypt`/`libxcrypt` | present by default on most distros | present by default |
| Python 3 (only for running the test suite) | `python3` | `python3` |

```bash
# Debian/Ubuntu
sudo apt-get install build-essential cmake libmariadb-dev mariadb-server pkg-config python3

# Fedora
sudo dnf install gcc make cmake mariadb-connector-c-devel mariadb-server pkgconf-pkg-config python3
```

## Database setup

Start MariaDB, then seed it in two steps — the upstream world first, then
Tobin's own tables on top (run from the repo root):

```bash
sudo systemctl start mariadb   # or `mariadb-install-db` first on a brand-new install
sneezymud-master/db/init-db.sh          # upstream seed; grants your OS user
# or: sneezymud-master/db/init-db.sh myuser
c_port/db/apply-tobin-schema.sh         # Tobin tables + migrations
```

This creates and seeds both the `tobin` and `immortal` databases (19,209
rooms plus mobs/objects/shops/zones — the world won't function without it),
then adds Tobin's help/attrs/progress tables. See
[`db/README.md`](db/README.md) for details.

## Build

Two equivalent build files are provided — use whichever you have on hand.
Both produce `build/tobin_c` with the same compiler flags
(`-std=gnu11 -Wall -Wextra`) and link against the same libraries
(`libmariadb`, `libcrypt`).

**CMake:**

```bash
cd c_port
cmake -B build
cmake --build build
```

**Plain make** (no CMake required):

```bash
cd c_port
make            # debug build (default): build/tobin_c
make release    # optimized build (-O2)
make clean
```

Both are configured for zero compiler warnings on a clean build — if you see
any, something's off with your toolchain/headers, not expected behavior.

## Run

Point it at the MariaDB instance you seeded above:

```bash
export TOBIN_DB_HOST=localhost
export TOBIN_DB_USER=your_user
export TOBIN_DB_PASS=your_pass
export TOBIN_DB_NAME=tobin
export TOBIN_PORT=4000       # optional, defaults to 4000

./build/tobin_c
```

It fails fast with a clear error if the database isn't reachable, before
opening the listening socket.

## Try it

```bash
telnet localhost 4000
```

Enter any account name. If it doesn't exist yet, you'll be walked through
creating one (password, 3+ characters), then dropped into that account's
**character menu**. An account can own multiple characters:

- Type a number to play an existing character.
- Type `new` to create one — you'll pick a name, then allocate attributes:
  every attribute starts at 120, and you can raise or lower any single
  attribute by up to 30 in either direction, bounded overall by a net pool of
  30 points (lowering one attribute frees up room to raise another) —
  `<attribute> <amount>` (e.g. `str 30`, `wis -20`), `reset` to start over,
  `done` to finish. Whatever case you type the name in, it's normalized to
  proper case (first letter uppercase, rest lowercase) before it's stored —
  `testguy`, `TESTGUY`, and `TestGuy` all become `Testguy` everywhere it's
  displayed.
- Type `delete <name>` to remove one (asks for a typed `YES` to confirm).

You'll land in room vnum 1 ("Imperia") and can run:

- `look` / `who` / `score` (attributes, level, HP, XP)
- `color on` / `color off` to toggle ANSI color rendering of `<X>`-tagged text
- `attack <name>` to start a fight with another player in the same room —
  combat resolves over several real-time rounds (~1.2s apart), not instantly

Levels run 1-50 (mortal) and 51-60 (immortal — Immortal/God/Greater
God/Administrator/Implementor rank titles). There's no in-game path to
immortal status yet; it's DB-only (see STATUS.md).

## Running the tests

Raw-socket smoke tests live in `tests/` and drive the wire protocol directly
— no telnet client needed, but the server must already be running (defaults
to `127.0.0.1:4000`, override with `[host] [port]` args):

```bash
python3 tests/smoke_test.py
python3 tests/smoke_test_accounts.py
python3 tests/smoke_test_combat.py
# ...etc -- see tests/ for the full list
```

A few tests (`smoke_test_combat.py`, `smoke_test_level_titles.py`) shell out
to the `mariadb` CLI client to hand-set a test character's level directly in
the DB (there's no in-game promotion path yet), so they need `mariadb` on
your `PATH` and the same DB credentials the server itself is using.
`smoke_test_color.py` requires a room description already containing an
`<X>` tag to exercise translation — no seed content ships with one, so it's
not self-contained (see STATUS.md's Open Questions for how it was verified).
