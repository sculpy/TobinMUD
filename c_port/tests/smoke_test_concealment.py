#!/usr/bin/env python3
"""Smoke test for `concealment` (Session-158 backlog: Thief, skill.c level
30, passive). See cmd_track.c's concealment gate.

Concealment is the passive counter to `track`: a quarry who knows it
covers their own trail, so a mortal tracker's `track` goes cold on them
(they remain visible in the same room -- concealment hides the trail, not
the person). Checks, with a MORTAL thief tracker (an immortal sees
through it):

  * a plain (non-concealing) quarry one room east is tracked normally;
  * a concealment-knowing quarry in the same room has their trail go cold.

    python3 tests/smoke_test_concealment.py [host] [port]
"""
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26 ** i) % 26) for i in range(4))
ROOM_A = 957000 + (int(time.time()) % 15000)
ROOM_B = ROOM_A + 1


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


def maxdisc(name, level):
    sql(f"UPDATE player_progress SET level={level}, basic_disc_pct=100, combat_disc_pct=100, "
        f"advanced_disc_pct=100 WHERE player_id=(SELECT id FROM player WHERE name='{name}');")


announce("smoke_test_concealment", host, port)

mkroom(ROOM_A, "Conceal Sandbox A")
mkroom(ROOM_B, "Conceal Sandbox B")
sql(f"INSERT INTO roomexit (vnum,direction,name,description,type,condition_flag,"
    f"lock_difficulty,weight,key_num,destination) VALUES ({ROOM_A},1,'','',0,0,0,0,0,{ROOM_B});")

tracker, tpw = f"Cnt{_suffix}", "cntpw1234567"
plain, ppw = f"Cnp{_suffix}", "cnppw1234567"    # trackable control (no concealment)
hidden, hpw = f"Cnh{_suffix}", "cnhpw1234567"   # concealment-knowing quarry

# tracker: mortal Thief, level 30, disciplines maxed -> knows `track`.
st = make_char(tracker, tpw, 4)
cmd(st, "quit!"); st.close()
maxdisc(tracker, 30)
sql(f"UPDATE player SET load_room={ROOM_A} WHERE name='{tracker}';")

# plain quarry: a level-1 Warrior (never learns concealment).
sp = make_char(plain, ppw, 3)
cmd(sp, "quit!"); sp.close()
sql(f"UPDATE player SET load_room={ROOM_B} WHERE name='{plain}';")

# hidden quarry: mortal Thief, level 30, disciplines maxed -> knows `concealment`.
sh = make_char(hidden, hpw, 4)
cmd(sh, "quit!"); sh.close()
maxdisc(hidden, 30)
sql(f"UPDATE player SET load_room={ROOM_B} WHERE name='{hidden}';")

sp = relog(plain, ppw)
sh = relog(hidden, hpw)
st = relog(tracker, tpw)

check("Conceal Sandbox A" in cmd(st, "look"), "tracker is in room A")
check("Conceal Sandbox B" in cmd(sp, "look"), "plain quarry is in room B")
check("Conceal Sandbox B" in cmd(sh, "look"), "hidden quarry is in room B")

# The plain quarry (no concealment) is tracked normally, one hop east.
out = cmd(st, f"track {plain}")
check("leads east" in out.lower(), f"a non-concealing quarry is tracked east: {out[:80]!r}")

# The concealment-knowing quarry's trail goes cold.
out = cmd(st, f"track {hidden}")
check("goes cold" in out.lower(), f"a concealed quarry's trail goes cold: {out[:80]!r}")
check("leads" not in out.lower(), "no direction is given for a concealed quarry")

cmd(st, "quit!"); st.close()
cmd(sp, "quit!"); sp.close()
cmd(sh, "quit!"); sh.close()
sql(f"DELETE FROM player_progress WHERE player_id IN "
    f"(SELECT id FROM player WHERE name IN ('{tracker}','{plain}','{hidden}'));")
sql(f"DELETE FROM player WHERE name IN ('{tracker}','{plain}','{hidden}');")
sql(f"DELETE FROM roomexit WHERE vnum={ROOM_A};")
sql(f"DELETE FROM room WHERE vnum IN ({ROOM_A},{ROOM_B});")

announce_done("smoke_test_concealment", host, port)
print("=== ALL CHECKS PASSED ===")
