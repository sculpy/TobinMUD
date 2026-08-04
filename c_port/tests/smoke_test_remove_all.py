#!/usr/bin/env python3
"""Smoke test for `remove all`/`remove all.<target>` (TODO.md, user
2026-08-04: "remove all.target should [work] the same" as `get
all.<target>`). cmd_object.c's cmd_remove() previously only handled one
worn/held item at a time; remove_all_worn() now handles the bulk forms,
same `all`/`all.<name>` convention `get`/`sell` already use -- walks
equipment[] then held[], since those are the only two places a
"worn or held" item can be.

Covers:
  1. `remove all.<name>` removes only the matching worn item, leaving an
     unrelated held item untouched.
  2. `remove all` removes everything still worn/held.
  3. `remove all` with nothing worn/held reports cleanly (no crash).

    python3 tests/smoke_test_remove_all.py [host] [port]
"""
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done
from mud_creation import create_character

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 910000 + (int(time.time()) % 60000)
ARMOR_ITEM = ROOM + 1
DAGGER_ITEM = ROOM + 2

WEAR_TAKE = 1
WEAR_BODY = 8
WEAR_HOLD = 16384
TYPE_ARMOR = 9
TYPE_WEAPON = 5


def make_char(sock, name, pw):
    recv_all(sock)
    for step in (name, "y", pw, pw):
        send_line(sock, step)
        recv_all(sock)
    create_character(sock, name, send_line, recv_all)


def set_level(name, level):
    sql(f"UPDATE player_progress SET level={level} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


announce("smoke_test_remove_all", host, port)

imm_name = f"Rmallimm{_suffix}"
imm_pw = "rmallimmpw123"
mort_name = f"Rmallmor{_suffix}"
mort_pw = "rmallmorpw123"

s = socket.create_connection((host, port), timeout=5)
make_char(s, imm_name, imm_pw)
set_level(imm_name, 51)
s.close()
s = socket.create_connection((host, port), timeout=5)
recv_all(s)
for step in (imm_name, imm_pw, "1"):
    send_line(s, step)
    recv_all(s)
cmd(s, "color off")

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Remove-All Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
check("Remove-All Sandbox" in cmd(s, f"goto {ROOM}"), "goto lands in the SQL-bootstrapped sandbox room")

sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,weight,val0,can_be_seen) "
    f"VALUES ({ARMOR_ITEM},'rmall plate armor','a rmall test plate armor',"
    f"'A rmall test plate armor is lying here.',{TYPE_ARMOR},{WEAR_TAKE | WEAR_BODY},20,0,1);")
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,price,material,can_be_seen) "
    f"VALUES ({DAGGER_ITEM},'rmall dagger','a rmall test dagger','A rmall test dagger is lying here.',"
    f"{TYPE_WEAPON},{WEAR_TAKE | WEAR_HOLD},100,0,1);")
check("You conjure" in cmd(s, f"load obj {ARMOR_ITEM}"), "the plate armor is loaded")
cmd(s, "drop plate")
check("You conjure" in cmd(s, f"load obj {DAGGER_ITEM}"), "the dagger is loaded")
cmd(s, "drop dagger")

sv = socket.create_connection((host, port), timeout=5)
make_char(sv, mort_name, mort_pw)
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{mort_name}';")
cmd(sv, "quit!")
sv.close()
sv = socket.create_connection((host, port), timeout=5)
recv_all(sv)
for step in (mort_name, mort_pw, "1"):
    send_line(sv, step)
    recv_all(sv)
cmd(sv, "color off")
check("Remove-All Sandbox" in cmd(sv, "look"), "the mortal lands directly in the sandbox room")

check("you get" in cmd(sv, "get plate").lower(), "picks up the plate armor")
check("you get" in cmd(sv, "get dagger").lower(), "picks up the dagger")
check("wear" in cmd(sv, "wear plate").lower(), "wears the plate armor")
check("wield" in cmd(sv, "wield dagger").lower(), "wields the dagger")

# --- 1: remove all.<name> removes only the matching item ---
out = cmd(sv, "remove all.dagger")
check("rmall test dagger" in out.lower(), "removing all.dagger reports the dagger")
check("rmall test plate" not in out.lower(), "removing all.dagger leaves the armor untouched")

eq = cmd(sv, "equipment")
check("rmall test plate" in eq.lower(), "the armor is still worn after removing only the dagger")

# --- 2: remove all removes everything remaining ---
out = cmd(sv, "remove all")
check("rmall test plate" in out.lower(), "the bare `remove all` removes the remaining armor")

eq2 = cmd(sv, "equipment")
check("rmall test plate" not in eq2.lower(), "the armor is gone from equipment after `remove all`")

# --- 3: remove all with nothing worn/held reports cleanly ---
out = cmd(sv, "remove all")
check("aren't wearing or holding" in out.lower(), "`remove all` with nothing equipped reports cleanly, no crash")

sv.close()
s.close()

sql(f"DELETE FROM player_inventory WHERE player_id IN "
    f"(SELECT id FROM player WHERE name IN ('{imm_name}', '{mort_name}'));")
sql(f"DELETE FROM player_progress WHERE player_id IN "
    f"(SELECT id FROM player WHERE name IN ('{imm_name}', '{mort_name}'));")
sql(f"DELETE FROM player_attrs WHERE player_id IN "
    f"(SELECT id FROM player WHERE name IN ('{imm_name}', '{mort_name}'));")
sql(f"DELETE FROM player WHERE name IN ('{imm_name}', '{mort_name}');")
sql(f"DELETE FROM obj WHERE vnum IN ({ARMOR_ITEM}, {DAGGER_ITEM});")
sql(f"DELETE FROM room WHERE vnum={ROOM};")

announce_done("smoke_test_remove_all", host, port)
print("=== ALL CHECKS PASSED ===")
