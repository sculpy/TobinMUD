#!/usr/bin/env python3
"""Smoke test for immortal class-restriction bypass on cast/pray/skills
(user 2026-07-12: "immortals can use any skill or spell in game, no
class restrictions"). Covers:

  1. An immortal (non-Mage/Druid class) can `cast` a Mage spell that a
     mortal of their own class would be refused ("Command not found").
  2. An immortal can `pray` a Cleric spell despite not being a Cleric.
  3. An immortal's `skills` output includes spells from OTHER classes
     (not just their own), proving the full-roster view is active.
  4. A same-class mortal is still gated normally (regression check --
     the bypass must not leak to non-immortals).

    python3 tests/smoke_test_immortal_castpray.py [host] [port]
"""
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


announce("smoke_test_immortal_castpray", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 900000 + (int(time.time()) % 70000)
COMPONENT = ROOM + 1
SYMBOL = ROOM + 2


def set_level(name, level):
    sql(f"UPDATE player_progress SET level={level} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def make_char(name, pw, class_choice):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "y"); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "new"); recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "1"); recv_all(s)  # race: human
    send_line(s, "1"); recv_all(s)  # territory: urban
    send_line(s, class_choice); recv_all(s)
    send_line(s, "done"); recv_all(s)
    send_line(s, "done"); recv_all(s)  # alignment: neutral
    cmd(s, "color off")
    return s


# --- Warrior mortal: still gated normally (regression check) ---
warrior_name = f"Imcpwar{_suffix}"
pw = "immortcastpw123"
sw = make_char(warrior_name, pw, "3")
out = cmd(sw, "cast heal light")
check("Command not found" in out, "a mortal Warrior is still refused cast (regression check)")
out = cmd(sw, "pray heal light")
check("Command not found" in out, "a mortal Warrior is still refused pray (regression check)")
sw.close()

# --- Immortal Warrior: bypasses class gate on cast/pray/skills ---
imm_name = f"Imcpimm{_suffix}"
s_imm = socket.create_connection((host, port), timeout=5)
recv_all(s_imm)
send_line(s_imm, imm_name); recv_all(s_imm)
send_line(s_imm, "y"); recv_all(s_imm)
send_line(s_imm, pw); recv_all(s_imm)
send_line(s_imm, pw); recv_all(s_imm)
send_line(s_imm, "new"); recv_all(s_imm)
send_line(s_imm, imm_name); recv_all(s_imm)
send_line(s_imm, "1"); recv_all(s_imm)  # race: human
send_line(s_imm, "1"); recv_all(s_imm)  # territory: urban
send_line(s_imm, "3"); recv_all(s_imm)  # class: warrior
send_line(s_imm, "done"); recv_all(s_imm)
send_line(s_imm, "done"); recv_all(s_imm)  # alignment: neutral
set_level(imm_name, 51)
s_imm.close()
s_imm = socket.create_connection((host, port), timeout=5)
recv_all(s_imm)
send_line(s_imm, imm_name); recv_all(s_imm)
send_line(s_imm, pw); recv_all(s_imm)
send_line(s_imm, "1"); recv_all(s_imm)
cmd(s_imm, "color off")

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Immortal Castpray Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
check("Immortal Castpray Sandbox" in cmd(s_imm, f"goto {ROOM}"), "goto lands in the sandbox room")

out = cmd(s_imm, "cast gust")
check("don't have the spell components" in out, "immortal Warrior reaches the Mage spell 'gust' (component-gated, not class-gated)")

sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({COMPONENT},'pouch component reagent','a pouch of spell components',"
    f"'A pouch of spell components is lying here.',12,1,1);")
check("You conjure" in cmd(s_imm, f"load obj {COMPONENT}"), "the component pouch is loaded")
# `load obj` (2026-07-22) drops it straight into the loading immortal's
# OWN inventory, not the room floor -- drop it first so `get` has
# something on the ground to actually pick back up, same as
# smoke_test_castpray.py's identical load/drop/get sequence.
cmd(s_imm, "drop pouch")
out = cmd(s_imm, "get pouch")
check("you get" in out.lower(), "the immortal Warrior picks up the component pouch")
# "gust" is a real offensive spell now (offensive spell breadth,
# Sneezy -> Tobin feature audit) -- it requires a target, and s_imm isn't
# fighting anyone, so the actual (correct) response is "Cast that at
# whom?", which still only appears once the class gate has been bypassed
# (what this test is actually checking), not once gust deals damage. A
# generous 6s timeout (default is 1.0s) -- this response is now printed
# by cmd_cast_resolve_effect() only after the multi-round cast delay
# (spellcast.c, 2026-08-09) finishes resolving, 2-3 rounds x ~1.2s
# apart, not synchronously at the moment `cast gust` is typed.
out = cmd(s_imm, "cast gust", 6.0)
check("Cast that at whom?" in out, "immortal Warrior casts the Mage spell 'gust' despite being a Warrior (gust itself needs a target)")

out = cmd(s_imm, "pray heal light")
check("need a holy symbol" in out, "immortal Warrior still needs a holy symbol item to pray (item gate, not a class gate)")

sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({SYMBOL},'symbol holy silver','a tarnished silver holy symbol',"
    f"'A tarnished silver holy symbol is lying here.',12,1,1);")
check("You conjure" in cmd(s_imm, f"load obj {SYMBOL}"), "the holy symbol is loaded")
cmd(s_imm, "drop symbol")  # same load-drops-to-inventory reasoning as the pouch above
out = cmd(s_imm, "get symbol")
check("you get" in out.lower(), "the immortal Warrior picks up the holy symbol")
out = cmd(s_imm, "pray heal light")
check("You pray for heal light" in out, "immortal Warrior prays the Cleric spell 'heal light' despite being a Warrior")

out = cmd(s_imm, "skills")
# Immortal `skills` shows all six classes' full rosters -- long enough
# to hit the real pager (20 lines/page, descriptor.c), so page through
# ("ENTER for more") until the last class section (Warrior, per
# CLASS_COUNT's enum order in being.h) has arrived, or a safety cap.
pages = 0
while "ENTER" in out and "=== Warrior ===" not in out and pages < 30:
    out += cmd(s_imm, "", 0.5)
    pages += 1
check("=== Mage ===" in out, "immortal 'skills' shows the Mage section")
check("=== Cleric ===" in out, "immortal 'skills' shows the Cleric section")
check("=== Warrior ===" in out, "immortal 'skills' shows their own Warrior section too")
check("gust" in out.lower(), "immortal 'skills' lists a Mage-only spell")

s_imm.close()
announce_done("smoke_test_immortal_castpray", host, port)
print("=== ALL CHECKS PASSED ===")
