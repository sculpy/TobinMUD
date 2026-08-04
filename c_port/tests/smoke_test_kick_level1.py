#!/usr/bin/env python3
"""Smoke test for kick being available at level 1 for every class that
has it (user, 2026-08-04: "kick skill should be received at level 1 for
all classes who get the skill"). `kick` was already level 1 for Thief
(skill.c's SKILLS[] roster) but level 3 for Monk -- the only two classes
that carry it -- now both are level 1.

Covers:
  1. A freshly-created level-1 Monk can already `kick` a target (not
     refused for being under-level).
  2. A freshly-created level-1 Thief can already `kick` a target too
     (the pre-existing case, confirmed not regressed).

    python3 tests/smoke_test_kick_level1.py [host] [port]
"""
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 960000 + (int(time.time()) % 30000)
MOB = ROOM + 1


def make_char(name, pw, class_num):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw, "new", name, "1", "1", str(class_num), "done", "done"):
        # steps: name, confirm, pw, pw confirm, "new", char name, race=1, homeland=1, class, done (attrs), done (finish)
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


announce("smoke_test_kick_level1", host, port)

imm_name, imm_pw = f"Kkl{_suffix}", "kklpw1234567"

s1 = make_char(imm_name, imm_pw, 3)  # Warrior (level 51+ needed for `load`/`goto`)
cmd(s1, "quit!"); s1.close()
sql(f"UPDATE player_progress SET level=59 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{imm_name}');")
s1 = relog(imm_name, imm_pw)

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Kick Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
check("Kick Sandbox" in cmd(s1, f"goto {ROOM}"), "goto lands in the SQL-bootstrapped sandbox room")

sql(f"INSERT INTO mob (vnum,name,short_desc,long_desc,description,actions,affects,"
    f"faction,fact_perc,letter,attacks,class,level,tohit,ac,hpbonus,damage_level,"
    f"damage_precision,gold,race,weight,height,str,bra,con,dex,agi,intel,wis,foc,"
    f"per,cha,kar,spe,pos,def_position,sex,spec_proc,skin,vision,can_be_seen,max_exist) "
    f"VALUES ({MOB},'kickdummy{_suffix}','a kick test dummy','A kick test dummy stands here.',"
    f"'A stuffed practice dummy.',0,0,0,0,'A',1.0,0,1,0,999,0.1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"
    f"10,10,1,0,0,0,1,1);")

for name, class_num in ((f"Kkm{_suffix}", 6), (f"Kkt{_suffix}", 4)):  # Monk=6, Thief=4
    pw = "kkclasspw123"
    sc = make_char(name, pw, class_num)
    cmd(sc, "quit!"); sc.close()
    sql(f"UPDATE player SET load_room={ROOM} WHERE name='{name}';")
    sc = relog(name, pw)
    check("Kick Sandbox" in cmd(sc, "look"), f"{name} lands in the sandbox room")
    check("You conjure" in cmd(s1, f"load mob {MOB}"), f"a fresh dummy is loaded for {name}")
    out = cmd(sc, "kick dummy")
    check("you don't know how to kick" not in out.lower() and "not high enough level" not in out.lower(),
          f"a level-1 {name}'s class is never refused kick for being under-level")
    sc.close()
    cmd(s1, "purge")

s1.close()

announce_done("smoke_test_kick_level1", host, port)
print("=== ALL CHECKS PASSED ===")
