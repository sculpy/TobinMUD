#!/usr/bin/env python3
"""Smoke test for NEWBIE-flagged items disappearing on drop (TODO.md
priority item, user 2026-08-02). Ported from SneezyMUD's own `drop`
(misc/inventory.cc): an ITEM_NEWBIE-flagged item explodes in a flash of
white light when dropped on the floor, EXCEPT when it's a non-empty
container (a newbie-issue bag full of real loot shouldn't vanish along
with its contents).

    python3 tests/smoke_test_newbie_item_drop.py [host] [port]
"""
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 920000 + (int(time.time()) % 60000)
NEWBIE_ITEM = ROOM + 1
NORMAL_ITEM = ROOM + 2
NEWBIE_BAG = ROOM + 3
FILLER_ITEM = ROOM + 4

WEAR_TAKE = 1
TYPE_OTHER = 1
TYPE_CONTAINER = 15
ACTION_NEWBIE = 1 << 24


announce("smoke_test_newbie_item_drop", host, port)


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
    send_line(sock, "1"); recv_all(sock)  # race: human
    send_line(sock, "1"); recv_all(sock)  # territory: urban
    send_line(sock, "1"); recv_all(sock)  # class: mage
    send_line(sock, "done"); recv_all(sock)
    send_line(sock, "done"); recv_all(sock)  # alignment: neutral


imm_name = f"Nwbimm{_suffix}"
imm_pw = "nwbimmpw123"
mort_name = f"Nwbmor{_suffix}"
mort_pw = "nwbmorpw123"

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
    f"VALUES ({ROOM},0,0,0,'Newbie-Drop Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
check("Newbie-Drop Sandbox" in cmd(s, f"goto {ROOM}"), "goto lands in the SQL-bootstrapped sandbox room")

sv = socket.create_connection((host, port), timeout=5)
make_char(sv, mort_name, mort_pw)
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{mort_name}';")
cmd(sv, "quit!")
sv.close()
sv = socket.create_connection((host, port), timeout=5)
recv_all(sv)
send_line(sv, mort_name); recv_all(sv)
send_line(sv, mort_pw); recv_all(sv)
send_line(sv, "1"); recv_all(sv)
cmd(sv, "color off")
check("Newbie-Drop Sandbox" in cmd(sv, "look"), "the mortal lands directly in the sandbox room")

sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,action_flag,weight,val0,can_be_seen) "
    f"VALUES ({NEWBIE_ITEM},'newbie cloak','a newbie cloak','A newbie cloak is lying here.\\n',"
    f"{TYPE_OTHER},{WEAR_TAKE},{ACTION_NEWBIE},2,0,1),"
    f"({NORMAL_ITEM},'plain rock','a plain rock','A plain rock is lying here.\\n',"
    f"{TYPE_OTHER},{WEAR_TAKE},0,1,0,1),"
    f"({NEWBIE_BAG},'newbie sack','a newbie sack','A newbie sack is lying here.\\n',"
    f"{TYPE_CONTAINER},{WEAR_TAKE},{ACTION_NEWBIE},3,50,1),"
    f"({FILLER_ITEM},'small pebble','a small pebble','A small pebble is lying here.\\n',"
    f"{TYPE_OTHER},{WEAR_TAKE},0,1,0,1);")

# --- 1: a plain (non-NEWBIE) item survives a drop normally ---
check("You conjure" in cmd(s, f"load obj {NORMAL_ITEM}"), "the plain rock is loaded")
cmd(s, "drop rock")
check("you get" in cmd(sv, "get rock").lower(), "the mortal picks up the plain rock")
out = cmd(sv, "drop rock")
check("you drop" in out.lower() and "explodes" not in out.lower(),
      "dropping a normal item does NOT explode")
check("plain rock" in cmd(sv, "look").lower(), "the plain rock is still on the floor after dropping")

# --- 2: a NEWBIE item explodes in a flash of white light on drop ---
# `load` puts the item straight into the loader's own inventory (dropping
# it would explode it immediately, even for the immortal setting this up)
# -- `give` it to the mortal directly instead of routing through the floor.
check("You conjure" in cmd(s, f"load obj {NEWBIE_ITEM}"), "the newbie cloak is loaded")
check("you give" in cmd(s, f"give cloak {mort_name}").lower(), "the immortal hands the cloak to the mortal")
out = cmd(sv, "drop cloak")
check("you drop" in out.lower(), "the drop message still shows")
check("explodes in a flash of white light" in out.lower(), "the newbie cloak explodes on drop")
check("newbie cloak" not in cmd(sv, "look").lower(), "the newbie cloak is gone from the room floor")
check("newbie cloak" not in cmd(sv, "inventory").lower(), "the newbie cloak is gone from inventory too")

# --- 3: a NEWBIE container WITH contents does NOT explode (loot inside) ---
check("You conjure" in cmd(s, f"load obj {NEWBIE_BAG}"), "the newbie sack is loaded")
check("You conjure" in cmd(s, f"load obj {FILLER_ITEM}"), "the filler pebble is loaded")
cmd(s, "put pebble sack")
check("you give" in cmd(s, f"give sack {mort_name}").lower(), "the immortal hands the non-empty sack to the mortal")
out = cmd(sv, "drop sack")
check("explodes" not in out.lower(), "a non-empty NEWBIE container does NOT explode on drop")
check("newbie sack" in cmd(sv, "look").lower(), "the non-empty newbie sack is still on the floor")

s.close()
sv.close()
announce_done("smoke_test_newbie_item_drop", host, port)
print("=== ALL CHECKS PASSED ===")
