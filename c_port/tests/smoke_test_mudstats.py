#!/usr/bin/env python3
"""Smoke test for the `mudstats` command (cmd_mudstats.c).

  1. Anyone can run it; it reports room / mob / object counts.
  2. The room count is the seeded world (many thousands), so it's > 0.

    python3 tests/smoke_test_mudstats.py [host] [port]
"""
import re
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

announce("smoke_test_mudstats", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))


name = f"Stat{_suffix}"
s = socket.create_connection((host, port), timeout=5)
recv_all(s)
for step in (name, "y", "statpw", "statpw", "new", name, "1", "1", "1", "done", "done"):
    send_line(s, step); recv_all(s)
cmd(s, "color off")

out = cmd(s, "mudstats")
check("rooms in the game" in out, "mudstats reports a room count")
check("mobs (NPCs)" in out, "mudstats reports a mob count")
check("objects in the game" in out, "mudstats reports an object count")

m = re.search(r"There are (\d+) rooms", out)
check(m and int(m.group(1)) > 0, "the room count is positive (the seeded world)")

s.close()
announce_done("smoke_test_mudstats", host, port)
print("=== ALL CHECKS PASSED ===")
