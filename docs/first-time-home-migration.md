# First-time Home migration: tobin-mud → NewMUD

**One-time** guide for bringing the **Home** environment (dev tree +
`NUDServer` Fedora VM) onto the new `github.com/sculpy/NewMUD` repo. After
this, steady-state sync/deploy is just [../SYNC.md](../SYNC.md).

> Context: on 2026-07-09 the canonical repo migrated `sculpy/tobin-mud` →
> **`sculpy/NewMUD`** (private). NewMUD holds tobin-mud's entire history
> (through `c18d592`, "Zones part 1") **plus** later work — a **containers**
> feature and doc updates. Latest `main` at migration time: `8932fa7`.
> tobin-mud is now frozen — do not push to it.

Run these on the **Home dev tree** (and repeat Steps 2–3 on the `NUDServer`
VM's `~/NewMUD` if it has its own clone).

## Step 1 — Protect any un-synced Home work FIRST

This is the only step that can lose data. Do not skip it.

```bash
cd ~/NewMUD
git remote -v                          # current origin is probably tobin-mud
git status                             # uncommitted? -> commit or `git stash`
git fetch origin                       # from tobin-mud
git log --oneline origin/main..HEAD    # ANY local commits not pushed to tobin-mud?
```
- If `git log` prints commits, Home has work NewMUD doesn't have yet. **Stop.**
  Push them to tobin-mud (`git push origin main`) or save `git format-patch
  origin/main`, and tell the human — those need merging into NewMUD separately.
- If the tree is clean and at `c18d592` (or an ancestor) with nothing extra,
  it is safe to proceed: NewMUD already contains everything tobin-mud has.

## Step 2 — Point Home at NewMUD (private repo → needs credentials)

**If this machine already has `gh` authed as `sculpy`:**
```bash
git remote set-url origin https://github.com/sculpy/NewMUD.git
```

**Otherwise (recommended for the VM) — a read-only deploy key:**
```bash
ssh-keygen -t ed25519 -f ~/.ssh/newmud_deploy -N '' -C "$(hostname)-newmud-deploy"
cat ~/.ssh/newmud_deploy.pub           # give this to the human to register
git remote set-url origin git@github.com:sculpy/NewMUD.git
git config core.sshCommand "ssh -i ~/.ssh/newmud_deploy -o IdentitiesOnly=yes -o StrictHostKeyChecking=accept-new"
```
A **human must register** that public key on the repo as a **read-only**
deploy key: GitHub → NewMUD → Settings → Deploy keys, or from any machine with
`gh`: `gh repo deploy-key add newmud_deploy.pub --repo sculpy/NewMUD --title "<box> (read-only)"`.
Each box needs its own key. (This is the same setup the Work box `db.kullit.com`
already uses.)

## Step 3 — Sync to the new tip

```bash
git fetch origin
git reset --hard origin/main           # ONLY if Step 1 confirmed no un-synced work
git rev-parse --short HEAD             # expect 8932fa7 or newer
```
Make sure the gitignored upstream reference exists beside `c_port/`:
```bash
ls ~/NewMUD/sneezymud-master || \
  git clone https://github.com/sneezymud/sneezymud.git ~/NewMUD/sneezymud-master
```

## Step 4 — Deploy on NUDServer

```bash
cd ~/NewMUD/c_port
rm -rf build && cmake -S . -B build && cmake --build build   # expect ZERO warnings
bash db/apply-tobin-schema.sh                                 # loads zone_reset + new help/news/wiznews
mariadb sneezy -e "SELECT COUNT(*) FROM zone_reset;"          # expect 35922
mariadb sneezy -e "SELECT COUNT(*) FROM player;"              # sanity: must not drop
# restart the server the usual way, then:
bash tests/sweep.sh
```
Known rotating sweep flakes (pass standalone): `idle`, `parser_display`, `set`,
`mortal_toggle`, occasionally `notify`/`news`. Re-run any failure standalone
(`python3 tests/smoke_test_X.py 127.0.0.1 4000`) before treating it as real.

## Step 5 — What's new, what's next

- **New:** containers — `put`/`get <item> <container>`, `look <container>`,
  `open`/`close` on containers, weight capacity. Smoke-tested as
  `smoke_test_containers.py`. Deferred: container-content nesting persistence
  and lock/keys.
- **Next planned task:** Zones Part 2 (execute the zone resets). Containers
  were built first specifically to unblock its `P` opcode (put obj in
  container). See `c_port/TODO.md`'s zones block.

Once migrated, forget this file — use [../SYNC.md](../SYNC.md) from here on.
