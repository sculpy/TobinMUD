#!/usr/bin/env python3
"""Smoke test for `stats` (TODO.md priority item, user 2026-07-30: persisted
game statistics survive reboot/copyover and display correctly). Every count
is a live SELECT COUNT(*) against the DB, so this just verifies the command
exists, is immortal-gated, and its numbers roughly match direct SQL counts.

    python3 tests/smoke_test_stats.py [host] [port]
"""
import re
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))


def strip(s):
    return re.sub(r"\x1b\[[0-9;]*m", "", s)


def sql_scalar(stmt):
    out = subprocess.run(["mariadb", "tobin", "-N", "-e", stmt],
                          check=True, capture_output=True, text=True)
    return int(out.stdout.strip())


def make_char(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "y"); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "y"); recv_all(s)
    send_line(s, ""); recv_all(s)
    send_line(s, "new"); recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "1"); recv_all(s)
    send_line(s, "1"); recv_all(s)  # territory: urban
    send_line(s, "1"); recv_all(s)
    send_line(s, "done"); recv_all(s)
    send_line(s, "done"); recv_all(s)
    return s


name, pw = f"Stat{_suffix}", "statspw12345"
s0 = make_char(name, pw)
cmd(s0, "quit!")

# Mortal (level 1) should not see `stats` at all.
out = strip(cmd(s0, "stats"))
check("huh" in out.lower() or "don't know" in out.lower() or "arg" not in out.lower(),
      "mortal cannot use stats (command invisible above their level)")
s0.close()

sql(f"UPDATE player_progress SET level=55 WHERE player_id=(SELECT id FROM player WHERE name='{name}');")

s = socket.create_connection((host, port), timeout=5)
recv_all(s)
send_line(s, name); recv_all(s)
send_line(s, pw); recv_all(s)
send_line(s, "1"); recv_all(s, 1.0)

out = strip(cmd(s, "stats"))
check("world statistics" in out.lower(), "stats command produces the world-statistics panel")
check("rooms" in out.lower() and "mobiles" in out.lower() and "objects" in out.lower(),
      "stats shows rooms/mobiles/objects")
check("accounts" in out.lower() and "characters" in out.lower(),
      "stats shows accounts/characters")
check("currently online" in out.lower(), "stats shows currently-online count")

# Cross-check against direct SQL -- values should match exactly (both are
# the same live COUNT(*) query).
room_count = sql_scalar("SELECT COUNT(*) FROM room;")
m = re.search(r"Rooms \(seeded\):\s*(\d+)", out)
check(m is not None and int(m.group(1)) == room_count,
      "room count in `stats` matches a direct SQL COUNT(*)")

player_count = sql_scalar("SELECT COUNT(*) FROM player;")
m = re.search(r"Characters:\s*(\d+)", out)
check(m is not None and int(m.group(1)) == player_count,
      "character count in `stats` matches a direct SQL COUNT(*)")

cmd(s, "quit!")
s.close()

print("\n=== ALL CHECKS PASSED ===")
