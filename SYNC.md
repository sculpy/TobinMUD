# SYNC — dev box ⇄ TobinMUD droplet

**As of 2026-07-27, single location:** the DigitalOcean droplet `tobinmud.com`
(A record → `159.223.121.98`, hostname `TobinMUD`). It is both the build/test
box and live production. The old Home VM (`192.168.254.200`) and Work box
(`db.kullit.com`) are retired. Companion to [CLAUDE.md](CLAUDE.md) (project
rules) and [ENVIRONMENT.md](ENVIRONMENT.md) (droplet setup-from-scratch).

## Standing response style (applies everywhere, every session)

Keep all responses ultra-concise and direct — this travels with the repo
(git-synced):

- Work in silence. Final reports can be delivered if brief, otherwise complete silence.
- Use short sentences, bullet points, or tables.
- Delete any phrase that does not add new, critical information.

## The one environment

| | Dev tree | Build/test/prod box | Reach it via |
|---|---|---|---|
| **Dev machine** | `C:\Users\jhines\TobinMUD\` (Windows) | — | edit here |
| **TobinMUD droplet** | — | `~/TobinMUD/`, MariaDB local, `tobin_c` on port 4000 | `ssh mud@tobinmud.com` |

Repo root is the whole tree (`c_port/` is one subdir). Logs land in
`c_port/logs/`.

## Golden rule: git is the only sync channel

**Commit + push from the dev machine; `git pull` on the droplet.** Never rely
on the droplet's working copy to carry changes back — it is derived, not
authoritative, even though it's also production.

- **Repo:** `github.com/sculpy/TobinMUD` (private), branch `main`.

```bash
# dev machine, when leaving:
git status                                # nothing uncommitted left behind
git push origin main

# droplet, on arrival:
cd ~/TobinMUD && git pull --ff-only
```

The droplet authenticates to GitHub via its own **read-only deploy key**
(`~/.ssh/tobinmud_deploy`, scoped via `core.sshCommand`) — no interactive
GitHub login there. Already set up; if a fresh droplet is ever needed, see
[ENVIRONMENT.md](ENVIRONMENT.md).

The upstream reference clone sits beside `c_port/` (gitignored, never
carried by git):
```bash
git clone https://github.com/sneezymud/sneezymud.git ~/TobinMUD/sneezymud-master
```
Not required to build or run Tobin — only for looking up the original
SneezyMUD C++ source during porting/research work.

## Deploy sequence (on the droplet, after pulling)

```bash
cd ~/TobinMUD/c_port
git pull origin main
rm -rf build && cmake -S . -B build && cmake --build build     # or: make -j4 — zero warnings expected
bash db/apply-tobin-schema.sh                                   # picks up new/changed migrations (idempotent)
mariadb tobin < db/tobin/wiznews.sql                            # apply new changelog entries
mariadb tobin < db/tobin/news.sql
mariadb tobin < db/tobin/help_topic.sql                         # only if help text changed
```

**Never `rm -rf build` directly against a box that has a live production
process running out of that directory without also restarting that exact
process afterward** — `execl()` (used by `copyover`) doesn't reset CWD, so a
deleted-then-recreated `build/` leaves the running process with a `(deleted)`
CWD; any fresh relative-path lookup (e.g. `log list`) breaks until a real
cold restart. Bit us live 2026-07-28 (Session 87) — the process kept running
on already-open handles, but the CWD itself was gone.

Restart:
- **Players connected**: trigger in-game `copyover` (level 59+). Never a raw
  kill+restart while anyone's on.
- **Server down / user confirms no one's connected**:
  ```bash
  pkill -x tobin_c; sleep 1
  TOBIN_DB_HOST=localhost TOBIN_DB_USER=mud TOBIN_DB_NAME=tobin \
    setsid nohup ./build/tobin_c > ~/TobinMUD/tobin_c.log 2>&1 < /dev/null &
  ```

Then test:
```bash
python3 tests/smoke_test_<new_or_touched>.py 127.0.0.1 4000
for f in tests/smoke_test*.py; do python3 "$f"; done   # full suite, before a push
```
- **Clean up sandbox test data after every run**: smoke tests create
  throwaway rooms/mobs/characters via SQL (`WHERE name LIKE 'Xyz Sandbox%'`)
  and don't always self-clean — leftover rows can collide with a LATER
  test's time-based vnum range. Periodically sweep for orphaned
  `%Sandbox%` rows.
- **Never hot-deploy (rebuild/restart/copyover) while a regression sweep is
  running** — the copyover freeze makes unrelated tests flake.

## Toolchain

Single box now, so no cross-location parity concern — whatever gcc/cmake
the droplet has is authoritative. Zero-warning builds (`-Wall -Wextra`) are
still non-negotiable; always `rm -rf build` before a clean rebuild ahead of
committing (see the CWD warning above — restart the live process right
after if one was running out of that directory).

## First-time setup for a fresh droplet (deploy key)

Only needed if the droplet is ever rebuilt from scratch — see
[ENVIRONMENT.md](ENVIRONMENT.md) for the full from-nothing sequence. Deploy
key summary:

```bash
ssh-keygen -t ed25519 -f ~/.ssh/tobinmud_deploy -N '' -C "$(hostname)-tobinmud-deploy"
cat ~/.ssh/tobinmud_deploy.pub            # register as a read-only deploy key

git -C ~/TobinMUD remote set-url origin git@github.com:sculpy/TobinMUD.git
git -C ~/TobinMUD config core.sshCommand \
  "ssh -i ~/.ssh/tobinmud_deploy -o IdentitiesOnly=yes -o StrictHostKeyChecking=accept-new"
```
```bash
gh repo deploy-key add tobinmud_deploy.pub --repo sculpy/TobinMUD --title "<box name> (read-only)"
```

## Safety checklist before a `reset --hard`

1. `git status` — commit or stash anything real.
2. `git log --oneline origin/main..HEAD` — any local commits not on the
   remote? Push them first; a `reset --hard` would destroy them.
3. Only then `git reset --hard origin/main && git clean -fd` (preserves the
   gitignored `sneezymud-master/`, `build/`, and `logs/`).

## Gotchas

- **Windows line endings:** on this dev machine, `.c`/`.h` check out as CRLF
  (`.gitattributes` only forces LF on `.sh`/`.sql`/`.py`). They compile fine,
  but don't tar-sync raw `.c` for a *committed* build — commit from Windows
  (normalizes to LF) and `git pull` on the droplet.
- **Quick pre-commit test builds** without committing: `tar cf - <changed
  files> | ssh mud@tobinmud.com "cd ~/TobinMUD/c_port && tar xf -"`, build
  there, iterate, then commit clean and `git reset --hard origin/main` on
  the droplet to reconcile.
- **`gh` on this Windows machine** is at `C:\Program Files\GitHub CLI\gh.exe`
  and is often not on PATH — call it by full path.
- Running multi-line remote scripts over SSH from Windows: pipe them
  (`ssh mud@tobinmud.com 'bash -s' < script.sh`) — inline `ssh host "…"`
  strings mangle quotes and choke on literal parentheses.
- **Watchdog cron**: crontab on the droplet should be
  `* * * * * /home/mud/TobinMUD/c_port/watchdog.sh` (`cd`s to its own
  directory, appends to `tobin_c.log`, takes a `flock` on
  `/tmp/tobin_watchdog.lock` so only one restart ever runs). Check
  `crontab -l` there if a restart ever seems to double-fire or go missing.
