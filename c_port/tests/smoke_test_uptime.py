#!/usr/bin/env python3
"""Smoke test for `uptime` (TODO.md priority item, user 2026-08-02).

Covers: `uptime` reports a nonzero-but-small elapsed time right after a
fresh boot/copyover (this test is meant to be run shortly after one), and
the "Up since"/"Uptime:" lines are both present.

    python3 tests/smoke_test_uptime.py [host] [port]
"""
import re
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


announce("smoke_test_uptime", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))


def make_char(sock, name, pw):
    recv_all(sock)
    send_line(sock, name); recv_all(sock)
    send_line(sock, "y"); recv_all(sock)
    send_line(sock, pw); recv_all(sock)
    send_line(sock, pw); recv_all(sock)
    send_line(sock, "new"); recv_all(sock)
    send_line(sock, name); recv_all(sock)
    send_line(sock, "1"); recv_all(sock)
    send_line(sock, "1"); recv_all(sock)  # territory: urban
    send_line(sock, "1"); recv_all(sock)
    send_line(sock, "done"); recv_all(sock)
    send_line(sock, "done"); recv_all(sock)


name = f"Uptmor{_suffix}"
pw = "uptmorpw123"
s = socket.create_connection((host, port), timeout=5)
make_char(s, name, pw)
cmd(s, "color off")

out = cmd(s, "uptime")
check("up since" in out.lower(), "uptime shows the 'Up since' line")
check("uptime:" in out.lower(), "uptime shows the 'Uptime:' line")
m = re.search(r"Uptime: (\d+) day", out)
check(m is not None, "the day count is a parseable integer")
check(int(m.group(1)) < 2, "uptime reports a small day count (server booted recently)")

s.close()
announce_done("smoke_test_uptime", host, port)
print("=== ALL CHECKS PASSED ===")
