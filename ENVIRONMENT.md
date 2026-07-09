# TobinMUD — Working in Two Locations (Home ⇄ Work)

How to move development between the **home** setup and the **work** setup, and
how to stand up the work environment from nothing. Follow the parts in order.

Golden rule: **GitHub is the only thing that travels.** Code moves by
`git push` / `git pull` (or a fresh `git clone`) — never by copying working
directories around. Each machine's database is local and stays put.

---

## The moving parts

| Piece | What it is | Where it lives |
|---|---|---|
| **NewMUD repo** | The game (`c_port/`) + Tobin's schema. Source of truth. Migrated 2026-07-09 from the now-frozen `sculpy/tobin-mud`; build boxes auth via read-only deploy keys — see [SYNC.md](SYNC.md). | https://github.com/sculpy/NewMUD.git — branch `main` |
| **Upstream SneezyMUD reference** | 45 MB of original code + the base world/DB seed. **Gitignored** (`sneezymud-master/`), so it is **not** in the repo — fetch it per location. | https://github.com/sneezymud/sneezymud.git |
| **Dev box** | Where you run Claude Code and edit. Windows. | Home: `E:\New MUD` · Work: wherever you clone it |
| **Game server** | Linux box (Fedora 44): MariaDB + the built `tobin_c` binary. | Home: `mud@192.168.254.200:~/NewMUD/c_port` · Work: `mud@db.kullit.com` (internal `10.0.0.12`) `:~/NewMUD/c_port` (to create) |

> The work box currently only has a `root` login and also runs `talker.c` —
> Part 3 creates the `mud` user alongside it. Use `db.kullit.com` or `10.0.0.12`
> interchangeably for SSH.

Key facts that make the commands below work:
- The server runs as OS user **`mud`** and talks to MariaDB as `mud@localhost`
  over the **unix socket** (no password). So `mariadb sneezy` "just works" when
  logged in as `mud`.
- Databases: **`sneezy`** (the game) and **`immortal`** (immortal world copy).
- Default runtime config needs no env vars (host `localhost`, DB `sneezy`,
  telnet port `4000`). Overridable: `TOBIN_DB_HOST` / `TOBIN_DB_USER` /
  `TOBIN_DB_PASS` / `TOBIN_DB_NAME` / `TOBIN_PORT`.
- The server path is **`~/NewMUD/c_port`** (note: `NewMUD`, no space — unlike
  the Windows `E:\New MUD`). Keep this exact so all commands match.

---

## PART 1 — Before you leave home (5 seconds, do not skip)

On the **home dev box** (`E:\New MUD`), push everything up:

```sh
git add -A
git commit -m "wip: end of home session"
git push origin main
```

That is the entire hand-off. The home VM's database (accounts, test
characters) stays home — you don't need it at work; a fresh DB is rebuilt from
the seed in Part 3.

---

## PART 2 — First time at work: the dev box

1. **Install git** if it isn't there: https://git-scm.com/download/win
   (or in a terminal: `winget install --id Git.Git -e`).
2. **Clone the repo** and open it in Claude Code:
   ```sh
   git clone https://github.com/sculpy/NewMUD.git
   ```
3. *(Optional but recommended)* Fetch the upstream reference **into** the repo
   as `sneezymud-master` so Claude can read original SneezyMUD code. It's
   gitignored, so it will never be committed:
   ```sh
   cd NewMUD
   git clone https://github.com/sneezymud/sneezymud.git sneezymud-master
   ```

---

## PART 3 — First time at work: the game server (`db.kullit.com`)

Run these **on `db.kullit.com`**, in order.

### 3a. Create the `mud` user  *(as a sudo-capable login)*
```sh
sudo useradd -m -s /bin/bash mud
sudo passwd mud                     # set a password
# Optional: let your own account become mud easily, and add an SSH key:
sudo mkdir -p /home/mud/.ssh && sudo chown mud:mud /home/mud/.ssh
# (paste your public key into /home/mud/.ssh/authorized_keys, chmod 600)
```

### 3b. Install the toolchain + database  *(as sudo)*
The C connector **dev headers** are required to build (`mariadb-connector-c-devel`
/ `libmariadb-dev`) — easy to forget.

Fedora / RHEL:
```sh
sudo dnf install -y gcc cmake make git mariadb-server mariadb \
    mariadb-connector-c-devel python3
```
Debian / Ubuntu:
```sh
sudo apt update && sudo apt install -y build-essential cmake git \
    mariadb-server libmariadb-dev python3
```

### 3c. Start (and enable) MariaDB  *(as sudo)*
```sh
sudo systemctl enable --now mariadb
```

### 3d. Get the code as the `mud` user
```sh
sudo -iu mud                        # become mud
git clone https://github.com/sculpy/NewMUD.git ~/NewMUD
git clone https://github.com/sneezymud/sneezymud.git ~/NewMUD/sneezymud-master
git -C ~/NewMUD config core.autocrlf input   # keep LF endings clean
```
> Both locations keep the tree at `~/NewMUD`, so the server path is
> `~/NewMUD/c_port` everywhere and every deploy command is identical.

### 3e. Seed the databases
The seed script uses `sudo` for the CREATE DATABASE / CREATE USER steps, so run
it from a **sudo-capable** shell and pass `mud` as the DB user it should grant.
Then apply Tobin's own tables **as `mud`**.

```sh
# creates + fills fresh `sneezy` and `immortal`, and grants mud@localhost:
~/NewMUD/sneezymud-master/db/init-db.sh mud

# Tobin-specific tables + idempotent migrations on top (defaults to `sneezy`):
~/NewMUD/c_port/db/apply-tobin-schema.sh
```
Both are safe to re-run. `apply-tobin-schema.sh` is also the **"apply new
migrations after a pull"** step for later.

### 3f. Build  *(as mud)*
```sh
cd ~/NewMUD/c_port
cmake -S . -B build
cmake --build build                 # must be zero-warning
```

### 3g. First run  *(as mud)*
```sh
cd ~/NewMUD/c_port
setsid nohup ./build/tobin_c > tobin_c.log 2>&1 < /dev/null &
pgrep -x tobin_c && echo "up on :4000"
```

### 3h. Auto-restart via cron  *(as mud — mirrors home)*
```sh
crontab -e
# add this line (restarts within a minute if it ever dies):
* * * * * pgrep -x "tobin_c" >/dev/null || (cd /home/mud/NewMUD/c_port && ./build/tobin_c >> tobin_c.log 2>&1)
```
Deploys always `pkill; sleep 1; restart` within the same minute, so the cron
tick never double-launches.

### 3i. Open the firewall for telnet 4000 (if clients connect remotely)
```sh
sudo firewall-cmd --add-port=4000/tcp --permanent && sudo firewall-cmd --reload   # Fedora/RHEL
# or: sudo ufw allow 4000/tcp                                                     # Debian/Ubuntu
```

### 3j. Prove it works
```sh
cd ~/NewMUD/c_port
bash tests/sweep.sh                  # expect "SUMMARY: N passed, 0 failed"
```

---

## PART 4 — The daily deploy loop (at either location)

Edit on the dev box → push → update the server → rebuild → migrate → restart.
Because the server is a **git clone**, syncing it is just `git pull` (no scp,
no CRLF fixups — `.gitattributes` keeps `.sh`/`.sql`/`.py` as LF):

```sh
# 1. Dev box: commit + push your work
git add -A && git commit -m "..." && git push origin main

# 2. Server: pull, rebuild, apply any schema changes, restart
ssh mud@db.kullit.com          # (or mud@192.168.254.200 at home)
cd ~/NewMUD && git pull
cd c_port && cmake --build build          # zero-warning
./db/apply-tobin-schema.sh                 # picks up new/changed migrations (idempotent)
mariadb sneezy < db/sneezy/help_topic.sql  # only if help text changed (ON DUP KEY = no-op otherwise)
pkill -x tobin_c; sleep 1
setsid nohup ./build/tobin_c > tobin_c.log 2>&1 < /dev/null &
```

Rules that bit us before:
- **Never rebuild/restart while a sweep is running** — the restart freeze makes
  unrelated smoke tests flake. Let the sweep finish first.
- New schema files under `c_port/db/sneezy/` are auto-picked-up by
  `apply-tobin-schema.sh`; standalone seed edits (help/news/rules bodies) that
  use `ON DUPLICATE KEY UPDATE name=name` need an explicit `mariadb ... < file`
  to actually change existing rows.

> **The home VM is currently updated by scp, not a git clone.** If you want it
> on the same clean `git pull` model as work, once: `cd ~ && git clone
> https://github.com/sculpy/NewMUD.git NewMUD2`, seed/build it, verify, then
> swap it in for `~/NewMUD`. Until then, home deploys copy changed files with
> scp and strip CRLF (`sed -i 's/\r$//'` on `.sql`/`.py`) before building.

---

## PART 5 — Leaving work / resuming at home

- **Before leaving work:** Part 1 again from the work dev box (`git push`).
- **Back home:** on the home dev box `git pull`, then deploy to the home VM
  (Part 4). The home VM's DB still has your data — only re-seed (Part 3e) if you
  want a clean slate.

---

## Quick reference

```
Repo:            https://github.com/sculpy/NewMUD.git   (branch main)
Upstream ref:    https://github.com/sneezymud/sneezymud.git → sneezymud-master/  (gitignored, per-location)
Home server:     mud@192.168.254.200:~/NewMUD/c_port
Work server:     mud@db.kullit.com:~/NewMUD/c_port
Databases:       MariaDB `sneezy` + `immortal`, unix_socket auth as OS user mud
Seed a DB:       sneezymud-master/db/init-db.sh mud   &&   c_port/db/apply-tobin-schema.sh
Build:           cd ~/NewMUD/c_port && cmake -S . -B build && cmake --build build
Run:             cd ~/NewMUD/c_port && setsid nohup ./build/tobin_c > tobin_c.log 2>&1 < /dev/null &
Telnet port:     4000
Sweep:           cd ~/NewMUD/c_port && bash tests/sweep.sh
```
