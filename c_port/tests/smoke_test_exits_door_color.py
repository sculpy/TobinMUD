#!/usr/bin/env python3
"""Doored exits are shown in red in the Exits display (user 2026-08-16:
"list rooms with doors in Exits display in red color").

Both exit displays -- `look`'s one-line [Exits:] summary (cmd_look.c) and
the verbose `exits` command (cmd_exits.c) -- now color a direction red
(<r> -> \\033[0;31m) when that exit has a door, leaving plain openings in
their normal color. This walks an immortal into a room with a doored north
exit and a plain east exit (color left ON) and checks the doored direction
carries the red code while the plain one does not, in both displays.

    python3 tests/smoke_test_exits_door_color.py [host] [port]
"""
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

announce("smoke_test_exits_door_color", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time() * 1000000) // 26 ** i) % 26) for i in range(6))
BASE = 985200 + (int(time.time()) % 50)
ORIGIN, NROOM, EROOM = BASE, BASE + 1, BASE + 2
RED = "\x1b[0;31m"   # colorstring.c: <r>


def mkroom(vnum, name):
    sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
        f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
        f"VALUES ({vnum},0,0,0,'{name}','A bare test room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")


def mkexit(vnum, direction, dest, etype):
    sql(f"INSERT INTO roomexit (vnum,direction,name,description,type,condition_flag,"
        f"lock_difficulty,weight,key_num,destination) VALUES "
        f"({vnum},{direction},'','',{etype},0,0,0,0,{dest});")


pw = "exitcolpw1"
imm = f"Ecimm{_suffix}"

sql(f"DELETE FROM roomexit WHERE vnum IN ({ORIGIN},{NROOM},{EROOM});")
sql(f"DELETE FROM room WHERE vnum IN ({ORIGIN},{NROOM},{EROOM});")
mkroom(ORIGIN, "Exit Color Origin")
mkroom(NROOM, "North Chamber")
mkroom(EROOM, "East Chamber")
mkexit(ORIGIN, 0, NROOM, 1)   # north: a DOOR (type 1)
mkexit(ORIGIN, 1, EROOM, 0)   # east:  plain opening (type 0)

s = None
try:
    # create
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, imm); recv_all(s)
    send_line(s, "y"); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "new"); recv_all(s)
    send_line(s, imm); recv_all(s)
    send_line(s, "1"); recv_all(s)
    send_line(s, "1"); recv_all(s)
    send_line(s, "1"); recv_all(s)
    send_line(s, "done"); recv_all(s)
    send_line(s, "done"); recv_all(s)
    s.close()
    sql(f"UPDATE player_progress SET level=60, true_level=60 "
        f"WHERE player_id=(SELECT id FROM player WHERE name='{imm}');")

    # login WITHOUT `color off` -- we need the ANSI codes intact.
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, imm); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)

    cmd(s, f"goto {ORIGIN}")

    look = cmd(s, "look")
    check(RED + "North" in look,
          "look's [Exits:] line shows the doored North exit in red")
    check(RED + "East" not in look,
          "the plain East exit is NOT red in look's [Exits:] line")

    ex = cmd(s, "exits")   # this display uses lowercase direction names
    check(RED + "north" in ex,
          "the `exits` command shows the doored north exit in red")
    check(RED + "east" not in ex,
          "the plain east exit is NOT red in the `exits` command")

    print("=== ALL CHECKS PASSED ===")
finally:
    try:
        if s:
            cmd(s, "quit!"); s.close()
    except Exception:
        pass
    sql(f"DELETE FROM roomexit WHERE vnum IN ({ORIGIN},{NROOM},{EROOM});")
    sql(f"DELETE FROM room WHERE vnum IN ({ORIGIN},{NROOM},{EROOM});")
