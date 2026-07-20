# SYNC — keeping Home and Work in step

How the two working locations stay in sync, and how each build box gets code.
Companion to [CLAUDE.md](CLAUDE.md) (project rules) and
[ENVIRONMENT.md](ENVIRONMENT.md) (per-machine setup details).

## Standing response style (applies everywhere, every session)

Keep all responses ultra-concise and direct — this travels with the repo
(git-synced) specifically so it applies at Home and Work alike, not just on
whichever machine set it locally:

- Strip all greetings, preambles, and filler text.
- Omit concluding summaries and offers for further assistance.
- Use short sentences, bullet points, or tables.
- Delete any phrase that does not add new, critical information.

## The two environments

| | Dev tree | Build/test box | Reach it via |
|---|---|---|---|
| **Home** | `E:\New MUD\` (Windows) | VirtualBox VM `NUDServer` (Fedora), tree at `~/NewMUD/` | `ssh mud@192.168.254.200` |
| **Work** | `C:\Users\jhines\NewMUD\` (Windows) | `db.kullit.com` (10.0.0.12, Fedora), tree at `~/NewMUD/` | `ssh -i ~/.ssh/id_ed25519_kullit mud@db.kullit.com` |

Both dev machines are **Windows** (the Gotchas below apply to both). Repo root
is the whole tree (`c_port/` is one subdir). MariaDB is local to each build
box; the server listens on port 4000; logs land in `c_port/logs/`.

### Home specifics

**As of 2026-07-17, Home is a real git checkout**, same golden rule as Work:
`192.168.254.200:~/NewMUD/` is a proper deploy-key clone (`git -C ~/NewMUD
pull --ff-only` just works). Its own read-only deploy key is
`~/.ssh/newmud_deploy` (public key registered on the repo as "NUDServer home
VM (read-only)"), with `core.sshCommand` already configured in that clone.

Before this date, Home was a plain scp-populated copy with no `.git`, so a
lot of session history refers to "per-file scp from `E:\New MUD\...`" as the
only way code reached the box — that workflow still works for quick
iteration (scp a changed file, build, test, without committing yet), but it
is no longer the *only* path, and a real `git pull` is now the correct way
to bring the box fully current. The old scp-only tree was preserved as
`~/NewMUD_scp_backup` on the VM in case anything from that era needs
cross-checking; it is not kept in sync and should not be treated as
authoritative.

Iterating still typically looks like: per-file `scp` from `E:\New MUD\...`
to `~/NewMUD/c_port/...` for a quick test build (fast, no commit needed
yet), then a real `commit` + `push` from `E:\New MUD\` once it's solid, at
which point the VM's own `git pull` picks it up cleanly (or the next scp
cycle just continues working the same as before — both are fine, since the
VM tree is a real checkout either way now).

## Golden rule: git is the only sync channel

**Commit + push when you leave a location; pull on arrival at the other.**
Never rely on the build boxes' working copies to carry changes — they are
derived, not authoritative.

- **Repo:** `github.com/sculpy/NewMUD` (private), branch `main`.
- **Migrated 2026-07-09** from the old `sculpy/tobin-mud`. NewMUD contains
  tobin-mud's entire history (through `c18d592`) plus everything since.
  tobin-mud is **frozen** — do not push to it. First-time move of a location
  off tobin-mud: [docs/first-time-home-migration.md](docs/first-time-home-migration.md).

### Arriving at a location
```bash
sudo dnf update -y                       # FIRST: keep the toolchain matched across boxes (see Toolchain parity)
cd ~/NewMUD && git pull --ff-only        # or reset --hard origin/main if the tree is derived-only
```
The `git pull` is on the **dev tree** — `E:\New MUD\` at Home,
`C:\Users\jhines\NewMUD\` at Work (both Windows; `cd` there, not to
`~/NewMUD`). The `dnf update` is on the Fedora build box. Then, on that
location's build box, run the deploy sequence below (at Home, scp the changed
files to the VM first — see Home specifics).

> **`dnf update` is step 0 every session** (user habit, 2026-07-13) so Home and
> Work never drift apart on gcc/cmake. `mud` is in sudoers on both boxes but
> sudo needs a password, so a human runs this (or grant a scoped
> `NOPASSWD: /usr/bin/dnf` drop-in via `visudo` to let it be automated).

### Leaving a location
```bash
cd ~/NewMUD
git status                                # nothing uncommitted left behind
git push origin main                      # the other location pulls this next time
```

## First-time setup for a build box (deploy key)

The build boxes hold **no GitHub login**, so each authenticates to the private
NewMUD repo with its own **read-only deploy key**. Do this once per box:

```bash
# on the build box:
ssh-keygen -t ed25519 -f ~/.ssh/newmud_deploy -N '' -C "$(hostname)-newmud-deploy"
cat ~/.ssh/newmud_deploy.pub            # hand this to a human to register

git -C ~/NewMUD remote set-url origin git@github.com:sculpy/NewMUD.git
git -C ~/NewMUD config core.sshCommand \
  "ssh -i ~/.ssh/newmud_deploy -o IdentitiesOnly=yes -o StrictHostKeyChecking=accept-new"
```
Register that **public** key on the repo as a **read-only** deploy key:
GitHub → NewMUD → Settings → Deploy keys, **or** from any machine with `gh`:
```bash
gh repo deploy-key add newmud_deploy.pub --repo sculpy/NewMUD --title "<box name> (read-only)"
```
After that, `git pull` on the box just works. (Both `db.kullit.com` and the
home `NUDServer` VM are set up this way as of 2026-07-17.)

Also make sure the upstream reference clone sits beside `c_port/` (it is
gitignored, so git never carries it — re-clone per location) **if that
location's work needs it**:
```bash
git clone https://github.com/sneezymud/sneezymud.git ~/NewMUD/sneezymud-master
```
Not required to build or run Tobin — only for looking up the original
SneezyMUD C++ source during porting/research work. The Home VM doesn't
currently have it (never needed it yet).

## Deploy sequence (on a build box, after pulling)

As of Home's rebuilt VM (Session 48+), the box runs **two live instances
side by side**: a **preview** on port 4003 and **production** on port 4000,
sharing the one `sneezy` DB. New work gets built + deployed to preview
first, smoke-tested there, THEN the same binary is restarted on production
— this lets iteration happen without disrupting whoever/whatever is
relying on production mid-session (including a long-running `sweep.sh`).
Both use the plain `Makefile` (`make -j4`) day-to-day, not the `cmake`
path below — `CMakeLists.txt` still exists and still works, but everything
this session actually ran was `make`.

```bash
cd ~/NewMUD/c_port
git pull origin main                                          # (or the scp-then-commit-later flow, Home specifics above)
make -j4                                                       # expect ZERO warnings; incremental is fine day-to-day,
                                                                 #   but rm -rf build/obj first after any header change
mariadb sneezy < db/sneezy/tobin_migrations.sql                # + any other changed db/sneezy/*.sql (idempotent)
mariadb sneezy < db/sneezy/wiznews.sql                         # apply the new wiznews/news changelog entries
mariadb sneezy < db/sneezy/news.sql
```

Restart preview and production **separately** (each is its own
`kill <pid>` + relaunch — find current PIDs with `pgrep -af 'build/tobin_c'`
first, since they change every restart):

```bash
# preview (port 4003)
kill <preview_pid>; sleep 1
. ./.env.local && TOBIN_PORT=4003 setsid nohup ./build/tobin_c >> preview.log 2>&1 < /dev/null &

# production (port 4000) — only after preview smoke-tests clean
kill <production_pid>; sleep 1
. ./.env.local && setsid nohup ./build/tobin_c >> tobin_c.log 2>&1 < /dev/null &
```

Then test each in turn (always both — a change deployed to preview isn't
"done" until it's also verified on production):

```bash
python3 tests/smoke_test_<new_or_touched>.py 127.0.0.1 4003    # preview first
python3 tests/smoke_test_<new_or_touched>.py 127.0.0.1 4000    # then production
bash tests/sweep.sh                                             # full suite, only right before a repo push (~85 min)
```
- **Clean up sandbox test data after every run**: smoke tests create
  throwaway rooms/mobs/characters directly via SQL (e.g. `WHERE name LIKE
  'Xyz Sandbox%'`) and don't always self-clean — leftover rows can collide
  with a LATER test's time-based vnum range (hit this live: a stale
  `Extra Desc Sandbox%` room from an old, uncleaned run collided with a
  fresh `Mount Sandbox%` room's computed vnum and broke the insert).
  `DELETE FROM room/roomexit/mob/player/player_progress WHERE name LIKE
  '<TestPrefix>%'` after each test, and periodically sweep for orphaned
  `%Sandbox%` rows left by past sessions.
- Launch line (no DB password — `mud` has local socket access), if not
  using `.env.local`:
  ```bash
  TOBIN_DB_HOST=localhost TOBIN_DB_USER=mud TOBIN_DB_NAME=sneezy \
    setsid nohup ./build/tobin_c > ~/NewMUD/tobin_c.log 2>&1 < /dev/null &
  ```
- **Known rotating sweep flakes** (pass standalone): `idle`, `parser_display`,
  `set`, `mortal_toggle`, occasionally `notify`/`news`. The failing set rotates
  run-to-run because the tests share one live server — re-run any failure
  standalone (`python3 tests/smoke_test_X.py 127.0.0.1 4000`) before treating
  it as a real regression. `parser_display` in particular has a genuine,
  unrelated intermittent flake: a telnet keepalive IAC NOP occasionally
  races the test's strict `.endswith(">")` check on an ordinary `look`/
  `score`/unknown-command line — reproduces on DIFFERENT lines each run: if
  2 full re-runs pass clean, it's the known flake, not a regression.

## Toolchain parity (Home and Work must match — stricter wins)

Both locations build the same code, so they must build it the same way. The
compiler flags are shared via git (`add_compile_options(-Wall -Wextra)` in
`c_port/CMakeLists.txt`), but **warnings still vary by gcc version** — a newer
gcc flags things an older one lets slide. So a "clean" build at one location
does not guarantee clean at the other.

**Rule: the stricter toolchain wins, and zero warnings is non-negotiable.**
- Keep gcc/cmake at the **same version** on both build boxes (both are Fedora
  44 — `sudo dnf update` to converge; don't let one drift behind).
- **Always `rm -rf build` and do a full clean rebuild before committing** —
  never trust an incremental build or a build from only one location. (An
  incremental/home-only build is exactly how two `-Wformat-truncation`
  warnings slipped into a commit on 2026-07-13; the Work box's newer gcc
  caught them, the Home build had not.)
- If Home and Work ever disagree, fix to satisfy the **stricter** one.

Reference — Work box (`db.kullit.com`) as of 2026-07-13:
`gcc (GCC) 16.1.1`, `cmake 4.3.0`, Fedora Linux 44. Home should confirm
`gcc --version` matches (or is newer) before trusting a local clean build.

> Optional hard enforcement: adding `-Werror` to the shared
> `add_compile_options` makes any warning fail the build in *both* locations —
> the strongest form of "stricter wins." Not enabled yet because a
> bleeding-edge gcc can introduce new warnings that would then break the build
> outright; enable it only once both boxes are confirmed clean and you want CI-
> grade strictness. Team decision.

## Safety checklist before a `reset --hard`

The build boxes' working trees can drift (older file-sync habits, local
tweaks). Before forcing a tree to `origin/main`:

1. `git status` — commit or stash anything real.
2. `git log --oneline origin/main..HEAD` — **any local commits not on the
   remote?** If so, push them first; a `reset --hard` would destroy them.
3. Only then `git reset --hard origin/main && git clean -fd` (this preserves
   the gitignored `sneezymud-master/`, `build/`, and `logs/`).

## Gotchas

- **Windows line endings:** on the Work dev machine, `.c`/`.h` check out as
  CRLF (`.gitattributes` only forces LF on `.sh`/`.sql`/`.py`). They compile
  fine, but don't tar-sync raw `.c` to a Linux box for a *committed* build —
  commit from Windows (normalizes to LF) and `git pull` on the box.
- **Quick pre-commit test builds** without committing: from the dev tree,
  `tar cf - <changed files> | ssh <box> "cd ~/NewMUD/c_port && tar xf -"`,
  build there, iterate, then commit clean and `git reset --hard origin/main`
  on the box to reconcile.
- **`gh` on the Work Windows machine** is at `C:\Program Files\GitHub CLI\gh.exe`
  and is often not on PATH — call it by full path.
- Running multi-line remote scripts over SSH from Windows: pipe them
  (`ssh <box> 'bash -s' < script.sh`) — inline `ssh host "…"` strings mangle
  quotes and choke on literal parentheses.
- **Watchdog cron, check yours too:** the Home VM's crontab had a bare
  `* * * * * pgrep -x "tobin_c" > /dev/null || /home/mud/.../build/tobin_c` —
  no `cd` first (wrong cwd if it ever actually fired), no log redirection
  (output vanished into cron's mail/dev-null), and no lock (a slow rebuild
  could race a manual restart into two live instances on the same port).
  Fixed by replacing the binary invocation with `c_port/watchdog.sh`
  (`cd`s to its own directory, appends to `tobin_c.log`, takes a
  `flock` on `/tmp/tobin_watchdog.lock` so only one restart ever runs) --
  crontab is now just `* * * * * /home/mud/NewMUD/c_port/watchdog.sh`.
  If the Work box (`db.kullit.com`) has a similar watchdog line, check
  `crontab -l` there and point it at the same script.
