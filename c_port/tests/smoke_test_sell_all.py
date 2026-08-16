#!/usr/bin/env python3
"""Smoke test for `sell all`/`sell all.<target>` (TODO.md, user
2026-08-04: "in the shops sell all.commodity or all.compobnent should
work") -- cmd_shop.c's cmd_sell() previously only handled one item (with
ordinal support, "sell 2.sword"); sell_all_from_inventory() now handles
the bulk forms, same `all`/`all.<name>` convention `get`/`remove` already
use. Skips (doesn't refuse outright) any carried item the shop's
`shoptype` rows don't cover, so a mixed pile sells what it can.

Uses the same real seeded shop as smoke_test_shop_resell.py (shop_nr 0,
"The Glinting Dagger", room 559, weapon category).

Covers:
  1. `sell all.<name>` sells only the matching item(s), leaving an
     unrelated carried item untouched.
  2. `sell all` sells every remaining sellable loose carried item and
     reports a total.

    python3 tests/smoke_test_sell_all.py [host] [port]
"""
import re
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done
from mud_creation import create_character

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))

SHOP_ROOM = 559
DAGGER_VNUM = 958000 + (int(time.time()) % 900)
SWORD_VNUM = 957000 + (int(time.time()) % 900)
BAG_VNUM = 956000 + (int(time.time()) % 900)
TYPE_BAG = 27  # ITEM_BAG -> OBJ_CAT_CONTAINER
TYPE_WEAPON = 5
WEAR_TAKE = 1
WEAR_HOLD = 16384


def make_char(name, pw, class_num):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw):
        send_line(s, step)
        recv_all(s)
    create_character(s, name, send_line, recv_all, char_class=str(class_num))
    return s


def relog(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, pw, "1"):
        send_line(s, step)
        recv_all(s)
    cmd(s, "color off")
    return s


announce("smoke_test_sell_all", host, port)

name, pw = f"Slall{_suffix}", "sellalltestpw12"

sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,price,material,can_be_seen) "
    f"VALUES ({DAGGER_VNUM},'sellall dagger','a sellall test dagger','A sellall test dagger lies here.',"
    f"{TYPE_WEAPON},{WEAR_TAKE | WEAR_HOLD},500,0,1);")
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,price,material,can_be_seen) "
    f"VALUES ({SWORD_VNUM},'sellall sword','a sellall test sword','A sellall test sword lies here.',"
    f"{TYPE_WEAPON},{WEAR_TAKE | WEAR_HOLD},700,0,1);")
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,price,material,can_be_seen) "
    f"VALUES ({BAG_VNUM},'sellall bag','a sellall test bag','A sellall test bag lies here.',"
    f"{TYPE_BAG},{WEAR_TAKE},10,0,1);")

s = make_char(name, pw, 3)  # Warrior (level 51+ needed for `load`)
cmd(s, "quit!"); s.close()
sql(f"UPDATE player_progress SET level=59 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{name}');")
sql(f"UPDATE player SET load_room={SHOP_ROOM} WHERE name='{name}';")
s = relog(name, pw)
check("Glinting Dagger" in cmd(s, "look"), "lands in the real shop room")

check("You conjure" in cmd(s, f"load obj {DAGGER_VNUM}"), "the test dagger is loaded")
check("You conjure" in cmd(s, f"load obj {SWORD_VNUM}"), "the test sword is loaded")

# --- 1: sell all.<name> sells only the matching item ---
out = cmd(s, "sell all.dagger")
check("sellall test dagger" in out.lower(), "the dagger-only sale reports the dagger")
check("sellall test sword" not in out.lower(), "the dagger-only sale leaves the sword untouched")
check("total of" in out.lower(), "a total-paid summary line is shown")

inv = cmd(s, "inventory")
check("sellall test sword" in inv.lower(), "the sword is still in inventory after selling only the dagger")
check("sellall test dagger" not in inv.lower(), "the dagger is gone from inventory after the sale")

# --- 2: sell all sells everything remaining sellable ---
out = cmd(s, "sell all")
check("sellall test sword" in out.lower(), "the bare `sell all` sells the remaining sword")

inv2 = cmd(s, "inventory")
check("sellall test sword" not in inv2.lower(), "the sword is gone from inventory after `sell all`")

# --- 3: sell all.<name> reaches into a carried container ---
check("You conjure" in cmd(s, f"load obj {BAG_VNUM}"), "the test bag is loaded")
check("You conjure" in cmd(s, f"load obj {DAGGER_VNUM}"), "a fresh dagger is loaded for the bag case")
cmd(s, "put dagger bag")
binv = cmd(s, "inventory")
check("sellall test dagger" not in binv.lower(), "the dagger is inside the bag, not loose in inventory")
out3 = cmd(s, "sell all.dagger")
check("sellall test dagger" in out3.lower(), "sell all.<name> pulls the dagger out of the bag and sells it")
lookin = cmd(s, "look in bag")
check("sellall test dagger" not in lookin.lower(), "the bag is empty after the container sale")

s.close()

sql(f"DELETE FROM player_inventory WHERE player_id=(SELECT id FROM player WHERE name='{name}');")
sql(f"DELETE FROM player_progress WHERE player_id=(SELECT id FROM player WHERE name='{name}');")
sql(f"DELETE FROM player_attrs WHERE player_id=(SELECT id FROM player WHERE name='{name}');")
sql(f"DELETE FROM player WHERE name='{name}';")
sql(f"DELETE FROM obj WHERE vnum IN ({DAGGER_VNUM}, {SWORD_VNUM}, {BAG_VNUM});")

announce_done("smoke_test_sell_all", host, port)
print("=== ALL CHECKS PASSED ===")
