#!/bin/bash
# Applies Tobin's own schema/migrations (everything under db/tobin/) on top
# of a seeded game database. Normally invoked for you as the last step of
# the self-contained build:
#
#   c_port/db/init-db.sh [db_user]   # creates + seeds tobin + immortal from
#                                    # db/seed, then calls THIS on tobin
#
# Run it directly only to (re)apply migrations to an EXISTING db, e.g. after
# adding a new db/tobin/*.sql file:  apply-tobin-schema.sh [db_name].
#
# Every file under db/tobin/ is idempotent and safe to re-run (CREATE TABLE
# IF NOT EXISTS / INSERT ... ON DUPLICATE KEY UPDATE / idempotent ALTERs),
# so this doubles as the "apply new migrations to an existing DB" step. The
# files are applied in ASCII/C sort order (see below); a file that depends
# on tables/columns another creates must sort AFTER it -- e.g.
# zz_newbie_gear_race.sql is prefixed to run last, since it needs both
# suit.sql (the suit/suit_item tables) and tobin_migrations.sql (the
# race/quantity columns it inserts into).
#
# This build path no longer touches the old `sneezy` database or the
# upstream sneezymud-master seed at all (user 2026-08-16: "no more sneezy
# db seed"); the seed under db/seed/ is now owned by this project.
#
# Usage: apply-tobin-schema.sh [db_name]   (default: tobin)

set -euo pipefail

DB_NAME="${1:-tobin}"
DB_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/tobin" && pwd)"

shopt -s nullglob
sqls=("${DB_DIR}"/*.sql)
shopt -u nullglob

# Apply in a stable, locale-INDEPENDENT (ASCII/C) order. Bash pathname
# expansion sorts per LC_COLLATE, which differs across boxes -- under some
# locales `trigger_seed.sql` (INSERTs) sorts before `trigger.sql` (CREATE
# TABLE) because punctuation is ignored ("triggerseed" < "triggersql"),
# which fails on a fresh DB. In C collation '.' (0x2E) < '_' (0x5F), so a
# base file like `trigger.sql` always precedes its `trigger_seed.sql`.
readarray -t sqls < <(printf '%s\n' "${sqls[@]}" | LC_ALL=C sort)

for sql in "${sqls[@]}"; do
  echo "applying '${sql}' -> ${DB_NAME}"
  mariadb "${DB_NAME}" < "${sql}"
done
