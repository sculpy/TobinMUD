#!/usr/bin/env python3
"""Smoke test for the 3 missing-spell audit ports: protection from
earth (Mage), inferno (Mage), sterilize (Cleric).

    python3 tests/smoke_test_missing_spells.py [host] [port]
"""
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done
from mud_creation import create_character

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 942800 + (int(time.time()) % 25000)
SYMBOL = ROOM + 1
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


announce("smoke_test_missing_spells", host, port)

imm_name, imm_pw = f"Smsimm{_suffix}", "smsimmpw123"
s = make_char(imm_name, imm_pw, "1")
cmd(s, "quit!")
s.close()
sql(f"UPDATE player_progress SET level=51 WHERE player_id=(SELECT id FROM player WHERE name='{imm_name}');")
s = relog(imm_name, imm_pw)

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'MissingSpell Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,val0,val1,can_be_seen) "
    f"VALUES ({SYMBOL},'symbol holy silver','a tarnished silver holy symbol',"
    f"'A tarnished silver holy symbol is lying here.',12,{WEAR_TAKE},5,5,1);")
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({COMPONENT},'pouch component reagent','a pouch of spell components',"
    f"'A pouch of spell components is lying here.',12,{WEAR_TAKE},1);")
check("MissingSpell" in cmd(s, f"goto {ROOM}"), "the immortal goto's into the sandbox room")

mage_name, mage_pw = f"Smsmag{_suffix}", "smsmagpw123"
sm = make_char(mage_name, mage_pw, "1")
cmd(sm, "quit!")
sm.close()
set_level(mage_name, 51)
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{mage_name}';")
sm = relog(mage_name, mage_pw)
check("MissingSpell" in cmd(sm, "look"), "the mage lands in the sandbox room")

victim_name, victim_pw = f"Smsvic{_suffix}", "smsvicpw123"
sv = make_char(victim_name, victim_pw, "1")
cmd(sv, "quit!")
sv.close()
sql(f"UPDATE player_progress SET level=50,hp=9000,max_hp=9000 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{victim_name}');")
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{victim_name}';")
sv = relog(victim_name, victim_pw)
check("MissingSpell" in cmd(sv, "look"), "the victim lands in the sandbox room")

# --- protection from earth (Mage, level 8): real room-wide buff ---
check("You conjure" in cmd(s, f"load obj {COMPONENT}"), "component 1 loaded")
cmd(s, "drop pouch")
check("you get" in cmd(sm, "get pouch").lower(), "the mage picks up component 1")
out = cmd(sm, "cast protection from earth")
check("warding shimmer" in out.lower(), "protection from earth casts for real, not a no-op")
check("sanctuary" in cmd(sv, "affects").lower(), "protection from earth really gives the OTHER occupant Sanctuary")

# --- inferno (Mage, level 21): real damage ---
check("You conjure" in cmd(s, f"load obj {COMPONENT}"), "component 2 loaded")
cmd(s, "drop pouch")
check("you get" in cmd(sm, "get pouch").lower(), "the mage picks up component 2")
out = cmd(sm, f"cast inferno {victim_name}")
check("scorching" in out.lower(), "inferno lands a real strike, not a no-op")

# --- sterilize (Cleric, level 6): real targeted infection cure ---
cleric_name, cleric_pw = f"Smscle{_suffix}", "smsclepw123"
sc = make_char(cleric_name, cleric_pw, "2")
cmd(sc, "quit!")
sc.close()
set_level(cleric_name, 51)
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{cleric_name}';")
sc = relog(cleric_name, cleric_pw)
check("MissingSpell" in cmd(sc, "look"), "the cleric lands in the sandbox room")

check("You conjure" in cmd(s, f"load obj {SYMBOL}"), "the holy symbol is loaded")
cmd(s, "drop symbol")
check("you get" in cmd(sc, "get symbol").lower(), "the cleric picks up the holy symbol")
out = cmd(sc, "pray sterilize")
check("no infection to cleanse" in out.lower(), "sterilize with no infection reports cleanly, not a crash/no-op")

s.close(); sm.close(); sv.close(); sc.close()

sql(f"DELETE FROM room WHERE vnum={ROOM};")
sql(f"DELETE FROM obj WHERE vnum IN ({SYMBOL}, {COMPONENT});")

announce_done("smoke_test_missing_spells", host, port)
print("=== ALL CHECKS PASSED ===")
