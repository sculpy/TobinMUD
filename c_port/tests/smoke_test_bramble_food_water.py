#!/usr/bin/env python3
"""Smoke test for `bramble drain` (Druid, level 3), and `create food`/
`create water` (Cleric level 3 + Druid level 9) stub-audit fixes.

    python3 tests/smoke_test_bramble_food_water.py [host] [port]
"""
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done
from mud_creation import create_character

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 942200 + (int(time.time()) % 30000)
SYMBOL = ROOM + 1
COMPONENT = ROOM + 2
JUG = ROOM + 3

WEAR_TAKE = 1


def set_level(name, level):
    sql(f"UPDATE player_progress SET level={level} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def make_char(name, pw, class_choice):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw):
        send_line(s, step)
        recv_all(s)
    create_character(s, name, send_line, recv_all, char_class=class_choice)
    return s


def relog(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, pw, "1"):
        send_line(s, step)
        recv_all(s)
    cmd(s, "color off")
    return s


announce("smoke_test_bramble_food_water", host, port)

imm_name, imm_pw = f"Sbfimm{_suffix}", "sbfimmpw123"
s = make_char(imm_name, imm_pw, "1")
cmd(s, "quit!")
s.close()
sql(f"UPDATE player_progress SET level=51 WHERE player_id=(SELECT id FROM player WHERE name='{imm_name}');")
s = relog(imm_name, imm_pw)

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'BrambleFW Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,val0,val1,can_be_seen) "
    f"VALUES ({SYMBOL},'symbol holy silver','a tarnished silver holy symbol',"
    f"'A tarnished silver holy symbol is lying here.',12,{WEAR_TAKE},5,5,1);")
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({COMPONENT},'pouch component reagent','a pouch of spell components',"
    f"'A pouch of spell components is lying here.',12,{WEAR_TAKE},1);")
# raw item type 17 = ITEM_DRINKCON -> OBJ_CAT_DRINK (obj.c's
# ITEM_TYPE_CATEGORY table); val0=capacity val1=current val2=liquid type.
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,val0,val1,val2,can_be_seen) "
    f"VALUES ({JUG},'jug clay empty','an empty clay jug',"
    f"'An empty clay jug is lying here.',17,{WEAR_TAKE},10,0,0,1);")
check("BrambleFW" in cmd(s, f"goto {ROOM}"), "the immortal goto's into the sandbox room")

# --- bramble drain (Druid, level 3): real damage + heal-back ---
druid_name, druid_pw = f"Sbfdru{_suffix}", "sbfdrupw123"
sd = make_char(druid_name, druid_pw, "3")
cmd(sd, "quit!")
sd.close()
set_level(druid_name, 51)
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{druid_name}';")
sd = relog(druid_name, druid_pw)
check("BrambleFW" in cmd(sd, "look"), "the druid lands in the sandbox room")

victim_name, victim_pw = f"Sbfvic{_suffix}", "sbfvicpw123"
sv = make_char(victim_name, victim_pw, "1")
cmd(sv, "quit!")
sv.close()
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{victim_name}';")
sv = relog(victim_name, victim_pw)
check("BrambleFW" in cmd(sv, "look"), "the victim lands in the sandbox room")

check("You conjure" in cmd(s, f"load obj {COMPONENT}"), "component 1 loaded")
cmd(s, "drop pouch")
check("you get" in cmd(sd, "get pouch").lower(), "the druid picks up component 1")

hp_before = cmd(sd, "score")
out = cmd(sd, f"cast bramble drain {victim_name}")
check("life flow into you" in out.lower(), "bramble drain lands a real drain, not a no-op")
check("nothing happens" not in out.lower(), "bramble drain doesn't fall through to the no-op placeholder")

# --- create food (Druid, level 9): real conjured, eatable item appears ---
set_level(druid_name, 51)
check("You conjure" in cmd(s, f"load obj {COMPONENT}"), "component 2 loaded")
cmd(s, "drop pouch")
check("you get" in cmd(sd, "get pouch").lower(), "the druid picks up component 2")
fout = cmd(sd, "cast create food")
check("appears in your hands" in fout.lower(), "create food conjures a real item")
inv = cmd(sd, "inventory").lower()
check("bread" in inv, "the conjured bread loaf is really in the druid's inventory")
eatout = cmd(sd, "eat bread").lower()
check("you eat" in eatout or "you finish" in eatout or "you devour" in eatout, "the conjured bread is genuinely eatable")

# --- create water (Druid, level 9): fills a carried empty jug ---
check("You conjure" in cmd(s, f"load obj {COMPONENT}"), "component 3 loaded")
cmd(s, "drop pouch")
check("you get" in cmd(sd, "get pouch").lower(), "the druid picks up component 3")
check("You conjure" in cmd(s, f"load obj {JUG}"), "jug loaded")
cmd(s, "drop jug")
check("you get" in cmd(sd, "get jug").lower(), "the druid picks up the empty jug")
wout = cmd(sd, "cast create water jug")
check("fills to the brim" in wout.lower(), "create water fills the jug for real")
sipout = cmd(sd, "sip jug").lower()
check("thirst" in sipout or "you sip" in sipout or "water" in sipout, "the conjured water is genuinely drinkable")

# --- create food / create water (Cleric, level 3) ---
cleric_name, cleric_pw = f"Sbfcle{_suffix}", "sbfclepw123"
sc = make_char(cleric_name, cleric_pw, "2")
cmd(sc, "quit!")
sc.close()
set_level(cleric_name, 51)
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{cleric_name}';")
sc = relog(cleric_name, cleric_pw)
check("BrambleFW" in cmd(sc, "look"), "the cleric lands in the sandbox room")

check("You conjure" in cmd(s, f"load obj {SYMBOL}"), "the holy symbol is loaded")
cmd(s, "drop symbol")
check("you get" in cmd(sc, "get symbol").lower(), "the cleric picks up the holy symbol")

cfout = cmd(sc, "pray create food")
check("appears in your hands" in cfout.lower(), "cleric create food conjures a real item")
cinv = cmd(sc, "inventory").lower()
check("bread" in cinv, "the cleric's conjured bread is really in their inventory")

check("You conjure" in cmd(s, f"load obj {JUG}"), "jug 2 loaded")
cmd(s, "drop jug")
check("you get" in cmd(sc, "get jug").lower(), "the cleric picks up an empty jug")
cwout = cmd(sc, "pray create water jug")
check("fills to the brim" in cwout.lower(), "cleric create water fills the jug for real")

s.close(); sd.close(); sv.close(); sc.close()

sql(f"DELETE FROM room WHERE vnum={ROOM};")
sql(f"DELETE FROM obj WHERE vnum IN ({SYMBOL}, {COMPONENT}, {JUG});")

announce_done("smoke_test_bramble_food_water", host, port)
print("=== ALL CHECKS PASSED ===")
