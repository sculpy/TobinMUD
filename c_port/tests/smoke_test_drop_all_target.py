#!/usr/bin/env python3
"""Smoke test for `drop all.<target>` (user, 2026-08-04: "and drop
all.target", same `all`/`all.<name>` convention `get`/`remove`/`sell`
already use). cmd_object.c's `drop all` (bare, drops every loose carried
item) now also accepts a name filter.

Covers:
  1. `drop all.<name>` drops only matching loose carried items, leaving
     an unrelated carried item untouched.
  2. Bare `drop all` (no filter) still drops everything loose remaining.
  3. `drop all.<bogus>` with nothing matching reports cleanly.

    python3 tests/smoke_test_drop_all_target.py [host] [port]
"""
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done
from mud_creation import create_character

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 925000 + (int(time.time()) % 60000)
DAGGER = ROOM + 1
SWORD = ROOM + 2

WEAR_TAKE = 1
WEAR_HOLD = 16384
TYPE_WEAPON = 5


def set_level(name, level):
    sql(f"UPDATE player_progress SET level={level} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def make_char(sock, name, pw):
    recv_all(sock)
    for step in (name, "y", pw, pw):
        send_line(sock, step)
        recv_all(sock)
    create_character(sock, name, send_line, recv_all)


announce("smoke_test_drop_all_target", host, port)

imm_name = f"Dratimm{_suffix}"
imm_pw = "dratimmpw123"

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
    f"VALUES ({ROOM},0,0,0,'DropAllTarget Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
check("DropAllTarget" in cmd(s, f"goto {ROOM}"), "goto lands in the sandbox room")

sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,price,material,can_be_seen) "
    f"VALUES ({DAGGER},'dratdagger dagger','a dratdagger test dagger','A dratdagger test dagger is lying here.',"
    f"{TYPE_WEAPON},{WEAR_TAKE | WEAR_HOLD},100,0,1);")
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,price,material,can_be_seen) "
    f"VALUES ({SWORD},'dratsword sword','a dratsword test sword','A dratsword test sword is lying here.',"
    f"{TYPE_WEAPON},{WEAR_TAKE | WEAR_HOLD},100,0,1);")

check("You conjure" in cmd(s, f"load obj {DAGGER}"), "the dagger is loaded (lands in inventory)")
check("You conjure" in cmd(s, f"load obj {SWORD}"), "the sword is loaded (lands in inventory)")

# --- 1: drop all.dagger drops only the dagger ---
out = cmd(s, "drop all.dagger")
check("dratdagger test dagger" in out.lower(), "the filtered drop reports the dagger")
check("dratsword test sword" not in out.lower(), "the filtered drop leaves the sword message out")

inv = cmd(s, "inventory", timeout=2.0)
check("dratsword test sword" in inv.lower(), "the sword is still carried after the filtered drop")
check("dratdagger test dagger" not in inv.lower(), "the dagger is gone from inventory")
if "ENTER for more" in inv:
    cmd(s, "q", timeout=1.0)  # drain the inventory pager so it doesn't eat the next command

# --- 2: bare drop all drops everything remaining ---
out2 = cmd(s, "drop all")
check("dratsword test sword" in out2.lower(), "bare `drop all` drops the remaining sword")

inv2 = cmd(s, "inventory", timeout=2.0)
check("dratsword test sword" not in inv2.lower(), "the sword is gone from inventory after bare drop all")
if "ENTER for more" in inv2:
    cmd(s, "q", timeout=1.0)

# --- 3: drop all.<bogus> with nothing matching reports cleanly ---
out3 = cmd(s, f"drop all.nonexistent{_suffix}")
check("aren't carrying any of those" in out3.lower(), "drop all.<bogus> is refused cleanly, no crash")

s.close()

sql(f"DELETE FROM room WHERE vnum={ROOM};")
sql(f"DELETE FROM obj WHERE vnum IN ({DAGGER}, {SWORD});")

announce_done("smoke_test_drop_all_target", host, port)
print("=== ALL CHECKS PASSED ===")
