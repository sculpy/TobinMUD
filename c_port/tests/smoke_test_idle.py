#!/usr/bin/env python3
"""Smoke test for the idle flag in `who`.

An active player is NOT tagged (idle); the (idle) tag itself appears only
after five minutes of no input (verified by logic/manual test, too slow for
the sweep), and any command clears it.

    python3 tests/smoke_test_idle.py [host] [port]
"""
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

announce("smoke_test_idle", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))


name = f"Idle{_suffix}"
s = socket.create_connection((host, port), timeout=5)
recv_all(s)
for step in (name, "y", "idlepw", "idlepw", "new", name, "1", "1", "1", "done", "done"):
    send_line(s, step); recv_all(s)
cmd(s, "color off")

out = cmd(s, "who")
check(name in out, "who lists the active player")
check("(idle)" not in out, "an active player is not tagged (idle)")

s.close()
announce_done("smoke_test_idle", host, port)
print("=== ALL CHECKS PASSED ===")
