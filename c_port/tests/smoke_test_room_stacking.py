#!/usr/bin/env python3
"""Smoke test for room-listing stacking (user 2026-07-11: "in look at
room, object stacking and mob stacking. for 2 gremlins you would see
A gremlin is standing here. (x2)"). Covers:

  1. Three identical mobs show as ONE line with "(x3)", not three
     separate lines.
  2. Three identical loose objects show as ONE line with "(x3)".
  3. A single (non-duplicated) mob/object shows with no "(xN)" suffix
     at all.
  4. Two DIFFERENT mobs (different short_descr) are never merged into
     the same group.

    python3 tests/smoke_test_room_stacking.py [host] [port]
"""
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


announce("smoke_test_room_stacking", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 900000 + (int(time.time()) % 70000)
MOBA1, MOBA2, MOBA3 = ROOM + 1, ROOM + 2, ROOM + 3
MOBB = ROOM + 4
OBJA1, OBJA2, OBJA3 = ROOM + 5, ROOM + 6, ROOM + 7


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


def make_mob(vnum, keyword):
    sql(f"INSERT INTO mob (vnum,name,short_desc,long_desc,description,actions,affects,"
        f"faction,fact_perc,letter,attacks,class,level,tohit,ac,hpbonus,damage_level,"
        f"damage_precision,gold,race,weight,height,str,bra,con,dex,agi,intel,wis,foc,"
        f"per,cha,kar,spe,pos,def_position,sex,spec_proc,skin,vision,can_be_seen,max_exist) "
        f"VALUES ({vnum},'{keyword}','a {keyword}','A {keyword} stands here.',"
        f"'desc',0,0,0,0,'A',1.0,0,1,0,0,0.3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"
        f"10,10,1,0,0,0,1,1);")


imm_name = f"Stackimm{_suffix}"
imm_pw = "stackimmpw123"
s = socket.create_connection((host, port), timeout=5)
make_char(s, imm_name, imm_pw)
set_level(imm_name, 51)
s.close()
s = socket.create_connection((host, port), timeout=5)
recv_all(s)
send_line(s, imm_name); recv_all(s)
send_line(s, imm_pw); recv_all(s)
send_line(s, "1"); recv_all(s)
cmd(s, "color off")

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Stacking Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
check("Stacking Sandbox" in cmd(s, f"goto {ROOM}"), "goto lands in the SQL-bootstrapped sandbox room")

# --- 1: three identical mobs stack into one line with (x3) ---
gremlin = f"gremlin{_suffix}"
make_mob(MOBA1, gremlin)
make_mob(MOBA2, gremlin)
make_mob(MOBA3, gremlin)
for vnum in (MOBA1, MOBA2, MOBA3):
    check("You conjure" in cmd(s, f"load mob {vnum}"), f"gremlin vnum {vnum} loaded")

out = cmd(s, "look")
check(f"A {gremlin} is here. (x3)" in out, "three identical gremlins stack into one line with (x3)")
check(out.count(f"A {gremlin} is here.") == 1, "no separate unstacked gremlin lines remain")

# --- 2: a lone, different mob shows with no (xN) suffix ---
goblin = f"goblin{_suffix}"
make_mob(MOBB, goblin)
check("You conjure" in cmd(s, f"load mob {MOBB}"), "the lone goblin is loaded")
out = cmd(s, "look")
check(f"A {goblin} is here.\r\n" in out, "a single mob shows with no (xN) suffix")
check(f"A {goblin} is here. (x" not in out, "the lone goblin is never merged with the gremlin group")

# --- 3: three identical loose objects stack into one line with (x3) ---
widget = f"widget{_suffix}"
for vnum in (OBJA1, OBJA2, OBJA3):
    sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
        f"VALUES ({vnum},'{widget}','a {widget}','A {widget} is lying here.',12,1,1);")
    check("You conjure" in cmd(s, f"load obj {vnum}"), f"widget vnum {vnum} loaded")

out = cmd(s, "look")
check(f"A {widget} is lying here. (x3)" in out, "three identical widgets stack into one line with (x3)")
check(out.count(f"A {widget} is lying here.") == 1, "no separate unstacked widget lines remain")

s.close()
announce_done("smoke_test_room_stacking", host, port)
print("=== ALL CHECKS PASSED ===")
