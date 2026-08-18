#!/usr/bin/env python3
"""Smoke test for `doorbash` (Unimplemented skills/spells backlog,
Session 158 audit: Warrior, skill.c level 1). See cmd_doorbash.c.

Covers:
  1. A Warrior facing a CLOSED (in fact LOCKED) door can't walk through it.
  2. `doorbash <dir>` bursts that door open (an immortal Warrior bypasses
     the success roll, so the open path is deterministic here).
  3. The door is now open in the DB and can be walked through.
  4. Refusal paths: no direction given, and a direction with no door.

    python3 tests/smoke_test_doorbash.py [host] [port]
"""
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26 ** i) % 26) for i in range(4))
ROOM = 951000 + (int(time.time()) % 20000)
ROOM2 = ROOM + 1

# Exit condition bits (room.h): CLOSED=1, LOCKED=2. East = direction 1.
CLOSED_LOCKED = 1 | 2
DOOR_TYPE = 1  # a plain Door


def make_char(name, pw, class_num):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw, "new", name, "1", "1", str(class_num), "done", "done"):
        send_line(s, step)
        recv_all(s)
    return s


def relog(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, pw, "1"):
        send_line(s, step)
        recv_all(s)
    cmd(s, "color off")
    return s


def mkroom(vnum, name):
    sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
        f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
        f"VALUES ({vnum},0,0,0,'{name}','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")


announce("smoke_test_doorbash", host, port)

name, pw = f"Dbw{_suffix}", "dbwpw1234567"
s = make_char(name, pw, 3)  # Warrior
cmd(s, "quit!"); s.close()
sql(f"UPDATE player_progress SET level=59 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{name}');")
s = relog(name, pw)

mkroom(ROOM, "Doorbash Sandbox")
mkroom(ROOM2, "Beyond The Door")
# A closed+locked door east from ROOM -> ROOM2.
sql(f"INSERT INTO roomexit (vnum,direction,name,description,type,condition_flag,"
    f"lock_difficulty,weight,key_num,destination) VALUES "
    f"({ROOM},1,'door','',{DOOR_TYPE},{CLOSED_LOCKED},0,0,0,{ROOM2});")

check("Doorbash Sandbox" in cmd(s, f"goto {ROOM}"), "goto lands in the sandbox room")

# 1: closed/locked door blocks travel
check("closed" in cmd(s, "east").lower(), "a closed door blocks travel east")

# 4a: no direction
check("which direction" in cmd(s, "doorbash").lower(), "doorbash with no argument prompts for a direction")
# 4b: a direction with no door/exit
nb = cmd(s, "doorbash north").lower()
check("no door" in nb or "no exit" in nb, "doorbash toward a doorless/absent exit is refused")

# 2: burst it open (immortal bypasses the roll)
out = cmd(s, "doorbash east")
check("bursts off its hinges" in out.lower(), f"doorbash bursts the door open: {out[:80]!r}")

# 3: now walkable
check("Beyond The Door" in cmd(s, "east"), "the door is open -- travel east now succeeds")

cmd(s, "quit!"); s.close()
announce_done("smoke_test_doorbash", host, port)
print("=== ALL CHECKS PASSED ===")
