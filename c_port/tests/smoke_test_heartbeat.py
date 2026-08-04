#!/usr/bin/env python3
"""Smoke test for the half-hour real-time heartbeat tick (Session 43
continued, user: "every hour on the half hour send a blank line of
uinput to the game so a tick becomes apparent to the player without any
messages"). heartbeat.c's bucket-boundary logic (fires once per real
wall-clock half-hour, not once per ~60s pulse) was manually verified
this session with a temporarily shortened bucket window (15s instead of
3600s) and a fast pulse interval, confirming: (1) a blank line is
actually delivered, (2) it does NOT re-fire on every pulse within the
same bucket. That test isn't practical to keep as an automated smoke
test -- waiting for a real half-hour boundary takes up to ~30 minutes,
and shortening the interval requires editing/rebuilding the binary,
which the full sweep can't do per-test.

What this test CAN verify without waiting an hour:
  1. A normal short observation window produces no unexpected blank-only
     bursts (the tick isn't mis-firing constantly).

    python3 tests/smoke_test_heartbeat.py [host] [port]
"""
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


announce("smoke_test_heartbeat", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))


name = f"Hbchk{_suffix}"
pw = "hbchkpw123"

s = socket.create_connection((host, port), timeout=5)
recv_all(s)
send_line(s, name); recv_all(s)
send_line(s, "y"); recv_all(s)
send_line(s, pw); recv_all(s)
send_line(s, pw); recv_all(s)
send_line(s, ""); recv_all(s)   # color default
send_line(s, ""); recv_all(s)   # timezone default
send_line(s, "new"); recv_all(s)
send_line(s, name); recv_all(s)
send_line(s, "1"); recv_all(s)  # race: human
send_line(s, "1"); recv_all(s)  # territory: urban
send_line(s, "1"); recv_all(s)  # class: mage
cmd(s, "done")
cmd(s, "done")

# --- a short window shouldn't produce a flood of blank-only bursts ---
blank_bursts = 0
t0 = time.time()
while time.time() - t0 < 5:
    out = recv_all(s, 1.0)
    if out and out.replace("\r", "").replace("\n", "") == "":
        blank_bursts += 1
check(blank_bursts <= 1,
      f"a short 5s window doesn't flood blank-only bursts (saw {blank_bursts}, "
      "the tick fires at most once per real half-hour)")

s.close()
announce_done("smoke_test_heartbeat", host, port)
print("=== ALL CHECKS PASSED ===")
