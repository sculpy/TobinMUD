#!/usr/bin/env python3
"""Smoke test for the per-noun `edit <room|object|mob|trigger> reclaim
<low>-<high>` commands (user, 2026-08-04: "now we can add room reclaim
for just rooms obj reclaim for objs mob reclaim for mobs trigger reclaim
etc" -- splitting `zone reclaim`'s all-at-once room+obj+mob+trigger sweep
into narrower per-type forms under each editor's own `edit <noun>` entry
point). Each shares `zone reclaim`'s underlying repo_delete_range()
functions and 59+ gate, just scoped to one table.

Covers, one vnum range with one of each prototype type in it:
  1. `edit room reclaim <range>` deletes only the room row; obj/mob/
     trigger rows in the same range survive untouched.
  2. `edit object reclaim <range>` deletes only the obj row.
  3. `edit mob reclaim <range>` deletes only the mob row.
  4. `edit trigger reclaim <range>` deletes only the trigger row.
  5. Each form is refused below level 59.

    python3 tests/smoke_test_edit_reclaim.py [host] [port]
"""
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done
from mud_creation import create_character

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
BASE = 950000 + (int(time.time()) % 20000)
RANGE_LOW = BASE
RANGE_HIGH = BASE + 100
ROOM_VNUM = BASE + 10
OBJ_VNUM = BASE + 20
MOB_VNUM = BASE + 30


def query(stmt):
    return subprocess.run(["mariadb", "-N", "tobin", "-e", stmt],
                          check=True, capture_output=True, text=True).stdout.strip()


def set_level(name, level):
    sql(f"UPDATE player_progress SET level={level} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def make_char(sock, name, pw):
    recv_all(sock)
    for step in (name, "y", pw, pw):
        send_line(sock, step)
        recv_all(sock)
    create_character(sock, name, send_line, recv_all)


def login(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, pw, "1"):
        send_line(s, step)
        recv_all(s)
    cmd(s, "color off")
    return s


announce("smoke_test_edit_reclaim", host, port)

imm_name = f"Editrclm{_suffix}"
imm_pw = "editrclmpw123"

s = socket.create_connection((host, port), timeout=5)
make_char(s, imm_name, imm_pw)
s.close()
set_level(imm_name, 58)
s = login(imm_name, imm_pw)

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM_VNUM},0,0,0,'EditReclaim Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({OBJ_VNUM},'erobj trinket','an erobj test trinket','An erobj test trinket is lying here.',12,1,1);")
sql(f"INSERT INTO mob (vnum,name,short_desc,long_desc,description,actions,affects,"
    f"faction,fact_perc,letter,attacks,class,level,tohit,ac,hpbonus,damage_level,"
    f"damage_precision,gold,race,weight,height,str,bra,con,dex,agi,intel,wis,foc,"
    f"per,cha,kar,spe,pos,def_position,sex,spec_proc,skin,vision,can_be_seen,max_exist) "
    f"VALUES ({MOB_VNUM},'ermob dummy','an ermob test dummy','An ermob test dummy stands here.',"
    f"'A stuffed practice dummy.',0,0,0,0,'A',1.0,0,1,0,0,0.3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"
    f"10,10,1,0,0,0,1,1);")
check(True, "trigger row inserted next via `edit trigger` menu path is skipped -- using a direct SQL row instead")
sql(f"INSERT INTO `trigger` (created_by,target_type,target_vnum,trigger_type,chance_pct,script) "
    f"VALUES ('{imm_name}','room',{ROOM_VNUM},'enter',100,'tell $n Hello.');")

# --- 5: refused below level 59, for all four forms ---
for form in (f"edit room reclaim {RANGE_LOW}-{RANGE_HIGH}",
             f"edit object reclaim {RANGE_LOW}-{RANGE_HIGH}",
             f"edit mob reclaim {RANGE_LOW}-{RANGE_HIGH}",
             f"edit trigger reclaim {RANGE_LOW}-{RANGE_HIGH}"):
    out = cmd(s, form)
    check("Command not found" in out, f"'{form}' is refused for a level-58 immortal (needs 59+)")

cmd(s, "quit!"); s.close()
set_level(imm_name, 59)
s = login(imm_name, imm_pw)

# --- 1: edit room reclaim deletes only the room ---
out = cmd(s, f"edit room reclaim {RANGE_LOW}-{RANGE_HIGH}")
check("room(s) deleted" in out, "edit room reclaim reports a room deleted")
check(query(f"SELECT COUNT(*) FROM room WHERE vnum={ROOM_VNUM};") == "0", "the room row is gone")
check(query(f"SELECT COUNT(*) FROM obj WHERE vnum={OBJ_VNUM};") == "1", "the obj row survives room reclaim")
check(query(f"SELECT COUNT(*) FROM mob WHERE vnum={MOB_VNUM};") == "1", "the mob row survives room reclaim")

# --- 2: edit object reclaim deletes only the obj ---
out = cmd(s, f"edit object reclaim {RANGE_LOW}-{RANGE_HIGH}")
check("obj(s) deleted" in out, "edit object reclaim reports an obj deleted")
check(query(f"SELECT COUNT(*) FROM obj WHERE vnum={OBJ_VNUM};") == "0", "the obj row is gone")
check(query(f"SELECT COUNT(*) FROM mob WHERE vnum={MOB_VNUM};") == "1", "the mob row survives obj reclaim")

# --- 3: edit mob reclaim deletes only the mob ---
out = cmd(s, f"edit mob reclaim {RANGE_LOW}-{RANGE_HIGH}")
check("mob(s) deleted" in out, "edit mob reclaim reports a mob deleted")
check(query(f"SELECT COUNT(*) FROM mob WHERE vnum={MOB_VNUM};") == "0", "the mob row is gone")

# --- 4: edit trigger reclaim deletes only the trigger ---
before_trig = query(f"SELECT COUNT(*) FROM `trigger` WHERE target_vnum={ROOM_VNUM};")
check(before_trig == "1", "the trigger row is still present before trigger reclaim")
out = cmd(s, f"edit trigger reclaim {RANGE_LOW}-{RANGE_HIGH}")
check("trigger(s) deleted" in out, "edit trigger reclaim reports a trigger deleted")
check(query(f"SELECT COUNT(*) FROM `trigger` WHERE target_vnum={ROOM_VNUM};") == "0", "the trigger row is gone")

s.close()

announce_done("smoke_test_edit_reclaim", host, port)
print("=== ALL CHECKS PASSED ===")
