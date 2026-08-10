#!/bin/sh
# Regenerates web/news_data.json from the live `news` table (user,
# 2026-08-10: "add news to tobinmud website" -- show the in-game `news`
# feed on the public site, each entry carrying its date). The in-game
# `news` viewer is open to every player at the prompt, so publishing the
# same rows here is the same visibility the game already grants, not a new
# exposure. Newest-first, matching the in-game feed order. Run on a cron
# (see crontab -l) so a new `edit news` post shows up here within a few
# minutes with no manual step -- same pattern as generate_help_json.sh.
set -e
cd "$(dirname "$0")"
mariadb -N -B --raw tobin -e "SELECT COALESCE(JSON_ARRAYAGG(JSON_OBJECT('title', title, 'author', author, 'body', body, 'date', DATE_FORMAT(created_at, '%M %e, %Y'), 'iso', DATE_FORMAT(created_at, '%Y-%m-%d')) ORDER BY created_at DESC), '[]') FROM news;" > news_data.json.tmp
mv news_data.json.tmp news_data.json
