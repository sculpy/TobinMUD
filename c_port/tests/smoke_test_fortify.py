#!/usr/bin/env python3
"""Smoke test for `fortify` (Unimplemented skills/spells backlog,
Session 158 audit: Warrior, skill.c level 1). See cmd_fortify.c.

Covers:
  1. An immortal Warrior (bypasses the roll) raises the shield wall and
     gains the "Fortified" affect.
  2. Re-fortifying while already braced is refused.
  3. A mortal Warrior who KNOWS the skill but holds no shield is refused
     for lack of a shield (the shield-required gate).

    python3 tests/smoke_test_fortify.py [host] [port]
"""
import re
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26 ** i) % 26) for i in range(4))
ROOM = 952000 + (int(time.time()) % 20000)


def strip(s):
    return re.sub(r"\x1b\[[0-9;]*m", "", s)


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


announce("smoke_test_fortify", host, port)

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Fortify Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")

# --- 1 & 2: immortal Warrior gains, then can't re-raise, the shield wall ---
iname, ipw = f"Ftw{_suffix}", "ftwpw1234567"
s = make_char(iname, ipw, 3)  # Warrior
cmd(s, "quit!"); s.close()
sql(f"UPDATE player_progress SET level=59 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{iname}');")
s = relog(iname, ipw)
check("Fortify Sandbox" in cmd(s, f"goto {ROOM}"), "goto lands in the sandbox room")

out = cmd(s, "fortify")
check("brace against incoming attacks" in out.lower(), f"fortify raises the shield wall: {out[:80]!r}")
check("Fortified" in strip(cmd(s, "affects")), "the Fortified affect is now active")
check("already braced" in cmd(s, "fortify").lower(), "re-fortifying while braced is refused")
cmd(s, "quit!"); s.close()

# --- 3: a mortal Warrior who knows the skill but holds no shield is refused ---
mname, mpw = f"Ftm{_suffix}", "ftmpw1234567"
sm = make_char(mname, mpw, 3)  # Warrior
cmd(sm, "quit!"); sm.close()
# knows fortify (basic discipline practiced) but is otherwise a plain mortal
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{mname}';")
sql(f"UPDATE player_progress SET basic_disc_pct=100 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{mname}');")
sm = relog(mname, mpw)
check("Fortify Sandbox" in cmd(sm, "look"), "the mortal Warrior lands in the sandbox room")
out = cmd(sm, "fortify")
check("without a shield" in out.lower(),
      f"a shieldless Warrior is refused fortify: {out[:80]!r}")
cmd(sm, "quit!"); sm.close()

announce_done("smoke_test_fortify", host, port)
print("=== ALL CHECKS PASSED ===")
