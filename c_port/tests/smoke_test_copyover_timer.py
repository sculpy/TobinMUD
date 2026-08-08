#!/usr/bin/env python3
"""Smoke test for timed copyover (user, 2026-08-08: "i wanted a timer
argument so we can copyover in 300 seconds or whatever time frame we want
converted into messages sent to all announcing every minute the countdown
has changed until 5 seconds then a count every second ... also the -now
argument ... this is for shutdown and copyover"), plus the `abort`
synonym for `cancel` on both commands (user follow-up, same day).

copyover used to be a single blocking command: warn everyone, sleep()
literally 5 seconds (freezing the whole single-threaded select loop, no
command or combat round could run), then exec. That's fine for 5 seconds,
not for a real "reboot in 5 minutes" countdown. Refactored (copyover.h/
copyover_schedule.c, mirroring shutdown.c's existing pulse-driven design)
so `copyover <seconds>` counts down via the pulse scheduler while the
game keeps running completely normally -- only the final copyover_execute()
call itself (recovery-file write + exec) is a blocking moment, same as
before, just deferred to the countdown's actual zero point instead of
always being preceded by a hardcoded 5-second freeze.

This test intentionally does NOT let a real countdown reach zero (that
would kill this very server process mid-test-suite) -- it schedules a
long countdown, confirms the game stays responsive and a milestone
broadcast arrives, then cancels it via both `cancel` and `abort` wording.
smoke_test_copyover_state.py (pre-existing) already covers an actual
completed copyover's world-state round-trip.

Covers:
  1. `copyover <seconds>` broadcasts the schedule immediately.
  2. The game keeps responding to ordinary commands while a long
     countdown is pending (not frozen).
  3. `copyover cancel` cancels a pending countdown and announces it.
  4. `copyover abort` is accepted as a synonym for `cancel`.
  5. `shutdown abort` is accepted as a synonym for `shutdown cancel`
     (shutdown's own countdown engine was untouched by this session's
     work, just given the same wording).
  6. A short (`copyover 6`) countdown actually reaches a `5 seconds`
     milestone broadcast without any input.

    python3 tests/smoke_test_copyover_timer.py [host] [port]
"""
import re
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))

announce("smoke_test_copyover_timer", host, port)

name, pw = f"Cptmr{_suffix}", "cptmrpw123456"
s = socket.create_connection((host, port), timeout=5)
recv_all(s)
for step in (name, "y", pw, pw, "new", name, "1", "1", "1", "done", "done"):
    send_line(s, step)
    recv_all(s)
cmd(s, "quit!"); s.close()
sql(f"UPDATE player_progress SET level=60 WHERE player_id=(SELECT id FROM player WHERE name='{name}');")

s = socket.create_connection((host, port), timeout=5)
recv_all(s)
for step in (name, pw, "1"):
    send_line(s, step)
    recv_all(s)

# --- 1: scheduling a long countdown announces it immediately ---
out = cmd(s, "copyover 300", timeout=1.0)
check("scheduled a copyover in 5 minutes" in out, "`copyover 300` broadcasts the schedule immediately, rendered as minutes not raw seconds")

# --- 2: the game keeps responding while it's pending ---
time.sleep(1)
out = cmd(s, "score", timeout=1.0)
check("HP" in out, "the game keeps responding to ordinary commands while a long copyover countdown is pending")

# --- 3: `cancel` cancels it ---
out = cmd(s, "copyover cancel", timeout=1.0)
check("has cancelled the scheduled copyover" in out, "`copyover cancel` cancels a pending countdown, with the canceller named")
out = cmd(s, "copyover cancel", timeout=1.0)
check("No copyover is pending" in out, "cancelling with nothing pending reports that, not a false success")

# --- 4: `abort` is accepted as a synonym on copyover ---
cmd(s, "copyover 300", timeout=1.0)
out = cmd(s, "copyover abort", timeout=1.0)
check("has cancelled the scheduled copyover" in out, "`copyover abort` works as a synonym for `copyover cancel`")

# --- 5: `abort` is accepted as a synonym on shutdown too ---
out = cmd(s, "shutdown 300", timeout=1.0)
check("scheduled a shutdown in 5 minutes" in out, "`shutdown 300` schedules a long countdown, rendered as minutes not raw seconds")

out = cmd(s, "copyover 90", timeout=1.0)
check("scheduled a copyover in 1 minute 30 seconds" in out, "a non-round duration renders as 'N minute(s) M second(s)'")
cmd(s, "copyover abort", timeout=1.0)
out = cmd(s, "shutdown abort", timeout=1.0)
check("has cancelled the scheduled shutdown" in out, "`shutdown abort` works as a synonym for `shutdown cancel`, with the canceller named")

# --- 6: a short countdown reaches its 5-second milestone unattended ---
cmd(s, "copyover 8", timeout=1.0)
buf = ""
end = time.time() + 4
s.settimeout(0.5)
while time.time() < end:
    try:
        d = s.recv(4096)
        if d:
            buf += d.decode(errors="replace")
    except socket.timeout:
        pass
check("will copyover in 5 seconds" in buf, "a short countdown reaches its 5-second milestone broadcast unattended")
cmd(s, "copyover abort", timeout=1.0)  # don't actually let this one land -- this server has more tests to run
s.close()

announce_done("smoke_test_copyover_timer", host, port)
print("=== ALL CHECKS PASSED ===")
