#!/bin/bash
# Applies Tobin's own schema on top of a freshly-seeded upstream `sneezy`
# database. Run this AFTER the upstream seed:
#
#   sneezymud-master/db/init-db.sh [db_user]   # creates + seeds sneezy/immortal
#   c_port/db/apply-tobin-schema.sh            # then this
#
# These four files are Tobin-specific (not part of the upstream SneezyMUD
# seed): help_topic, player_attrs, player_progress, and the idempotent
# migrations. Each is safe to re-run (CREATE TABLE IF NOT EXISTS / INSERT
# ... ON DUPLICATE KEY UPDATE / idempotent ALTERs), so this doubles as the
# "apply new migrations to an existing DB" step.
#
# Usage: apply-tobin-schema.sh [db_name]   (default: sneezy)

set -euo pipefail

DB_NAME="${1:-sneezy}"
DB_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/sneezy" && pwd)"

shopt -s nullglob
for sql in "${DB_DIR}"/*.sql; do
  echo "applying '${sql}' -> ${DB_NAME}"
  mariadb "${DB_NAME}" < "${sql}"
done
shopt -u nullglob
