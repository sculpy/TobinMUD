#!/usr/bin/env python3
"""Smoke test for container-content stacking (user, 2026-08-08: "in
containers objs should stack and components merge. see sneezy for
inspiration").

`look <container>`'s "It contains:" listing and the immortal-only
carried-inventory view's one-level-nested container listing (both
cmd_look.c) used to print one line per item with no grouping at all --
three identical potions in a bag showed as three identical lines instead
of the "(xN)" stacking `inventory` (cmd_object.c) and the room floor
(cmd_look.c's group_room_items()) already had. New shared
render_grouped_contents() (cmd_look.c) closes that gap using the exact
same "identical rendered line -> one entry, count it" technique those
two already use -- covers "components merge" too, since ephemeral
same-label items (e.g. Planting's fruit/hide/meat) group the same way
regular prototype items do, no separate vnum-equality check needed.

Covers:
  1. Three identical items dropped into a container show as one grouped
     "(x3)" line via `look <container>`.
  2. A single, non-duplicated item in the same container still shows on
     its own line, no stray "(x1)".
  3. An empty container still shows "Nothing." (the grouping helper
     returning zero groups doesn't break the existing empty-container
     message).

    python3 tests/smoke_test_container_stack.py [host] [port]
"""
import re
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


def strip(s):
    return re.sub(r"\x1b\[[0-9;]*m", "", s)


announce("smoke_test_container_stack", host, port)

imm_name, imm_pw = "Deploybot", "deploybotpw12"
si = socket.create_connection((host, port), timeout=5)
recv_all(si)
send_line(si, imm_name); recv_all(si)
send_line(si, imm_pw); recv_all(si)
send_line(si, "1"); recv_all(si)

ROOM = 963000 + (int(time.time()) % 20000)
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Container Stack Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
cmd(si, f"goto {ROOM}")

TYPE_CONTAINER = 15
TYPE_OTHER = 20
WEAR_TAKE = 1
WEAR_HOLD = 16384

BAG_VNUM = ROOM + 1
POT_VNUM = ROOM + 2
COIN_VNUM = ROOM + 3
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,val0,val1,weight,can_be_seen) "
    f"VALUES ({BAG_VNUM},'stacksandbox bag','a leather bag','A leather bag is lying here.',"
    f"{TYPE_CONTAINER},{WEAR_TAKE | WEAR_HOLD},100,0,2,1);")
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,weight,can_be_seen) "
    f"VALUES ({POT_VNUM},'stacksandbox potion','a red potion','A red potion is lying here.',"
    f"{TYPE_OTHER},{WEAR_TAKE},1,1);")
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,weight,can_be_seen) "
    f"VALUES ({COIN_VNUM},'stacksandbox coin','a gold coin','A gold coin is lying here.',"
    f"{TYPE_OTHER},{WEAR_TAKE},0,1);")

cmd(si, f"load obj {BAG_VNUM}")

# --- 3: empty container still says Nothing ---
out = strip(cmd(si, "look bag", timeout=1.5))
check("Nothing." in out, "an empty container still shows 'Nothing.'")

for _ in range(3):
    cmd(si, f"load obj {POT_VNUM}")
    cmd(si, "put potion bag")
cmd(si, f"load obj {COIN_VNUM}")
cmd(si, "put coin bag")

# --- 1/2: three potions group, the lone coin doesn't ---
out = strip(cmd(si, "look bag", timeout=1.5))
check("red potion (x3)" in out, "three identical items in a container show as one grouped '(x3)' line")
check("gold coin" in out and "gold coin (x1)" not in out,
      "a single, non-duplicated item still shows on its own line, no stray '(x1)'")

si.close()

announce_done("smoke_test_container_stack", host, port)
print("=== ALL CHECKS PASSED ===")
