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
| **Home** | `~/NewMUD/` | VirtualBox VM `NUDServer` (Fedora) | `ssh mud@192.168.254.200` |
| **Work** | `C:\Users\jhines\NewMUD\` (Windows) | `db.kullit.com` (10.0.0.12, Fedora) | `ssh -i ~/.ssh/id_ed25519_kullit mud@db.kullit.com` |

Both keep the whole tree at `~/NewMUD/` (repo root = the whole tree). MariaDB
is local to each build box; the server listens on port 4000; logs land in
`c_port/logs/`.

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
cd ~/NewMUD && git pull --ff-only        # or reset --hard origin/main if the tree is derived-only
```
Then, on that location's build box, run the deploy sequence below.

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
After that, `git pull` on the box just works. (The work box `db.kullit.com`
is already set up this way; the home `NUDServer` VM needs its own key.)

Also make sure the upstream reference clone sits beside `c_port/` (it is
gitignored, so git never carries it — re-clone per location):
```bash
git clone https://github.com/sneezymud/sneezymud.git ~/NewMUD/sneezymud-master
```

## Deploy sequence (on a build box, after pulling)

```bash
cd ~/NewMUD/c_port
rm -rf build && cmake -S . -B build && cmake --build build   # expect ZERO warnings
bash db/apply-tobin-schema.sh                                 # Tobin schema + migrations (idempotent)
mariadb sneezy -e "SELECT COUNT(*) FROM player;"              # sanity: must not drop
# restart the server the usual way (copyover if players are connected, else cold start)
bash tests/sweep.sh                                           # full smoke suite
```
- `rm -rf build` matters after any header change: stale object files → a
  silently inconsistent binary.
- Launch line (no DB password — `mud` has local socket access):
  ```bash
  TOBIN_DB_HOST=localhost TOBIN_DB_USER=mud TOBIN_DB_NAME=sneezy \
    setsid nohup ./build/tobin_c > ~/NewMUD/tobin_c.log 2>&1 < /dev/null &
  ```
- **Known rotating sweep flakes** (pass standalone): `idle`, `parser_display`,
  `set`, `mortal_toggle`, occasionally `notify`/`news`. The failing set rotates
  run-to-run because the tests share one live server — re-run any failure
  standalone (`python3 tests/smoke_test_X.py 127.0.0.1 4000`) before treating
  it as a real regression.

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
