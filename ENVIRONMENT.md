# TobinMUD — Setting Up the Droplet From Scratch

How to stand up the DigitalOcean droplet environment if it's ever rebuilt.
The **current** droplet (`tobinmud.com`, DNS live → `159.223.121.98`,
hostname `TobinMUD`, user `mud`, key auth + passwordless `sudo`) is already
set up — this doc is the recovery/rebuild procedure, not a daily workflow.
For day-to-day sync see [SYNC.md](SYNC.md); for project rules and the
build/deploy commands, see [CLAUDE.md](CLAUDE.md).

Golden rule: **GitHub is the only thing that travels.** Code moves by
`git push` / `git pull` (or a fresh `git clone`) — never by copying working
directories around. The droplet's MariaDB is local and stays put.

---

## The moving parts

| Piece | What it is | Where it lives |
|---|---|---|
| **NewMUD repo** | The game (`c_port/`) + Tobin's schema. Source of truth. | https://github.com/sculpy/NewMUD.git — branch `main` |
| **Upstream SneezyMUD reference** | Original code + base world/DB seed. **Gitignored** (`sneezymud-master/`), not in the repo — fetch it separately. | https://github.com/sneezymud/sneezymud.git |
| **Dev box** | Where you run Claude Code and edit. Windows. | `C:\Users\jhines\NewMUD\` |
| **Droplet** | Build, test, AND live production. DigitalOcean, `tobinmud.com`. | `mud@tobinmud.com:~/NewMUD` |

Key facts that make the commands below work:
- The server runs as OS user **`mud`** and talks to MariaDB as `mud@localhost`
  over the **unix socket** (no password). `mariadb tobin` "just works" when
  logged in as `mud`.
- Databases: **`tobin`** (the game) and **`immortal`** (immortal world copy).
- Default runtime config needs no env vars (host `localhost`, DB `tobin`,
  telnet port `4000`). Overridable: `TOBIN_DB_HOST` / `TOBIN_DB_USER` /
  `TOBIN_DB_PASS` / `TOBIN_DB_NAME` / `TOBIN_PORT`.
- Server path: **`~/NewMUD/c_port`**.
- DNS: `tobinmud.com` A record → the droplet's IP. Players telnet to
  `tobinmud.com:4000` (bare IP still works too).

---

## PART 1 — Before you leave the dev machine (5 seconds, do not skip)

```sh
git add -A
git commit -m "..."
git push origin main
```

---

## PART 2 — Dev machine setup (one-time)

1. **Install git**: https://git-scm.com/download/win (or `winget install
   --id Git.Git -e`).
2. **Clone the repo**:
   ```sh
   git clone https://github.com/sculpy/NewMUD.git
   ```
3. *(Optional but recommended)* Fetch the upstream reference into the repo
   as `sneezymud-master` (gitignored, never committed):
   ```sh
   cd NewMUD
   git clone https://github.com/sneezymud/sneezymud.git sneezymud-master
   ```

---

## PART 3 — Rebuilding the droplet from nothing

Only needed if the current droplet is ever lost/replaced. Run these **on
the new droplet**, in order.

### 3a. Create the `mud` user *(as a sudo-capable login)*
```sh
sudo useradd -m -s /bin/bash mud
sudo passwd mud
sudo mkdir -p /home/mud/.ssh && sudo chown mud:mud /home/mud/.ssh
# paste your public key into /home/mud/.ssh/authorized_keys, chmod 600
# grant passwordless sudo for mud (visudo -> mud ALL=(ALL) NOPASSWD:ALL, or scope it)
```

### 3b. Install the toolchain + database *(as sudo)*
The C connector **dev headers** are required to build
(`mariadb-connector-c-devel` / `libmariadb-dev`).

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

### 3c. Start (and enable) MariaDB *(as sudo)*
```sh
sudo systemctl enable --now mariadb
```

### 3d. Get the code as the `mud` user
```sh
sudo -iu mud
git clone https://github.com/sculpy/NewMUD.git ~/NewMUD
git clone https://github.com/sneezymud/sneezymud.git ~/NewMUD/sneezymud-master
git -C ~/NewMUD config core.autocrlf input
```

Set up the read-only deploy key (the droplet has no interactive GitHub
login):
```sh
ssh-keygen -t ed25519 -f ~/.ssh/newmud_deploy -N '' -C "$(hostname)-newmud-deploy"
cat ~/.ssh/newmud_deploy.pub
git -C ~/NewMUD remote set-url origin git@github.com:sculpy/NewMUD.git
git -C ~/NewMUD config core.sshCommand \
  "ssh -i ~/.ssh/newmud_deploy -o IdentitiesOnly=yes -o StrictHostKeyChecking=accept-new"
```
Register the public key: GitHub → NewMUD → Settings → Deploy keys, or
```sh
gh repo deploy-key add newmud_deploy.pub --repo sculpy/NewMUD --title "<box name> (read-only)"
```

### 3e. Seed the databases
```sh
~/NewMUD/sneezymud-master/db/init-db.sh mud     # fresh tobin + immortal, grants mud@localhost
~/NewMUD/c_port/db/apply-tobin-schema.sh        # Tobin tables + idempotent migrations
```
Both safe to re-run. `apply-tobin-schema.sh` is also the "apply new
migrations after a pull" step going forward.

### 3f. Build *(as mud)*
```sh
cd ~/NewMUD/c_port
cmake -S . -B build
cmake --build build                 # must be zero-warning
```

### 3g. First run *(as mud)*
```sh
cd ~/NewMUD/c_port
setsid nohup ./build/tobin_c > tobin_c.log 2>&1 < /dev/null &
pgrep -x tobin_c && echo "up on :4000"
```

### 3h. Auto-restart via cron *(as mud)*
```sh
crontab -e
# * * * * * /home/mud/NewMUD/c_port/watchdog.sh
```
`watchdog.sh` `cd`s to its own directory, appends to `tobin_c.log`, and
takes a `flock` on `/tmp/tobin_watchdog.lock` so only one restart ever runs.

### 3i. Open the firewall for telnet 4000

**Two independent firewall layers guard this droplet -- both must be
opened, or the port silently times out with no error on either side**
(burned real debugging time, 2026-08-05, opening port 80 for the
TobinMUD Client's update host -- the DO cloud firewall rule alone
looked correct and was NOT enough):

1. **Local, on the droplet itself.** Despite this being Fedora (where
   `firewall-cmd`/firewalld is the normal default), the CURRENT droplet
   actually runs **ufw** instead -- `firewall-cmd` isn't even installed
   (`command not found`). Check which one is actually active before
   assuming:
   ```sh
   sudo ufw status numbered          # if this shows Status: active, use ufw, not firewall-cmd
   sudo ufw allow <port>/tcp
   ```
   (If a future droplet rebuild genuinely uses firewalld instead:
   `sudo firewall-cmd --add-port=<port>/tcp --permanent && sudo firewall-cmd --reload`.)
2. **DigitalOcean's cloud firewall** (a separate product, configured
   via the DO web console under Networking -> Firewalls, or `doctl
   compute firewall add-rules`) -- open the same port there too, on
   whichever firewall resource is actually attached to this droplet.

Test end-to-end from OUTSIDE the droplet (an open local port with a
still-blocking cloud firewall, or vice versa, both look identical from
localhost -- `curl http://127.0.0.1/...` succeeding proves nothing
about external reachability):
```sh
curl -v --max-time 10 http://tobinmud.com:<port>/...
```

### 3j. DNS
Point `tobinmud.com`'s A record at the droplet's public IP with your
registrar/DNS provider. (Already done for the current droplet — DNS is
live and the domain resolves.)

### 3k. Prove it works
```sh
cd ~/NewMUD/c_port
bash tests/sweep.sh                  # expect "SUMMARY: N passed, 0 failed"
```

---

## PART 4 — The daily deploy loop

See [SYNC.md](SYNC.md)'s "Deploy sequence" section — it's the day-to-day
version of PART 3's build/migrate/restart steps, plus the `copyover`-vs-
cold-restart rule and the gdb-attach habit.

---

## Quick reference

```
Repo:            https://github.com/sculpy/NewMUD.git   (branch main)
Upstream ref:    https://github.com/sneezymud/sneezymud.git → sneezymud-master/  (gitignored, per-location)
Droplet:         mud@tobinmud.com:~/NewMUD/c_port   (also live production)
Databases:       MariaDB `tobin` + `immortal`, unix_socket auth as OS user mud
Seed a DB:       sneezymud-master/db/init-db.sh mud   &&   c_port/db/apply-tobin-schema.sh
Build:           cd ~/NewMUD/c_port && cmake -S . -B build && cmake --build build   (or: make -j4)
Run:             cd ~/NewMUD/c_port && setsid nohup ./build/tobin_c > tobin_c.log 2>&1 < /dev/null &
Telnet:          tobinmud.com:4000  (or 159.223.121.98:4000)
Sweep:           cd ~/NewMUD/c_port && bash tests/sweep.sh
```
