#!/usr/bin/env python3
"""Smoke test for object stacking in `inventory` (user 2026-07-26: "object
stacking needs to work on inventory"). Groups identical rendered lines
together with a "(xN)" suffix, same technique cmd_look.c's own room-floor/
mob stacking already uses (group by the RENDERED line itself, not a
separate vnum-equality check).

  1. Three of the same real item (same vnum, same condition) stack into
     one line with "(x3)".
  2. Two visually distinct items (different vnum/label) stay on their own
     separate lines, not merged together.
  3. A single (unstacked) item shows no "(x1)" suffix.

    python3 tests/smoke_test_inventory_stacking.py [host] [port]
"""
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


announce("smoke_test_inventory_stacking", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 970000 + (int(time.time()) % 20000)
SEED_TOMATO = 13880  # a small sack of tomato seeds (Planting)
SEED_ROSE = 13881    # a small sack of red rose seeds -- visually distinct


def make_char(name, pw, class_choice="1"):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "y"); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "new"); recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "1"); recv_all(s)
    send_line(s, "1"); recv_all(s)  # territory: urban
    send_line(s, class_choice); recv_all(s)
    send_line(s, "done"); recv_all(s)
    send_line(s, "done"); recv_all(s)
    cmd(s, "color off")
    return s


def relog(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


sql(f"DELETE FROM room WHERE vnum={ROOM};")
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Inv Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")

imm_name, imm_pw = f"Invimmb{_suffix}", "invimmpw1234"
s_imm = make_char(imm_name, imm_pw)
sql(f"UPDATE player_progress SET level=51 WHERE player_id=(SELECT id FROM player WHERE name='{imm_name}');")
s_imm.close()
s_imm = relog(imm_name, imm_pw)

char_name, char_pw = f"Invcharb{_suffix}", "invcharpw1234"
sc = make_char(char_name, char_pw)
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{char_name}';")
cmd(sc, "quit!")
sc.close()
sc = relog(char_name, char_pw)
cmd(s_imm, f"goto {ROOM}")

# Three of the same item.
for _ in range(3):
    cmd(s_imm, f"load obj {SEED_TOMATO}")
    cmd(s_imm, "drop seeds")
    cmd(sc, "get seeds")

# One visually distinct item.
cmd(s_imm, f"load obj {SEED_ROSE}")
cmd(s_imm, "drop seeds")
cmd(sc, "get seeds")

out = cmd(sc, "inventory")
print(out)
lines = [l for l in out.splitlines() if "seed" in l.lower()]
check(any("tomato" in l and "(x3)" in l for l in lines),
      "three identical tomato seed sacks stack into one line with (x3)")
check(any("rose" in l and "(x" not in l for l in lines),
      "the visually distinct rose seed sack stays its own unstacked line")
check(len(lines) == 2, "exactly two distinct item lines shown, not four separate ones")

announce_done("smoke_test_inventory_stacking", host, port)
print("=== ALL CHECKS PASSED ===")
