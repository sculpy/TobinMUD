#!/usr/bin/env python3
"""Smoke test for buildable-now #696: renting sets your load room to the
room you rented in.

An immortal (a PC, so `rent` applies) goes to a known room and rents; the
character's stored load_room in the DB should now be that room.

    python3 tests/smoke_test_rent_loadroom.py [host] [port]
"""
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

announce("smoke_test_rent_loadroom", host, port)

_sfx = "".join(chr(ord("a") + (int(time.time() * 1000000) // 26 ** i) % 26) for i in range(6))
pw = "rentp123"
ROOM = 1200
name = f"Rent{_sfx}"


def q(stmt):
    return subprocess.run(["mariadb", "tobin", "-N", "-e", stmt],
                          capture_output=True, text=True).stdout.strip()


s = socket.create_connection((host, port), timeout=5)
recv_all(s, 1.0)
for step in [name, "y", pw, pw, "new", name, "1", "1", "1", "done", "done"]:
    send_line(s, step); recv_all(s, 0.7)
s.close()
sql("UPDATE player_progress SET level=60, true_level=60 "
    f"WHERE player_id=(SELECT id FROM player WHERE name='{name}');")

s = socket.create_connection((host, port), timeout=5)
recv_all(s, 1.0)
send_line(s, name); recv_all(s, 0.7)
send_line(s, pw); recv_all(s, 0.7)
send_line(s, "1"); recv_all(s, 0.7)
cmd(s, "color off")
cmd(s, f"goto {ROOM}")
cmd(s, "rent")
time.sleep(0.5)
s.close()

load_room = q(f"SELECT load_room FROM player WHERE name='{name}';")
check(load_room == str(ROOM),
      f"after renting in room {ROOM}, load_room is now {ROOM} (got {load_room!r})")

announce_done("smoke_test_rent_loadroom", host, port)
print("PASS: smoke_test_rent_loadroom")
