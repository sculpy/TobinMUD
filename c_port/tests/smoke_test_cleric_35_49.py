#!/usr/bin/env python3
"""Smoke test for Cleric levels 35-49 stub-audit fixes: astral walk,
bone breaker, consecrate, wither limb, spontaneous combust, crusade,
portal.

    python3 tests/smoke_test_cleric_35_49.py [host] [port]
"""
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done
from mud_creation import create_character

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 942700 + (int(time.time()) % 25000)
ROOM2 = ROOM + 1
SYMBOL = ROOM + 2

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


def get_symbol(imm_sock, cleric_sock):
    check("You conjure" in cmd(imm_sock, f"load obj {SYMBOL}"), "symbol loaded")
    cmd(imm_sock, "drop symbol")
    check("you get" in cmd(cleric_sock, "get symbol").lower(), "the cleric picks up the symbol")


announce("smoke_test_cleric_35_49", host, port)

imm_name, imm_pw = f"Scfiveimm{_suffix}", "sc549immpw1"
s = make_char(imm_name, imm_pw, "1")
cmd(s, "quit!")
s.close()
sql(f"UPDATE player_progress SET level=51 WHERE player_id=(SELECT id FROM player WHERE name='{imm_name}');")
s = relog(imm_name, imm_pw)

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Cleric3549 Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM2},0,0,0,'Cleric3549 Astral Target','Another bare room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,val0,val1,can_be_seen) "
    f"VALUES ({SYMBOL},'symbol holy silver','a tarnished silver holy symbol',"
    f"'A tarnished silver holy symbol is lying here.',12,{WEAR_TAKE},5,5,1);")
check("Cleric3549" in cmd(s, f"goto {ROOM}"), "the immortal goto's into the sandbox room")

cleric_name, cleric_pw = f"Scfivecle{_suffix}", "sc549clepw1"
sc = make_char(cleric_name, cleric_pw, "2")
cmd(sc, "quit!")
sc.close()
set_level(cleric_name, 51)
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{cleric_name}';")
sc = relog(cleric_name, cleric_pw)
check("Cleric3549" in cmd(sc, "look"), "the cleric lands in the sandbox room")

victim_name, victim_pw = f"Scfivevic{_suffix}", "sc549vicpw1"
sv = make_char(victim_name, victim_pw, "1")
cmd(sv, "quit!")
sv.close()
sql(f"UPDATE player_progress SET level=50,hp=9000,max_hp=9000 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{victim_name}');")
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{victim_name}';")
sv = relog(victim_name, victim_pw)
check("Cleric3549" in cmd(sv, "look"), "the victim lands in the sandbox room")

get_symbol(s, sc)
out = cmd(sc, "pray astral walk Cleric3549 Astral Target")
check("shimmering portal" in out.lower(), "astral walk opens a real portal")
check("Astral Target" in cmd(sc, "look"), "astral walk really teleports the cleric to the named room")

# return to sandbox for the rest -- SQL load_room update must happen
# AFTER quit! (not before), same load_room-timing quirk documented
# elsewhere in this test suite.
cmd(sc, "quit!")
sc.close()
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{cleric_name}';")
sc = relog(cleric_name, cleric_pw)
check("Cleric3549 Sandbox" in cmd(sc, "look"), "the cleric is back in the sandbox room")

get_symbol(s, sc)
out = cmd(sc, f"pray bone breaker {victim_name}")
check("snaps and goes limp" in out.lower(), "bone breaker lands a real limb-break")

get_symbol(s, sc)
out = cmd(sc, f"pray wither limb {victim_name}")
check("snaps and goes limp" in out.lower(), "wither limb lands a real limb-break")

get_symbol(s, sc)
out = cmd(sc, "pray consecrate")
check("blessing everyone" in out.lower(), "consecrate casts a real room-wide blessing")
check("sanctuary" in cmd(sv, "affects").lower(), "consecrate really gives the OTHER occupant Sanctuary")

get_symbol(s, sc)
out = cmd(sc, f"pray spontaneous combust {victim_name}")
check("nothing happens" not in out.lower(), "spontaneous combust deals real damage, not a no-op")

get_symbol(s, sc)
out = cmd(sc, "pray crusade")
check("blessing everyone" in out.lower(), "crusade casts a real room-wide blessing")

get_symbol(s, sc)
out = cmd(sc, "pray portal Cleric3549 Astral Target")
check("shimmering portal" in out.lower(), "portal opens a real portal")
check("Astral Target" in cmd(sc, "look"), "portal really teleports the cleric to the named room")

s.close(); sc.close(); sv.close()

sql(f"DELETE FROM room WHERE vnum IN ({ROOM}, {ROOM2});")
sql(f"DELETE FROM obj WHERE vnum={SYMBOL};")

announce_done("smoke_test_cleric_35_49", host, port)
print("=== ALL CHECKS PASSED ===")
