#!/usr/bin/env python3
"""Smoke test for `purge <low>-<high>` (user, 2026-08-04: asked for a
one-shot way to clean up ad hoc test-sandbox rooms/mobs instead of
walking rooms one at a time -- TODO.md's "leftover-mob accumulation"
follow-up). cmd_purge.c's new range form purges every currently LOADED
room's mobs/objects within [low, high] (never a player), gated at 59+
(one tier above `purge linkdead`'s 58, user 2026-08-04) since a mistyped
range could sweep real zone content.

Covers:
  1. `purge <low>-<high>` is refused below level 59, even for a 58+
     immortal who can already do `purge linkdead`.
  2. At 59+, a loose mob/object in a room WITHIN the range is purged.
  3. A loose mob/object in a room OUTSIDE the range survives.
  4. A player standing in an in-range room is never touched.

    python3 tests/smoke_test_purge_range.py [host] [port]
"""
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done
from mud_creation import create_character

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
BASE = 930000 + (int(time.time()) % 30000)
IN_ROOM = BASE + 10
OUT_ROOM = BASE + 500  # well outside the [BASE, BASE+100] range used below
IN_MOB = BASE + 11
OUT_MOB = BASE + 501

WEAR_TAKE = 1


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


announce("smoke_test_purge_range", host, port)

imm_name = f"Prgrimm{_suffix}"
imm_pw = "prgrimmpw123"

s = socket.create_connection((host, port), timeout=5)
make_char(s, imm_name, imm_pw)
s.close()
set_level(imm_name, 58)
s = login(imm_name, imm_pw)

for rv, mv, label in ((IN_ROOM, IN_MOB, "in-range"), (OUT_ROOM, OUT_MOB, "out-of-range")):
    sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
        f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
        f"VALUES ({rv},0,0,0,'PurgeRange {label} Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
    sql(f"INSERT INTO mob (vnum,name,short_desc,long_desc,description,actions,affects,"
        f"faction,fact_perc,letter,attacks,class,level,tohit,ac,hpbonus,damage_level,"
        f"damage_precision,gold,race,weight,height,str,bra,con,dex,agi,intel,wis,foc,"
        f"per,cha,kar,spe,pos,def_position,sex,spec_proc,skin,vision,can_be_seen,max_exist) "
        f"VALUES ({mv},'purgerange{label}{_suffix}','a purgerange {label} dummy','A purgerange {label} dummy stands here.',"
        f"'A stuffed practice dummy.',0,0,0,0,'A',1.0,0,1,0,0,0.3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"
        f"10,10,1,0,0,0,1,1);")
    check("PurgeRange" in cmd(s, f"goto {rv}"), f"goto lands in the {label} sandbox room")
    check("You conjure" in cmd(s, f"load mob {mv}"), f"the {label} test dummy is loaded")

# --- 1: refused below level 59, even for a 58+ immortal ---
out = cmd(s, f"purge {BASE}-{BASE + 100}")
check("Command not found" in out, "purge <range> is refused for a level-58 immortal (needs 59+)")

# --- 2/3/4: promote to 59, run the range purge, verify scoping ---
cmd(s, "quit!")
s.close()
set_level(imm_name, 59)
s = login(imm_name, imm_pw)

victim_name = f"Prgrvic{_suffix}"
victim_pw = "prgrvicpw123"
sv = socket.create_connection((host, port), timeout=5)
make_char(sv, victim_name, victim_pw)
sql(f"UPDATE player SET load_room={IN_ROOM} WHERE name='{victim_name}';")
cmd(sv, "quit!")
sv.close()
sv = login(victim_name, victim_pw)
check("PurgeRange in-range Sandbox" in cmd(sv, "look"), "the victim lands directly in the in-range sandbox room")

out = cmd(s, f"purge {BASE}-{BASE + 100}")
check("Purged" in out and "loaded room" in out, "purge <range> reports a summary")

check("PurgeRange in-range" in cmd(s, f"goto {IN_ROOM}"), "re-enter the in-range room")
out = cmd(s, "look")
check("purgerange in-range dummy" not in out.lower(), "the in-range dummy is purged")
check(victim_name.lower() in out.lower(), "the player standing in the in-range room is untouched")

check("PurgeRange out-of-range" in cmd(s, f"goto {OUT_ROOM}"), "re-enter the out-of-range room")
out = cmd(s, "look")
check("purgerange out-of-range dummy" in out.lower(), "the out-of-range dummy survives (outside the purged range)")

s.close()
sv.close()

sql(f"DELETE FROM room WHERE vnum IN ({IN_ROOM}, {OUT_ROOM});")
sql(f"DELETE FROM mob WHERE vnum IN ({IN_MOB}, {OUT_MOB});")

announce_done("smoke_test_purge_range", host, port)
print("=== ALL CHECKS PASSED ===")
