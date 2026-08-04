#!/usr/bin/env python3
"""Smoke test for ordinal targeting (user 2026-07-11: "mob 2.mob 3.mob etc
should attack the 1st 2nd and 3rd, same for getting multiple objects, obj
2.obj 3.obj"). Covers:

  1. Three identically-keyworded objects on the ground: bare "get widget"
     always gets the first; "get 2.widget"/"get 3.widget" get the second
     and third specifically.
  2. Three identically-keyworded mobs in the room: bare "kill dummy"
     always instakills the first (immortal caller); "kill 2.dummy"/
     "kill 3.dummy" target the second and third specifically.

    python3 tests/smoke_test_ordinal_target.py [host] [port]
"""
import re
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


announce("smoke_test_ordinal_target", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 900000 + (int(time.time()) % 70000)
OBJ1, OBJ2, OBJ3 = ROOM + 1, ROOM + 2, ROOM + 3
MOB1, MOB2, MOB3 = ROOM + 4, ROOM + 5, ROOM + 6

WEAR_TAKE = 1


def count_standing(look_output, standing_text):
    """Room listings stack identical mobs/objects as one line with an
    '(xN)' suffix (object/mob stacking feature) rather than repeating the
    line N times -- a naive substring .count() would always see at most 1.
    Returns the real instance count, whether stacked or not."""
    m = re.search(re.escape(standing_text) + r"(?: \(x(\d+)\))?", look_output)
    if not m:
        return 0
    return int(m.group(1)) if m.group(1) else 1


def set_level(name, level):
    sql(f"UPDATE player_progress SET level={level} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


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


def login(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


def make_mob(vnum, keyword):
    sql(f"INSERT INTO mob (vnum,name,short_desc,long_desc,description,actions,affects,"
        f"faction,fact_perc,letter,attacks,class,level,tohit,ac,hpbonus,damage_level,"
        f"damage_precision,gold,race,weight,height,str,bra,con,dex,agi,intel,wis,foc,"
        f"per,cha,kar,spe,pos,def_position,sex,spec_proc,skin,vision,can_be_seen,max_exist) "
        f"VALUES ({vnum},'{keyword}','a {keyword}','A {keyword} stands here.',"
        f"'desc',0,0,0,0,'A',1.0,0,1,0,0,0.3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"
        f"10,10,1,0,0,0,1,1);")


imm_name = f"Ordimm{_suffix}"
imm_pw = "ordimmpw123"
s = socket.create_connection((host, port), timeout=5)
make_char(s, imm_name, imm_pw)
set_level(imm_name, 51)
s.close()
s = login(imm_name, imm_pw)

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Ordinal Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
check("Ordinal Sandbox" in cmd(s, f"goto {ROOM}"), "goto lands in the SQL-bootstrapped sandbox room")

# --- 1: three identically-keyworded objects. Each ordinal check reloads a
# fresh set of exactly 3 (get consumes one, so a depleting pool across
# checks would make "3.widget" fail once only 1-2 remain) -- `load obj`
# spawns a new instance from the prototype each time, so re-running it
# for the same vnums is enough to reset the room to 3 loose widgets. ---
keyword = f"widget{_suffix}"
for vnum in (OBJ1, OBJ2, OBJ3):
    sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
        f"VALUES ({vnum},'{keyword}','a {keyword}','A {keyword} is lying here.',12,{WEAR_TAKE},1);")


def clear_widgets():
    # Drain any widgets left over from a prior round so each round starts
    # from exactly zero, then reload exactly 3 fresh instances.
    for _ in range(20):
        if "you get" not in cmd(s, f"get {keyword}").lower():
            break
    for vnum in (OBJ1, OBJ2, OBJ3):
        check("You conjure" in cmd(s, f"load obj {vnum}"), f"widget vnum {vnum} reloaded")


reload_three_widgets = clear_widgets
reload_three_widgets()
out = cmd(s, f"get {keyword}")
check("you get" in out.lower(), "bare 'get widget' picks up a widget")

reload_three_widgets()
out = cmd(s, f"get 2.{keyword}")
check("you get" in out.lower(), "'get 2.widget' picks up a second widget")

reload_three_widgets()
out = cmd(s, f"get 3.{keyword}")
check("you get" in out.lower(), "'get 3.widget' picks up the third widget")

reload_three_widgets()
out = cmd(s, f"get 4.{keyword}")
check("you get" not in out.lower(), "'get 4.widget' fails -- only 3 exist")

# --- 2: three identically-keyworded mobs. Each ordinal check reloads a
# fresh set of exactly 3 (killing depletes the room, so a shrinking pool
# across checks would make "3.dummy" fail once only 1-2 remain). ---
mobword = f"dummy{_suffix}"
make_mob(MOB1, mobword)
make_mob(MOB2, mobword)
make_mob(MOB3, mobword)
standing = f"A {mobword} is here."


def reload_three_dummies():
    # Slay any leftovers from a prior round first so each round starts
    # from exactly zero, then load exactly 3 fresh instances.
    for _ in range(20):
        if standing not in cmd(s, "look"):
            break
        cmd(s, f"kill {mobword}")
    for vnum in (MOB1, MOB2, MOB3):
        check("You conjure" in cmd(s, f"load mob {vnum}"), f"dummy vnum {vnum} reloaded")


reload_three_dummies()
out = cmd(s, f"kill 2.{mobword}")
check(count_standing(cmd(s, "look"), standing) == 2,
      "'kill 2.dummy' killed exactly one of the three dummies (two remain standing)")

reload_three_dummies()
out = cmd(s, f"kill 3.{mobword}")
check(count_standing(cmd(s, "look"), standing) == 2,
      "'kill 3.dummy' killed a specific dummy out of a fresh set of three (two remain standing)")

reload_three_dummies()
out = cmd(s, f"kill {mobword}")
check(count_standing(cmd(s, "look"), standing) == 2,
      "bare 'kill dummy' (defaults to ordinal 1) kills one dummy out of a fresh set of three")

s.close()
announce_done("smoke_test_ordinal_target", host, port)
print("=== ALL CHECKS PASSED ===")
