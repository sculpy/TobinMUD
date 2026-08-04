#!/usr/bin/env python3
"""Smoke test for the `nospam` toggle (user 2026-07-11: "add a nospam
toggle where the games output during fights doesnt show missed hits in
messages and logs" -- ported from Sneezy's AUTO_NOSPAM, toggle.h/
combat.cc, which gates a miss message independently per viewer). Covers:

  1. `toggle nospam` appears in the personal toggle list, default off.
  2. With nospam off, a guaranteed-miss attacker sees "You miss ...".
  3. With nospam on, the same guaranteed-miss attack shows no miss text.

A weapon with an absurdly negative APPLY_HITROLL (objaffect type 15,
same mechanism smoke_test_weapon_messaging.py uses in reverse to force
guaranteed HITS) forces every swing to miss deterministically, instead of
waiting on ordinary ~50% RNG.

    python3 tests/smoke_test_nospam.py [host] [port]
"""
import re
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


announce("smoke_test_nospam", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 900000 + (int(time.time()) % 70000)
MOB1 = ROOM + 1
MOB2 = ROOM + 2
SWORD = ROOM + 3

WEAR_TAKE = 1
WEAR_HOLD = 16384
TYPE_WEAPON = 5


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
    send_line(sock, "1"); recv_all(sock)  # race: human (zero stat modifier)
    send_line(sock, "1"); recv_all(sock)  # territory: urban
    send_line(sock, "1"); recv_all(sock)  # class: mage
    send_line(sock, "done"); recv_all(sock)
    send_line(sock, "done"); recv_all(sock)  # alignment: neutral


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
        f"VALUES ({vnum},'{keyword}','a nospam test dummy','A nospam test dummy stands here.',"
        f"'A stuffed practice dummy.',0,0,0,0,'A',1.0,0,1,0,0,0.3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"
        f"10,10,1,0,0,0,1,1);")


imm_name = f"Nospamimm{_suffix}"
imm_pw = "nospamimmpw123"
mort_name = f"Nospammor{_suffix}"
mort_pw = "nospammorpw123"

s = socket.create_connection((host, port), timeout=5)
make_char(s, imm_name, imm_pw)
set_level(imm_name, 51)
s.close()
s = login(imm_name, imm_pw)

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Nospam Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
check("Nospam Sandbox" in cmd(s, f"goto {ROOM}"), "goto lands in the SQL-bootstrapped sandbox room")

sv = socket.create_connection((host, port), timeout=5)
make_char(sv, mort_name, mort_pw)
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{mort_name}';")
cmd(sv, "quit!")
sv.close()
sv = login(mort_name, mort_pw)
check("Nospam Sandbox" in cmd(sv, "look"), "the mortal attacker lands directly in the sandbox room")

# --- 1: nospam appears in `toggle`, default off ---
out = cmd(sv, "toggle")
check("nospam" in out.lower(), "nospam appears in the personal toggle list")
check(re.search(r"nospam\s+off", out.lower()), "nospam defaults to off")

# A weapon with an absurdly negative hitroll guarantees every swing misses
# (same mechanism smoke_test_weapon_messaging.py uses in reverse for
# guaranteed hits) -- deterministic instead of waiting on ~50% RNG.
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({SWORD},'clumsy club testclub','a clumsy club','A clumsy club is lying here.',"
    f"{TYPE_WEAPON},{WEAR_TAKE | WEAR_HOLD},1);")
sql(f"INSERT INTO objaffect (vnum, type, mod1, mod2) VALUES ({SWORD}, 15, -1000, 0);")  # APPLY_HITROLL

make_mob(MOB1, f"nospamdummy1{_suffix}")
check("You conjure" in cmd(s, f"load mob {MOB1}"), "the first dummy is loaded")
check("You conjure" in cmd(s, f"load obj {SWORD}"), "the clumsy club is loaded")
cmd(s, "drop club")

out = cmd(sv, "get club")
check("you get" in out.lower(), "the mortal attacker picks up the clumsy club")
out = cmd(sv, "wield club")
check("wield" in out.lower(), "wield equips the clumsy club")

# --- 2: nospam off -- the guaranteed miss is shown ---
out = cmd(sv, f"attack nospamdummy1{_suffix}")
check("You attack" in out, "attack initiated with nospam off")

found_miss = False
chunks = [out]
for _ in range(6):
    time.sleep(1.5)
    chunk = recv_all(sv, timeout=0.5)
    chunks.append(chunk)
    if "You miss" in chunk:
        found_miss = True
        break
check(found_miss, "with nospam off, the guaranteed-miss attacker sees 'You miss'")

cmd(sv, "flee")
recv_all(sv, timeout=1.0)

# --- 3: nospam on -- the same guaranteed miss is now silent ---
out = cmd(sv, "toggle nospam")
check("nospam is now" in out.lower() and "on" in out.lower(), "toggle nospam turns it on")

make_mob(MOB2, f"nospamdummy2{_suffix}")
check("You conjure" in cmd(s, f"load mob {MOB2}"), "the second dummy is loaded")

out = cmd(sv, f"attack nospamdummy2{_suffix}")
check("You attack" in out, "attack initiated with nospam on")

saw_any_miss = "You miss" in out
chunks = [out]
for _ in range(6):
    time.sleep(1.5)
    chunk = recv_all(sv, timeout=0.5)
    chunks.append(chunk)
    if "You miss" in chunk:
        saw_any_miss = True
check(not saw_any_miss, "with nospam on, the guaranteed-miss attack never shows 'You miss'")

s.close()
sv.close()
announce_done("smoke_test_nospam", host, port)
print("=== ALL CHECKS PASSED ===")
