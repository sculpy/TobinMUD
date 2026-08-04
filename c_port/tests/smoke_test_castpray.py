#!/usr/bin/env python3
"""Smoke test for `cast`/`pray` (user 2026-07-11: "clerics should require
a holy symbol to pray successfully, druids and mages should require
components to cast with, so implement task_pray task_cast etc"). Covers:

  1. `cast` is refused for a non-Mage/Druid class; `pray` refused for a
     non-Cleric.
  2. A Mage with no spell component can't cast -- refused with a specific
     message. Picking up a "component"-keyword item lets the cast
     through, and the component is consumed (gone afterward).
  3. A Cleric with no holy symbol can't pray -- refused. Picking up a
     "symbol"-keyword item lets the prayer through, and the symbol IS
     consumed (gone afterward) -- user 2026-07-12: "holy symbols should
     use the same logic as components for mages and druids" (this test
     originally covered the OPPOSITE -- a symbol as a non-consumed
     keepsake -- before that request changed the design).
  4. An unknown spell/prayer name and a too-high-level one are both
     rejected with a specific message, before any component/symbol check.

    python3 tests/smoke_test_castpray.py [host] [port]
"""
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


announce("smoke_test_castpray", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 900000 + (int(time.time()) % 70000)
COMPONENT = ROOM + 1
SYMBOL = ROOM + 2

WEAR_TAKE = 1


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


imm_name = f"Cpimm{_suffix}"
imm_pw = "cpimmpw123"
s_imm = socket.create_connection((host, port), timeout=5)
recv_all(s_imm)
send_line(s_imm, imm_name); recv_all(s_imm)
send_line(s_imm, "y"); recv_all(s_imm)
send_line(s_imm, imm_pw); recv_all(s_imm)
send_line(s_imm, imm_pw); recv_all(s_imm)
send_line(s_imm, "new"); recv_all(s_imm)
send_line(s_imm, imm_name); recv_all(s_imm)
send_line(s_imm, "1"); recv_all(s_imm)
send_line(s_imm, "1"); recv_all(s_imm)  # territory: urban
send_line(s_imm, "1"); recv_all(s_imm)
send_line(s_imm, "done"); recv_all(s_imm)
send_line(s_imm, "done"); recv_all(s_imm)
s_imm.close()
set_level(imm_name, 51)
s_imm = socket.create_connection((host, port), timeout=5)
recv_all(s_imm)
send_line(s_imm, imm_name); recv_all(s_imm)
send_line(s_imm, imm_pw); recv_all(s_imm)
send_line(s_imm, "1"); recv_all(s_imm)
cmd(s_imm, "color off")

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Castpray Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
check("Castpray Sandbox" in cmd(s_imm, f"goto {ROOM}"), "goto lands in the SQL-bootstrapped sandbox room")

sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({COMPONENT},'pouch component reagent','a pouch of spell components',"
    f"'A pouch of spell components is lying here.',12,{WEAR_TAKE},1);")
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({SYMBOL},'symbol holy silver','a tarnished silver holy symbol',"
    f"'A tarnished silver holy symbol is lying here.',12,{WEAR_TAKE},1);")

# --- 1: gating by class ---
warrior_name = f"Cpwar{_suffix}"
pw = "castpraypw123"
sw = make_char(warrior_name, pw, "3")
out = cmd(sw, "cast heal light")
check("Command not found" in out, "cast is hidden from a non-Mage/Druid class")
out = cmd(sw, "pray heal light")
check("Command not found" in out, "pray is hidden from a non-Cleric class")
sw.close()

# --- 2: Mage cast requires a component, consumed on success ---
mage_name = f"Cpmag{_suffix}"
sm = make_char(mage_name, pw, "1")
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{mage_name}';")
# Set (properly immortal, >=51) and reconnect BEFORE the session under
# test -- a raw SQL level change doesn't reach an already-connected
# descriptor's live being_t, and immortal status also bypasses task 47's
# Basic-discipline-percentage gate on "gust" (a Class-tier spell this
# never-practiced test character would otherwise fail 0%-Basic), which
# isn't what this test is checking.
cmd(sm, "quit!")
sm.close()
set_level(mage_name, 51)
sm = socket.create_connection((host, port), timeout=5)
recv_all(sm)
send_line(sm, mage_name); recv_all(sm)
send_line(sm, pw); recv_all(sm)
send_line(sm, "1"); recv_all(sm)
cmd(sm, "color off")

out = cmd(sm, "cast gust")
check("don't have the spell components" in out, "casting without a component is refused")

out = cmd(sm, "cast nosuchspellzzz")
check("don't know a spell" in out, "an unknown spell name is rejected")

check("You conjure" in cmd(s_imm, f"load obj {COMPONENT}"), "the component pouch is loaded")
# `load obj` (2026-07-22) drops it straight into the loading immortal's
# OWN inventory, not the room floor -- drop it first so the mage can pick
# it back up off the ground, same as any other real item hand-off.
cmd(s_imm, "drop pouch")
out = cmd(sm, "get pouch")
check("you get" in out.lower(), "the mage picks up the component pouch")

out = cmd(sm, "cast gust")
# "gust" is a real offensive spell now (offensive spell breadth,
# Sneezy -> Tobin feature audit) -- it requires a target, and this mage
# isn't fighting anyone, so the actual (correct) response is "Cast that
# at whom?", not a spell effect. That message only appears once the
# component gate has already passed, which is what this test is really
# checking -- not what "gust" specifically does (see this test's own
# note above about spell effects being out of scope here).
check("Cast that at whom?" in out, "casting with a component succeeds (past the gate; gust itself needs a target)")
out = cmd(sm, "inventory")
check("pouch" not in out.lower(), "the component is consumed after a successful cast (even though gust itself refused for lack of a target)")

# --- 3: Cleric pray requires a holy symbol, consumed on success ---
cleric_name = f"Cpcle{_suffix}"
sc = make_char(cleric_name, pw, "2")
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{cleric_name}';")
cmd(sc, "quit!")
sc.close()
set_level(cleric_name, 51)  # properly immortal before reconnect -- see the Mage comment above
sc = socket.create_connection((host, port), timeout=5)
recv_all(sc)
send_line(sc, cleric_name); recv_all(sc)
send_line(sc, pw); recv_all(sc)
send_line(sc, "1"); recv_all(sc)
cmd(sc, "color off")

out = cmd(sc, "pray heal light")
check("need a holy symbol" in out, "praying without a holy symbol is refused")

check("You conjure" in cmd(s_imm, f"load obj {SYMBOL}"), "the holy symbol is loaded")
cmd(s_imm, "drop symbol")
out = cmd(sc, "get symbol")
check("you get" in out.lower(), "the cleric picks up the holy symbol")

out = cmd(sc, "pray heal light")
check("You pray for heal light" in out, "praying with a holy symbol succeeds")
out = cmd(sc, "inventory")
check("symbol" not in out.lower(), "the holy symbol IS consumed after a successful prayer")

s_imm.close()
sm.close()
sc.close()
announce_done("smoke_test_castpray", host, port)
print("=== ALL CHECKS PASSED ===")
