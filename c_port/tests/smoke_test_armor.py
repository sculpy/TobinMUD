#!/usr/bin/env python3
"""Smoke test for armor class (user 2026-07-11: "Armor & protection (AC)
go in next, complete the to-hit / defense formula depth"). Covers:

  1. A fresh character shows "Armor Class: 0" in `score` -- unarmored.
  2. Wearing a heavy armor piece raises it. The real seeded `obj` table's
     armor rows all have val0=0 (no per-item AC data ever populated), so
     obj_armor_ac() derives AC from the item's weight instead -- this
     confirms that derivation actually runs, not just that a hardcoded
     stat displays.
  3. Removing the armor drops Armor Class back to 0.

    python3 tests/smoke_test_armor.py [host] [port]
"""
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


announce("smoke_test_armor", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 900000 + (int(time.time()) % 70000)
ARMOR_ITEM = ROOM + 1

WEAR_TAKE = 1
WEAR_BODY = 8
TYPE_ARMOR = 9  # ITEM_ARMOR


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


imm_name = f"Armorimm{_suffix}"
imm_pw = "armorimmpw123"
mort_name = f"Armormor{_suffix}"
mort_pw = "armormorpw123"

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
    f"VALUES ({ROOM},0,0,0,'Armor Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
check("Armor Sandbox" in cmd(s, f"goto {ROOM}"), "goto lands in the SQL-bootstrapped sandbox room")

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
check("Armor Sandbox" in cmd(sv, "look"), "the mortal lands directly in the sandbox room")

# --- 1: unarmored, Armor Class: 0 ---
out = cmd(sv, "score")
check("Armor Class: 0" in out, "a fresh character shows Armor Class: 0")

# A heavy (weight 20) armor item -- val0=0 like every real seeded armor
# row, so this only shows a nonzero AC at all if obj_armor_ac() derives
# it from weight rather than reading the (always-zero) DB field.
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,weight,val0,can_be_seen) "
    f"VALUES ({ARMOR_ITEM},'heavy plate armor','a suit of heavy plate armor',"
    f"'A suit of heavy plate armor is lying here.',{TYPE_ARMOR},{WEAR_TAKE | WEAR_BODY},20,0,1);")
check("You conjure" in cmd(s, f"load obj {ARMOR_ITEM}"), "the heavy plate armor is loaded")
cmd(s, "drop plate")

out = cmd(sv, "get plate")
check("you get" in out.lower(), "the mortal picks up the plate armor")
out = cmd(sv, "wear plate")
check("wear" in out.lower(), "wear equips the plate armor")

# --- 2: armored, Armor Class rises (weight 20 * 2 per point, capped at 30) ---
out = cmd(sv, "score")
check("Armor Class: 30" in out, "wearing heavy armor raises Armor Class (derived from weight, capped at 30)")

# --- 3: remove it, Armor Class drops back to 0 ---
out = cmd(sv, "remove plate")
check("remove" in out.lower(), "remove takes the armor back off")
out = cmd(sv, "score")
check("Armor Class: 0" in out, "removing the armor drops Armor Class back to 0")

s.close()
sv.close()
announce_done("smoke_test_armor", host, port)
print("=== ALL CHECKS PASSED ===")
