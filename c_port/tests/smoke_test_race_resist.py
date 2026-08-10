#!/usr/bin/env python3
"""Smoke test for PC-race resistances applied at a real effect site (Phase 2,
user 2026-08-10). An immortal builds a large, long-lived puddle (pee grows
it a tier each time; drinking never consumes it) and sets Ogre poison
resistance via the `balance` editor (which refreshes the live cache). A
mortal Ogre then drinks the puddle many times: with resist 100 the poison
never lands but the resist branch fires; with resist 0 it lands. Proves
being_race_resists() gates the poison at cmd_drink.c. Restores the seeded
value. See docs/RACE_PERKS.md.

    python3 tests/smoke_test_race_resist.py [host] [port]
"""
import re
import socket
import subprocess
import sys
import time

from mud_test_utils import send_line, recv_all, check, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000
ROOM = 951000 + (int(time.time()) % 20000)


def strip(s):
    return re.sub(r"\x1b\[[0-9;]*m", "", s)


def sql(stmt):
    subprocess.run(["mariadb", "tobin", "-e", stmt], check=True)


def set_ogre_poison(imm, value):
    cmd(imm, "balance race ogre", timeout=2.0)
    cmd(imm, "9", timeout=1.2)      # field 9 = poison resistance
    cmd(imm, str(value), timeout=1.2)
    cmd(imm, "S", timeout=1.2)
    cmd(imm, "Q", timeout=1.2)


def big_puddle(imm, n=20):
    for _ in range(n):
        cmd(imm, "pee", timeout=0.7)


def drink_many(sock, n):
    out = ""
    for _ in range(n):
        out += strip(cmd(sock, "drink puddle", timeout=0.7))
    return out


announce("smoke_test_race_resist", host, port)

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) VALUES "
    f"({ROOM},0,0,0,'Resist Sandbox','A test room.\\n',NULL,1,1,0,0,0,0,0,0,0,0);")

suf = "".join(chr(ord("a") + (int(time.time()) // 26 ** i) % 26) for i in range(5))
iname, mname, pw = f"Simm{suf}", f"Sogre{suf}", "resistpw12345"

s = socket.create_connection((host, port), timeout=5)
recv_all(s)
for step in (iname, "y", pw, pw, "new", iname, "1", "1", "1", "done", "done"):
    send_line(s, step); recv_all(s)
send_line(s, "quit!"); recv_all(s); s.close()
sql(f"UPDATE player_progress SET level=60 WHERE player_id=(SELECT id FROM player WHERE name='{iname}');")
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{iname}';")

s = socket.create_connection((host, port), timeout=5)
recv_all(s)
for step in (mname, "y", pw, pw, "new", mname, "3", "1", "3", "done", "done"):  # race 3 -> Ogre
    send_line(s, step); recv_all(s)
send_line(s, "quit!"); recv_all(s); s.close()
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{mname}';")

imm = socket.create_connection((host, port), timeout=5)
recv_all(imm)
for step in (iname, pw, "1"):
    send_line(imm, step); recv_all(imm)
cmd(imm, "color off")
m = socket.create_connection((host, port), timeout=5)
recv_all(m)
for step in (mname, pw, "1"):
    send_line(m, step); recv_all(m)
cmd(m, "color off")

try:
    # --- resist 100: poison never lands, but the resist branch fires ---
    big_puddle(imm, 20)
    set_ogre_poison(imm, 100)
    out = drink_many(m, 45)
    check("scoop up some of" in out.lower(), f"mortal is actually drinking the puddle: {out[:80]!r}")
    check("poison courses through you" not in out.lower(),
          "Ogre with 100% poison resist is never poisoned")
    check("throws off the taint" in out.lower(),
          "the resist branch actually fired (saw a resisted drink)")

    # --- resist 0: poison lands at least once (control) ---
    big_puddle(imm, 20)
    set_ogre_poison(imm, 0)
    out = drink_many(m, 45)
    check("poison courses through you" in out.lower(),
          "Ogre with 0% poison resist does get poisoned (control)")
finally:
    set_ogre_poison(imm, 17)   # restore the seeded value + refresh cache

send_line(m, "quit!"); m.close()
send_line(imm, "quit!"); imm.close()
announce_done("smoke_test_race_resist", host, port)
print("=== ALL CHECKS PASSED ===")
