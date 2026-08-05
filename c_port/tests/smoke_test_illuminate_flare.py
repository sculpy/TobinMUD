#!/usr/bin/env python3
"""Smoke test for `illuminate` (Mage, level 2) and `flare` (Mage, level 3)
stub-audit fixes.

    python3 tests/smoke_test_illuminate_flare.py [host] [port]
"""
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done
from mud_creation import create_character

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 942100 + (int(time.time()) % 30000)
LAMP = ROOM + 1
COMPONENT = ROOM + 2

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


announce("smoke_test_illuminate_flare", host, port)

imm_name, imm_pw = f"Sifimm{_suffix}", "sifimmpw123"
s = make_char(imm_name, imm_pw, "1")
cmd(s, "quit!")
s.close()
sql(f"UPDATE player_progress SET level=51 WHERE player_id=(SELECT id FROM player WHERE name='{imm_name}');")
s = relog(imm_name, imm_pw)

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'IllumFlare Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
# raw item type 1 = ITEM_LIGHT (obj.c's ITEM_TYPE_NAMES), maps to OBJ_CAT_LIGHT
# val0=radius val1=maxburn val2=curburn val3=lit
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,val0,val1,val2,val3,can_be_seen) "
    f"VALUES ({LAMP},'lamp brass','a brass lamp',"
    f"'A brass lamp is lying here.',1,{WEAR_TAKE},3,50,0,0,1);")
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({COMPONENT},'pouch component reagent','a pouch of spell components',"
    f"'A pouch of spell components is lying here.',12,{WEAR_TAKE},1);")
check("IllumFlare" in cmd(s, f"goto {ROOM}"), "the immortal goto's into the sandbox room")

mage_name, mage_pw = f"Sifmag{_suffix}", "sifmagpw123"
sm = make_char(mage_name, mage_pw, "1")
cmd(sm, "quit!")
sm.close()
set_level(mage_name, 51)
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{mage_name}';")
sm = relog(mage_name, mage_pw)
check("IllumFlare" in cmd(sm, "look"), "the mage lands in the sandbox room")

check("You conjure" in cmd(s, f"load obj {COMPONENT}"), "component 1 loaded")
cmd(s, "drop pouch")
check("you get" in cmd(sm, "get pouch").lower(), "the mage picks up component 1")
check("You conjure" in cmd(s, f"load obj {LAMP}"), "lamp loaded")
cmd(s, "drop lamp")
check("you get" in cmd(sm, "get lamp").lower(), "the mage picks up the unlit lamp")

# --- illuminate: real object-target, lights an unlit lamp with no fuel cost ---
out = cmd(sm, "cast illuminate lamp")
check("bursts into flame" in out.lower(), "illuminate lights the lamp for real")
check("nothing happens" not in out.lower(), "illuminate doesn't fall through to the no-op placeholder")
check("You conjure" in cmd(s, f"load obj {COMPONENT}"), "component 1b loaded")
cmd(s, "drop pouch")
check("you get" in cmd(sm, "get pouch").lower(), "the mage picks up component 1b")
out2 = cmd(sm, "cast illuminate lamp")
check("already lit" in out2.lower(), "illuminate on an already-lit lamp reports cleanly, not a double-light")

# --- flare: room-wide, non-immortal occupants gain infravision ---
victim_name, victim_pw = f"Sifvic{_suffix}", "sifvicpw123"
sv = make_char(victim_name, victim_pw, "3")
cmd(sv, "quit!")
sv.close()
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{victim_name}';")
sv = relog(victim_name, victim_pw)
check("IllumFlare" in cmd(sv, "look"), "the victim lands in the sandbox room")

check("You conjure" in cmd(s, f"load obj {COMPONENT}"), "component 2 loaded")
cmd(s, "drop pouch")
check("you get" in cmd(sm, "get pouch").lower(), "the mage picks up component 2")

fout = cmd(sm, "cast flare")
check("warm magical glow" in fout.lower(), "flare fills the room with light")
check("nothing happens" not in fout.lower(), "flare doesn't fall through to the no-op placeholder")
vaff = cmd(sv, "affects").lower()
check("infravision" in vaff, "flare gives the OTHER occupant (not just the caster) infravision")

s.close(); sm.close(); sv.close()

sql(f"DELETE FROM room WHERE vnum={ROOM};")
sql(f"DELETE FROM obj WHERE vnum IN ({LAMP}, {COMPONENT});")

announce_done("smoke_test_illuminate_flare", host, port)
print("=== ALL CHECKS PASSED ===")
