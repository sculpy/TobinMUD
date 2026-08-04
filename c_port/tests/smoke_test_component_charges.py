#!/usr/bin/env python3
"""Smoke test for spell component charges / holy symbol decay (user
2026-07-18: "how long does each component last? should be getting 10
casts out of each component and the symbols should decay as in sneezy").
Covers:

  1. A fresh component (val[0]/val[1] seeded 10/10 by
     tobin_migrations.sql) survives exactly 10 `cast` attempts before
     being destroyed with a "used up" message; an 11th attempt correctly
     reports missing components again.
  2. A fresh holy symbol survives more than one `pray` attempt (real
     decay, 1-2 strength lost per use out of 10, not a single-use item
     like the pre-2026-07-18 behavior) and eventually shatters with a
     "shatters from the stress of the prayer" message.

    python3 tests/smoke_test_component_charges.py [host] [port]
"""
import re
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


announce("smoke_test_component_charges", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 900000 + (int(time.time() * 1000) % 60000)
COMPONENT = ROOM + 1
SYMBOL = ROOM + 2


def set_level(name, level):
    sql(f"UPDATE player_progress SET level={level} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


imm_name = f"Chgimm{_suffix}"
imm_pw = "chgimmpw123"
s = socket.create_connection((host, port), timeout=5)
recv_all(s)
send_line(s, imm_name); recv_all(s)
send_line(s, "y"); recv_all(s)
send_line(s, imm_pw); recv_all(s)
send_line(s, imm_pw); recv_all(s)
send_line(s, "new"); recv_all(s)
send_line(s, imm_name); recv_all(s)
send_line(s, "1"); recv_all(s)  # race: human
send_line(s, "1"); recv_all(s)  # territory: urban
send_line(s, "1"); recv_all(s)  # class: mage
send_line(s, "done"); recv_all(s)
send_line(s, "done"); recv_all(s)  # alignment: neutral
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
    f"VALUES ({ROOM},0,0,0,'Charges Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
check("Charges Sandbox" in cmd(s, f"goto {ROOM}"), "goto lands in the sandbox room")

# --- 1: a component lasts exactly 10 attempts, then is destroyed ---
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen,val0,val1) "
    f"VALUES ({COMPONENT},'pouch component reagent','a pouch of spell components',"
    f"'A pouch of spell components is lying here.',12,1,1,10,10);")
check("You conjure" in cmd(s, f"load obj {COMPONENT}"), "the component is loaded")
out = cmd(s, "get pouch")
check("you get" in out.lower(), "the component is picked up")

used_up_on = None
for i in range(1, 12):
    out = cmd(s, "cast gust")
    if "is used up" in out:
        used_up_on = i
        break
check(used_up_on == 10, f"the component survives exactly 10 casts, not {used_up_on}")

out = cmd(s, "cast gust")
check("don't have the spell components" in out, "an 11th cast correctly finds no component left")

# --- 2: a holy symbol decays (variable strength loss) and eventually
#     shatters, surviving MORE than a single use unlike the old
#     single-use-per-symbol behavior ---
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen,val0,val1) "
    f"VALUES ({SYMBOL},'symbol holy silver','a tarnished silver holy symbol',"
    f"'A tarnished silver holy symbol is lying here.',12,1,1,10,10);")
check("You conjure" in cmd(s, f"load obj {SYMBOL}"), "the holy symbol is loaded")
out = cmd(s, "get symbol")
check("you get" in out.lower(), "the holy symbol is picked up")

shattered_on = None
for i in range(1, 16):
    out = cmd(s, "pray armor")
    if "shatters from the stress" in out:
        shattered_on = i
        break
check(shattered_on is not None, "the symbol eventually shatters")
check(shattered_on > 1, f"the symbol survives more than one prayer before shattering (shattered on #{shattered_on})")
check(shattered_on <= 10, f"the symbol doesn't survive an implausible number of prayers ({shattered_on})")

out = cmd(s, "pray armor")
check("need a holy symbol" in out, "praying again correctly finds no symbol left")

s.close()
announce_done("smoke_test_component_charges", host, port)
print("=== ALL CHECKS PASSED ===")
