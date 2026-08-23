#!/usr/bin/env python3
"""Smoke test for the `autoloot` toggle (user, 2026-07-12: "add an
autoloot toggle where a player upon opponent death automatically loots
all from the corpse"). Covers:

  1. `toggle autoloot` flips it on/off, listed in the `toggle` menu.
  2. With autoloot ON, an immortal's instakill automatically moves the
     victim's gear out of the corpse and into the winner's inventory,
     with a confirmation message.
  3. With autoloot OFF, a kill leaves the gear sitting in the corpse
     instead (the existing, pre-autoloot behavior).

    python3 tests/smoke_test_autoloot.py [host] [port]
"""
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


announce("smoke_test_autoloot", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time() * 1000) // 26**i) % 26) for i in range(4))
ROOM = 900000 + (int(time.time() * 1000000) % 90000)
ITEM1 = ROOM + 1
ITEM2 = ROOM + 2

WEAR_TAKE = 1
WEAR_BODY = 8


def make_char(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "y"); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "new"); recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "1"); recv_all(s)  # race
    send_line(s, "1"); recv_all(s)  # territory: urban
    send_line(s, "1"); recv_all(s)  # class
    send_line(s, "done"); recv_all(s)
    send_line(s, "done"); recv_all(s)  # alignment
    cmd(s, "color off")
    return s


pw = "autolootpw123"

imm_name = f"Autolootimm{_suffix}"
s_imm = make_char(imm_name, pw)
cmd(s_imm, "quit!")
s_imm.close()
sql(f"UPDATE player_progress SET level=51 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{imm_name}');")
s_imm = socket.create_connection((host, port), timeout=5)
recv_all(s_imm)
send_line(s_imm, imm_name); recv_all(s_imm)
send_line(s_imm, pw); recv_all(s_imm)
send_line(s_imm, "1"); recv_all(s_imm)
cmd(s_imm, "color off")

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Autoloot Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
check("Autoloot Sandbox" in cmd(s_imm, f"goto {ROOM}"), "goto lands in the sandbox room")

# --- 1: toggle autoloot on/off, listed in the toggle menu ---
out = cmd(s_imm, "toggle")
check("autoloot" in out, "autoloot appears in the toggle menu")
out = cmd(s_imm, "toggle autoloot")
check("autoloot is now" in out and "on" in out, "toggle autoloot turns it on")

# --- 2: autoloot ON -- gear moves straight into the winner's inventory ---
mort1 = f"Alootvica{_suffix}"
s_mort1 = make_char(mort1, pw)
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{mort1}';")
cmd(s_mort1, "quit!")
s_mort1.close()
s_mort1 = socket.create_connection((host, port), timeout=5)
recv_all(s_mort1)
send_line(s_mort1, mort1); recv_all(s_mort1)
send_line(s_mort1, pw); recv_all(s_mort1)
send_line(s_mort1, "1"); recv_all(s_mort1)
cmd(s_mort1, "color off")
check("Autoloot Sandbox" in cmd(s_mort1, "look"), "the first victim lands in the sandbox")

sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({ITEM1},'ring plain','a plain ring','A plain ring is lying here.',12,"
    f"{WEAR_TAKE},1);")
check("You conjure" in cmd(s_imm, f"load obj {ITEM1}"), "the first ring is loaded")
cmd(s_imm, "drop ring")
check("you get" in cmd(s_mort1, "get ring").lower(), "victim 1 picks up the ring")

out = cmd(s_imm, f"kill {mort1}")
check("instantly" in out.lower() or "slain" in out.lower() or "defeated" in out.lower(),
      "the immortal's kill resolves")
out2 = recv_all(s_imm, 1.0)
combined = out + out2
# Message format changed 2026-08-03 (combat.c): autoloot used to print
# one generic "You automatically loot X'''s corpse." line, now reports
# each item by name as "You loot <item> from <name>'''s corpse." --
# this test was never updated to match (found live, Session 190).
check("You loot" in combined and "corpse" in combined, "autoloot fires and tells the winner")
check("ring" in cmd(s_imm, "inventory").lower(), "the ring ended up in the winner's inventory")

# --- 3: autoloot OFF -- gear stays in the corpse instead ---
out = cmd(s_imm, "toggle autoloot")
check("autoloot is now" in out and "off" in out, "toggle autoloot turns it back off")

# Clear victim 1's (now-empty) corpse first -- otherwise `look corpse`
# below could match the wrong one (first-in-room-order).
cmd(s_imm, "purge")

mort2 = f"Alootvicb{_suffix}"
s_mort2 = make_char(mort2, pw)
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{mort2}';")
cmd(s_mort2, "quit!")
s_mort2.close()
s_mort2 = socket.create_connection((host, port), timeout=5)
recv_all(s_mort2)
send_line(s_mort2, mort2); recv_all(s_mort2)
send_line(s_mort2, pw); recv_all(s_mort2)
send_line(s_mort2, "1"); recv_all(s_mort2)
cmd(s_mort2, "color off")
check("Autoloot Sandbox" in cmd(s_mort2, "look"), "the second victim lands in the sandbox")

sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({ITEM2},'amulet plain','a plain amulet','A plain amulet is lying here.',12,"
    f"{WEAR_TAKE},1);")
check("You conjure" in cmd(s_imm, f"load obj {ITEM2}"), "the second amulet is loaded")
cmd(s_imm, "drop amulet")
check("you get" in cmd(s_mort2, "get amulet").lower(), "victim 2 picks up the amulet")

out = cmd(s_imm, f"kill {mort2}")
out2 = recv_all(s_imm, 1.0)
combined = out + out2
check("You loot" not in combined, "autoloot does not fire while toggled off")
check("amulet" not in cmd(s_imm, "inventory").lower(), "the amulet is NOT in the winner's inventory")
check("amulet" in cmd(s_imm, "look corpse").lower(), "the amulet is still sitting inside the corpse")

s_imm.close()
s_mort1.close()
s_mort2.close()
announce_done("smoke_test_autoloot", host, port)
print("=== ALL CHECKS PASSED ===")
