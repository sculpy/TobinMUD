#!/bin/sh
# Regenerates web/help_data.json from the live `help_topic` table (user,
# 2026-08-08: "make help files searchable on the website"). help_topic has
# no level gate of its own -- reading a topic in-game via `help <name>` is
# open to any player regardless of the underlying command's min_level (see
# cmd_help.c's own doc comment), so publishing every row here is the same
# visibility the game already grants, not a new exposure. Run on a cron
# (see crontab -l) so an `edit help` save shows up here within a few
# minutes without a manual step.
set -e
cd "$(dirname "$0")"
mariadb -N -B --raw tobin -e "SELECT JSON_ARRAYAGG(JSON_OBJECT('name', name, 'body', body) ORDER BY name) FROM help_topic;" > help_data.json.tmp
mv help_data.json.tmp help_data.json
