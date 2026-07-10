#!/bin/bash
# Cron watchdog (every minute, see `crontab -l`): restarts tobin_c if it
# isn't running. Session 43 fix -- the original crontab line invoked the
# binary directly with no `cd` (wrong cwd if it ever fired), no log
# redirection (output vanished into cron's mail/dev-null), and no lock
# (a slow rebuild could race a manual restart into starting two instances).
# This script fixes all three; the crontab entry now just calls this file.
cd "$(dirname "$0")" || exit 1

exec 200>/tmp/tobin_watchdog.lock
flock -n 200 || exit 0  # another watchdog tick is already mid-restart

pgrep -x tobin_c >/dev/null && exit 0

setsid nohup ./build/tobin_c >> tobin_c.log 2>&1 < /dev/null &
disown
