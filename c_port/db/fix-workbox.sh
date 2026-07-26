#!/bin/bash
# One-shot catchup script for the Work box (db.kullit.com), bringing it to
# parity with what Sessions 73/75 already did on Home:
#
#   1. rename the live database `sneezy` -> `tobin` (atomic RENAME TABLE,
#      full backup first, old DB kept as a rollback safety net)
#   2. apply every db/tobin/*.sql migration via the existing
#      apply-tobin-schema.sh (this is what actually creates the missing
#      `player_drug` table and lands the latest wiznews/news entries)
#   3. clean rebuild
#   4. restart both live instances (preview :4003, production :4000 --
#      Work runs both side by side, sharing one DB, see SYNC.md)
#   5. smoke-test both afterward
#
# Run this ON db.kullit.com, as the `mud` user, from ~/NewMUD/c_port:
#
#   git pull origin main
#   bash db/fix-workbox.sh
#
# Safe to re-run: each step detects whether it already ran and skips if so,
# same idempotent convention as apply-tobin-schema.sh's own SQL files.
# Refuses to run at all while a real player is connected to either
# instance, since the DB rename requires stopping the server -- wait for
# the box to be empty (or coordinate with whoever's on) and re-run.

set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")/.."   # repo root (c_port/)

OLD_DB="sneezy"
NEW_DB="tobin"
BACKUP_DIR="${HOME}/db_backups"
PROD_PORT=4000
PREVIEW_PORT=4003

log() { echo "[fix-workbox] $*"; }

dump_cmd() {
  if command -v mysqldump >/dev/null 2>&1; then
    mysqldump "$@"
  elif command -v mariadb-dump >/dev/null 2>&1; then
    mariadb-dump "$@"
  else
    echo "[fix-workbox] ABORT: neither mysqldump nor mariadb-dump found." >&2
    exit 1
  fi
}

# --- 0. Refuse to run with real players connected --------------------------
for port in "$PROD_PORT" "$PREVIEW_PORT"; do
  established="$(ss -tnp 2>/dev/null | awk -v p=":${port}" '$1=="ESTAB" && $4 ~ p"$"{c++} END{print c+0}')"
  if [ "$established" -gt 0 ]; then
    echo "[fix-workbox] ABORT: ${established} real connection(s) currently ESTABLISHED on port ${port}." >&2
    echo "  This script stops the server to rename the live DB -- wait for the box to be" >&2
    echo "  empty (or coordinate with whoever's connected) before re-running." >&2
    exit 1
  fi
done

# --- 1. Stop both instances (if running) ------------------------------------
log "stopping any running tobin_c instances..."
pkill -f 'build/tobin_c' 2>/dev/null && sleep 2 || log "  (none running)"

# --- 2. Rename sneezy -> tobin, if not already done -------------------------
sneezy_exists="$(mariadb -N -e "SHOW DATABASES LIKE '${OLD_DB}';" 2>/dev/null || true)"
tobin_table_count="$(mariadb -N -e "SELECT COUNT(*) FROM information_schema.tables WHERE table_schema='${NEW_DB}';" 2>/dev/null || echo 0)"

if [ -n "$sneezy_exists" ] && [ "$tobin_table_count" -eq 0 ]; then
  log "renaming database ${OLD_DB} -> ${NEW_DB}..."
  mkdir -p "$BACKUP_DIR"
  backup_file="${BACKUP_DIR}/${OLD_DB}-backup-$(date +%Y%m%d%H%M%S).sql"
  log "  backing up ${OLD_DB} to ${backup_file}..."
  dump_cmd --single-transaction "$OLD_DB" > "$backup_file"
  log "  backup done ($(du -h "$backup_file" | cut -f1))."

  sudo mariadb -e "CREATE DATABASE IF NOT EXISTS ${NEW_DB};"

  tables="$(mariadb -N -e "SELECT table_name FROM information_schema.tables WHERE table_schema='${OLD_DB}';")"
  rename_clauses=""
  for t in $tables; do
    rename_clauses+="${OLD_DB}.\`${t}\` TO ${NEW_DB}.\`${t}\`, "
  done
  rename_clauses="${rename_clauses%, }"
  if [ -n "$rename_clauses" ]; then
    sudo mariadb -e "RENAME TABLE ${rename_clauses};"
  fi

  sudo mariadb -e "GRANT ALL PRIVILEGES ON ${NEW_DB}.* TO 'mud'@'localhost'; FLUSH PRIVILEGES;"

  new_count="$(mariadb -N -e "SELECT COUNT(*) FROM information_schema.tables WHERE table_schema='${NEW_DB}';")"
  log "  done: ${NEW_DB} now has ${new_count} tables. ${OLD_DB} kept as a rollback safety net (not dropped)."
elif [ "$tobin_table_count" -gt 0 ]; then
  log "database ${NEW_DB} already exists with ${tobin_table_count} tables -- rename already done, skipping."
else
  echo "[fix-workbox] ABORT: neither ${OLD_DB} nor a populated ${NEW_DB} database found." >&2
  echo "  Seed the DB first: sneezymud-master/db/init-db.sh mud" >&2
  exit 1
fi

# --- 3. Point .env.local at tobin -------------------------------------------
env_file=".env.local"
if [ -f "$env_file" ] && grep -q '^TOBIN_DB_NAME=' "$env_file"; then
  sed -i "s/^TOBIN_DB_NAME=.*/TOBIN_DB_NAME=${NEW_DB}/" "$env_file"
else
  echo "TOBIN_DB_NAME=${NEW_DB}" >> "$env_file"
fi
log "TOBIN_DB_NAME=${NEW_DB} in ${env_file}"

# --- 4. Apply every db/tobin/*.sql migration (idempotent) -------------------
log "applying db/tobin/*.sql migrations (player_drug + latest wiznews/news included)..."
bash db/apply-tobin-schema.sh "$NEW_DB"

# --- 5. Clean rebuild ---------------------------------------------------------
log "clean rebuild..."
rm -rf build/obj
build_log="$(mktemp)"
make -j4 2>&1 | tee "$build_log"
if grep -qi 'warning:' "$build_log"; then
  echo "[fix-workbox] ABORT: build produced warnings -- fix before deploying. See ${build_log}" >&2
  exit 1
fi
log "  zero-warning build OK."

# --- 6. Restart both instances ------------------------------------------------
log "starting preview (:${PREVIEW_PORT})..."
. ./.env.local
TOBIN_PORT="$PREVIEW_PORT" setsid nohup ./build/tobin_c >> preview.log 2>&1 < /dev/null &
disown
sleep 2

log "starting production (:${PROD_PORT})..."
. ./.env.local
setsid nohup ./build/tobin_c >> tobin_c.log 2>&1 < /dev/null &
disown
sleep 2

pgrep -af 'build/tobin_c' || { echo "[fix-workbox] ABORT: server did not start." >&2; exit 1; }

# --- 7. Smoke-test both -------------------------------------------------------
log "smoke-testing preview..."
python3 tests/smoke_test_accounts.py 127.0.0.1 "$PREVIEW_PORT"
python3 tests/smoke_test_drugs.py 127.0.0.1 "$PREVIEW_PORT"

log "smoke-testing production..."
python3 tests/smoke_test_accounts.py 127.0.0.1 "$PROD_PORT"
python3 tests/smoke_test_drugs.py 127.0.0.1 "$PROD_PORT"

log "done. Work box is now on ${NEW_DB}, player_drug table present, both instances verified."
