#!/usr/bin/env python3
"""Smoke test for `get all.<name> <container>` (user, 2026-08-04 follow-up
to the bare `get all.<name>` room-floor form: "get from containers?").
cmd_object.c's `get all <container>` (bare, empties everything) now also
accepts a name filter, same `all.<name>` convention `get`/`remove`/`sell`/
`drop` already use elsewhere.

Covers:
  1. `get all.<name> <container>` pulls only the matching item(s) out of
     a container, leaving an unrelated item inside untouched.
  2. Bare `get all <container>` (no filter) still empties everything,
     unaffected by the new filtered form sharing its code path.

    python3 tests/smoke_test_get_all_container.py [host] [port]
"""
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done
from mud_creation import create_character

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 920000 + (int(time.time()) % 60000)
BAG = ROOM + 1
DAGGER = ROOM + 2
SWORD = ROOM + 3
POUCH = ROOM + 4
GIZMO1 = ROOM + 5
GIZMO2 = ROOM + 6

WEAR_TAKE = 1
WEAR_HOLD = 16384
TYPE_CONTAINER = 15
TYPE_WEAPON = 5
TYPE_OTHER = 12


def set_level(name, level):
    sql(f"UPDATE player_progress SET level={level} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def make_char(sock, name, pw):
    recv_all(sock)
    for step in (name, "y", pw, pw):
        send_line(sock, step)
        recv_all(sock)
    create_character(sock, name, send_line, recv_all)


announce("smoke_test_get_all_container", host, port)

imm_name = f"Gacimm{_suffix}"
imm_pw = "gacimmpw12345"

s = socket.create_connection((host, port), timeout=5)
make_char(s, imm_name, imm_pw)
s.close()
set_level(imm_name, 51)

s = socket.create_connection((host, port), timeout=5)
recv_all(s)
for step in (imm_name, imm_pw, "1"):
    send_line(s, step)
    recv_all(s)
cmd(s, "color off")

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'GetAllContainer Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
check("GetAllContainer" in cmd(s, f"goto {ROOM}"), "goto lands in the sandbox room")

sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,val0,val1,weight,can_be_seen) "
    f"VALUES ({BAG},'gacbag bag','a gacbag test bag','A gacbag test bag is lying here.',"
    f"{TYPE_CONTAINER},{WEAR_TAKE},100,0,5,1);")
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,price,material,can_be_seen) "
    f"VALUES ({DAGGER},'gacdagger dagger','a gacdagger test dagger','A gacdagger test dagger is lying here.',"
    f"{TYPE_WEAPON},{WEAR_TAKE | WEAR_HOLD},100,0,1);")
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({SWORD},'gacsword sword','a gacsword test sword','A gacsword test sword is lying here.',"
    f"{TYPE_OTHER},{WEAR_TAKE},1);")

check("You conjure" in cmd(s, f"load obj {BAG}"), "the bag is loaded")
check("You conjure" in cmd(s, f"load obj {DAGGER}"), "the dagger is loaded")
check("You conjure" in cmd(s, f"load obj {SWORD}"), "the sword is loaded")
cmd(s, "put dagger bag")
cmd(s, "put sword bag")
drop_out = cmd(s, "drop bag")
check("you drop" in drop_out.lower(), "the loaded bag (now containing both items) is dropped")

# --- 1: get all.dagger bag pulls only the dagger, leaves the sword ---
out = cmd(s, "get all.dagger bag")
check("gacdagger test dagger" in out.lower(), "the filtered container-get reports the dagger")
check("gacsword test sword" not in out.lower(), "the filtered container-get leaves the sword message out")

look_bag = cmd(s, "look in bag")
check("gacsword test sword" in look_bag.lower(), "the sword is still inside the bag")
check("gacdagger test dagger" not in look_bag.lower(), "the dagger is gone from the bag")

inv = cmd(s, "inventory", timeout=2.0)
check("gacdagger test dagger" in inv.lower(), "the dagger landed in the player's own inventory")
if "ENTER for more" in inv:
    cmd(s, "q", timeout=1.0)  # drain the inventory pager so it doesn't eat the next command

# --- 2: bare `get all <container>` still empties everything ---
out2 = cmd(s, "get all bag")
check("gacsword test sword" in out2.lower(), "bare `get all <container>` still grabs the remaining sword")

look_bag2 = cmd(s, "look in bag")
check("nothing" in look_bag2.lower(), "the bag is empty after the bare get-all")

s.close()

sql(f"DELETE FROM room WHERE vnum={ROOM};")
sql(f"DELETE FROM obj WHERE vnum IN ({BAG}, {DAGGER}, {SWORD}, {POUCH}, {GIZMO1}, {GIZMO2});")

announce_done("smoke_test_get_all_container", host, port)
print("=== ALL CHECKS PASSED ===")
