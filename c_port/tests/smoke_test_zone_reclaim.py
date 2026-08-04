#!/usr/bin/env python3
"""Smoke test for `zone reclaim <low>-<high>` (user, 2026-08-04: a
one-shot way to wipe an entire ad hoc vnum range out of the world --
rooms, objects, mobs, and satellite rows, plus any zone_reset/trigger
rows referencing that range -- unlike `purge <range>` (cmd_purge.c),
which only clears loose in-memory instances and never touches the DB
prototype/room rows themselves).

Covers:
  1. `zone reclaim <low>-<high>` is refused below level 59.
  2. At 59+, a player standing in an in-range room blocks the whole
     reclaim (refused, nothing deleted).
  3. Once the player leaves, the same range succeeds: the room/mob/obj
     DB rows are gone, and a room OUTSIDE the range survives untouched.

    python3 tests/smoke_test_zone_reclaim.py [host] [port]
"""
import re
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done
from mud_creation import create_character

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
BASE = 940000 + (int(time.time()) % 30000)
IN_ROOM = BASE + 10
OUT_ROOM = BASE + 500
IN_MOB = BASE + 11
IN_OBJ = BASE + 12
WEAR_TAKE = 1


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


announce("smoke_test_zone_reclaim", host, port)

imm_name = f"Zrclimm{_suffix}"
imm_pw = "zrclimmpw123"

s = socket.create_connection((host, port), timeout=5)
make_char(s, imm_name, imm_pw)
s.close()
set_level(imm_name, 58)
s = login(imm_name, imm_pw)

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({IN_ROOM},0,0,0,'Reclaim In-Range Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({OUT_ROOM},0,0,0,'Reclaim Out-Of-Range Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO mob (vnum,name,short_desc,long_desc,description,actions,affects,"
    f"faction,fact_perc,letter,attacks,class,level,tohit,ac,hpbonus,damage_level,"
    f"damage_precision,gold,race,weight,height,str,bra,con,dex,agi,intel,wis,foc,"
    f"per,cha,kar,spe,pos,def_position,sex,spec_proc,skin,vision,can_be_seen,max_exist) "
    f"VALUES ({IN_MOB},'reclaimdummy{_suffix}','a reclaim test dummy','A reclaim test dummy stands here.',"
    f"'A stuffed practice dummy.',0,0,0,0,'A',1.0,0,1,0,0,0.3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"
    f"10,10,1,0,0,0,1,1);")
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({IN_OBJ},'trinket','a reclaim test trinket','A reclaim test trinket is lying here.',12,{WEAR_TAKE},1);")

check("Reclaim In-Range" in cmd(s, f"goto {IN_ROOM}"), "goto lands in the in-range sandbox room")
check("You conjure" in cmd(s, f"load mob {IN_MOB}"), "the in-range test dummy is loaded")

# --- 1: refused below 59 ---
out = cmd(s, f"zone reclaim {BASE}-{BASE + 100}")
check("Command not found" in out, "zone reclaim is refused for a level-58 immortal (needs 59+)")

cmd(s, "quit!"); s.close()
set_level(imm_name, 59)
s = login(imm_name, imm_pw)

# --- 2: a player standing in-range blocks the whole reclaim ---
victim_name = f"Zrclvic{_suffix}"
victim_pw = "zrclvicpw123"
sv = socket.create_connection((host, port), timeout=5)
make_char(sv, victim_name, victim_pw)
sql(f"UPDATE player SET load_room={IN_ROOM} WHERE name='{victim_name}';")
cmd(sv, "quit!")
sv.close()
sv = login(victim_name, victim_pw)
check("Reclaim In-Range" in cmd(sv, "look"), "the victim lands directly in the in-range room")

out = cmd(s, f"zone reclaim {BASE}-{BASE + 100}")
check("refused" in out.lower(), "zone reclaim refuses while a player is standing in the range")

still_there = query(f"SELECT COUNT(*) FROM room WHERE vnum={IN_ROOM};")
check(still_there == "1", "the in-range room row survives the refused reclaim attempt")

# --- 3: player leaves, reclaim succeeds, out-of-range room survives ---
cmd(sv, "quit!")
sv.close()
time.sleep(0.5)

out = cmd(s, f"zone reclaim {BASE}-{BASE + 100}")
check("Reclaimed range" in out, "zone reclaim succeeds once the room is empty of players")
# >= 1, not ==: an unrelated leftover test room from a past session could
# coincidentally fall in this same random window (BASE is time-derived,
# and the DB still carries hundreds of never-cleaned-up sandbox rooms
# from before `zone reclaim` existed to actually delete them) -- that's
# not a bug in this feature, just pre-existing DB clutter.
m = re.search(r"(\d+) room\(s\)", out)
check(m is not None and int(m.group(1)) >= 1, "the summary reports at least the in-range room deleted")
m = re.search(r"(\d+) mob\(s\)", out)
check(m is not None and int(m.group(1)) >= 1, "the summary reports at least the in-range mob prototype deleted")

room_count = query(f"SELECT COUNT(*) FROM room WHERE vnum={IN_ROOM};")
check(room_count == "0", "the in-range room row is gone from the DB")
mob_count = query(f"SELECT COUNT(*) FROM mob WHERE vnum={IN_MOB};")
check(mob_count == "0", "the in-range mob prototype row is gone from the DB")
obj_count = query(f"SELECT COUNT(*) FROM obj WHERE vnum={IN_OBJ};")
check(obj_count == "0", "the in-range obj prototype row is gone from the DB (even though never loaded into the room)")

out_room_count = query(f"SELECT COUNT(*) FROM room WHERE vnum={OUT_ROOM};")
check(out_room_count == "1", "the out-of-range room survives untouched")

s.close()

sql(f"DELETE FROM room WHERE vnum={OUT_ROOM};")

announce_done("smoke_test_zone_reclaim", host, port)
print("=== ALL CHECKS PASSED ===")
