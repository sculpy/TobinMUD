#!/bin/bash
# Self-contained from-scratch database build for TobinMUD.
#
# Creates and seeds the `tobin` (game world) and `immortal` (builder
# sandbox) databases directly from Tobin's OWN seed data under db/seed/,
# then layers Tobin's schema + idempotent migrations on top via
# apply-tobin-schema.sh. There is NO dependence on the old `sneezy`
# database or on the upstream sneezymud-master seed anymore -- this script
# is the whole bootstrap (user 2026-08-16: "eliminate the dependence on the
# sneezy db and self contain everything in tobin/immortal. no more sneezy
# db seed"). The seed under db/seed/ is a one-time copy of the original
# upstream world dumps, now owned by this project.
#
# Usage: init-db.sh [db_user] [game_db] [imm_db]
#   db_user  MariaDB user to create and grant access (default: current OS user)
#   game_db  game-world database name             (default: tobin)
#   imm_db   builder-sandbox database name        (default: immortal)
#
# The two DB-name arguments exist so a throwaway copy can be seeded for
# verification (e.g. `init-db.sh mud tobintest immortaltest`) without
# touching the live databases. WARNING: this DROPs and recreates both named
# databases -- never point game_db/imm_db at anything you want to keep.

set -euo pipefail

DB_USER="${1:-$(whoami)}"
GAME_DB="${2:-tobin}"
IMM_DB="${3:-immortal}"

for name in "$DB_USER" "$GAME_DB" "$IMM_DB"; do
  if [[ "$name" == *"'"* || "$name" == *'`'* ]]; then
    echo "Error: '$name' contains a quote/backtick, which is not supported." >&2
    exit 1
  fi
done

sudo systemctl is-active --quiet mariadb || sudo systemctl start mariadb

sudo mariadb -e "DROP DATABASE IF EXISTS \`${IMM_DB}\`; DROP DATABASE IF EXISTS \`${GAME_DB}\`;"
sudo mariadb -e "CREATE DATABASE \`${GAME_DB}\`; CREATE DATABASE \`${IMM_DB}\`;"
sudo mariadb -e "CREATE USER IF NOT EXISTS '${DB_USER}'@'localhost'; \
  GRANT ALL ON \`${GAME_DB}\`.* TO '${DB_USER}'@'localhost'; \
  GRANT ALL ON \`${IMM_DB}\`.* TO '${DB_USER}'@'localhost';"

DB_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Load every *.sql in a seed subdir into a target DB, in a stable,
# locale-INDEPENDENT (ASCII/C) order -- same ordering rationale as
# apply-tobin-schema.sh, so a base table file always precedes any companion
# that inserts into it.
load_seed() {  # $1 = seed subdir under db/seed, $2 = target db
  local dir="${DB_DIR}/seed/$1" db="$2"
  if [[ ! -d "$dir" ]]; then
    echo "Error: seed directory missing: $dir" >&2
    exit 1
  fi
  shopt -s nullglob
  local files=("$dir"/*.sql)
  shopt -u nullglob
  if (( ${#files[@]} == 0 )); then
    echo "Error: no .sql seed files in $dir" >&2
    exit 1
  fi
  readarray -t files < <(printf '%s\n' "${files[@]}" | LC_ALL=C sort)
  for sql in "${files[@]}"; do
    echo "seeding '${sql}' -> ${db}"
    mariadb "${db}" < "${sql}"
  done
}

load_seed world "${GAME_DB}"       # game world -> tobin
load_seed immortal "${IMM_DB}"     # builder sandbox -> immortal

# Layer Tobin's own schema + idempotent migrations onto the game DB.
"${DB_DIR}/apply-tobin-schema.sh" "${GAME_DB}"

echo "init-db: '${GAME_DB}' + '${IMM_DB}' built from db/seed and Tobin schema applied (no sneezy DB involved)."
