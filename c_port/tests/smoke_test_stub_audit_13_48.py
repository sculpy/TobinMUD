#!/usr/bin/env python3
"""Smoke test for the level 13-48 stub-audit batch:
Cleric: flamestrike, expel, numb, second wind, paralyze (full), earthquake
Mage: Garmul's tail, shatter, watery grave, infravision, true sight,
      cloud of concealment, flight, immobilize, fumble, suffocate,
      spontaneous generation, divination, silence, ethereal gate

    python3 tests/smoke_test_stub_audit_13_48.py [host] [port]
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
ROOM = 942600 + (int(time.time()) % 25000)
ROOM2 = ROOM + 1
SYMBOL = ROOM + 2
COMPONENT = ROOM + 3
DAGGER = ROOM + 4

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


def get_component(imm_sock, mage_sock):
    check("You conjure" in cmd(imm_sock, f"load obj {COMPONENT}"), "component loaded")
    cmd(imm_sock, "drop pouch")
    check("you get" in cmd(mage_sock, "get pouch").lower(), "the mage picks up a component")


announce("smoke_test_stub_audit_13_48", host, port)

imm_name, imm_pw = f"Ssbimm{_suffix}", "ssbimmpw123"
s = make_char(imm_name, imm_pw, "1")
cmd(s, "quit!")
s.close()
sql(f"UPDATE player_progress SET level=51 WHERE player_id=(SELECT id FROM player WHERE name='{imm_name}');")
s = relog(imm_name, imm_pw)

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'StubAudit1348 Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM2},0,0,0,'StubAudit1348 Gate Target','Another bare room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,val0,val1,can_be_seen) "
    f"VALUES ({SYMBOL},'symbol holy silver','a tarnished silver holy symbol',"
    f"'A tarnished silver holy symbol is lying here.',12,{WEAR_TAKE},5,5,1);")
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({COMPONENT},'pouch component reagent','a pouch of spell components',"
    f"'A pouch of spell components is lying here.',12,{WEAR_TAKE},1);")
WEAR_HOLD = 16384
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({DAGGER},'dagger small','a small dagger','A small dagger is lying here.',5,{WEAR_TAKE | WEAR_HOLD},1);")
check("StubAudit1348" in cmd(s, f"goto {ROOM}"), "the immortal goto's into the sandbox room")

# ===== Cleric batch =====
cleric_name, cleric_pw = f"Ssbcle{_suffix}", "ssbclepw123"
sc = make_char(cleric_name, cleric_pw, "2")
cmd(sc, "quit!")
sc.close()
set_level(cleric_name, 51)
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{cleric_name}';")
sc = relog(cleric_name, cleric_pw)
check("StubAudit1348" in cmd(sc, "look"), "the cleric lands in the sandbox room")

victim_name, victim_pw = f"Ssbvic{_suffix}", "ssbvicpw123"
sv = make_char(victim_name, victim_pw, "1")
cmd(sv, "quit!")
sv.close()
sql(f"UPDATE player_progress SET level=50,hp=9000,max_hp=9000 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{victim_name}');")
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{victim_name}';")
sv = relog(victim_name, victim_pw)
check("StubAudit1348" in cmd(sv, "look"), "the victim lands in the sandbox room")

get_symbol(s, sc)
out = cmd(sc, f"pray flamestrike {victim_name}")
check("striking" in out.lower() or "flamestrike" in out.lower(), "flamestrike lands a real strike")
check("nothing happens" not in out.lower(), "flamestrike isn't a no-op")

get_symbol(s, sc)
out = cmd(sc, "pray expel")
check("nothing was possessing" in out.lower() or "expelled" in out.lower(), "expel reports a real outcome, not a no-op")

get_symbol(s, sc)
out = cmd(sc, f"pray numb {victim_name}")
check("goes numb and clumsy" in out.lower(), "numb lands a real limb debuff")

get_symbol(s, sc)
out = cmd(sc, "pray second wind")
check("vitality return" in out.lower(), "second wind restores vitality for real")

get_symbol(s, sc)
out = cmd(sc, f"pray paralyze {victim_name}")
check("goes limp and unresponsive" in out.lower(), "full paralyze lands a real effect")

get_symbol(s, sc)
out = cmd(sc, "pray earthquake")
check("catching everyone" in out.lower() or "no one else" in out.lower(), "earthquake casts for real, not a no-op")

# ===== Mage batch =====
mage_name, mage_pw = f"Ssbmag{_suffix}", "ssbmagpw123"
sm = make_char(mage_name, mage_pw, "1")
cmd(sm, "quit!")
sm.close()
set_level(mage_name, 51)
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{mage_name}';")
sm = relog(mage_name, mage_pw)
check("StubAudit1348" in cmd(sm, "look"), "the mage lands in the sandbox room")

get_component(s, sm)
out = cmd(sm, f"cast garmul's tail {victim_name}")
check("heavy and clumsy" in out.lower(), "Garmul's tail lands a real debuff")

get_component(s, sm)
out = cmd(sm, f"cast shatter {victim_name}")
check("nothing happens" not in out.lower(), "shatter deals real damage, not a no-op")

get_component(s, sm)
out = cmd(sm, f"cast watery grave {victim_name}")
check("nothing happens" not in out.lower(), "watery grave deals real damage, not a no-op")

get_component(s, sm)
out = cmd(sm, "cast infravision")
check("piercing the darkness" in out.lower(), "infravision casts for real")
check("infravision" in cmd(sm, "affects").lower(), "infravision really applies the affect")

get_component(s, sm)
out = cmd(sm, "cast true sight")
check("piercing illusion" in out.lower(), "true sight casts for real")

get_component(s, sm)
out = cmd(sm, "cast cloud of concealment")
check("obscuring mist" in out.lower(), "cloud of concealment casts for real")
check("invisible" in cmd(sv, "affects").lower(), "cloud of concealment really applies invisibility to the other PC occupant")

get_component(s, sm)
out = cmd(sm, "cast flight")
check("rise gently" in out.lower() or "rises gently" in out.lower(), "flight lands a real room-wide buff")
check("flying" in cmd(sv, "affects").lower(), "flight really gives the OTHER occupant Flying")

get_component(s, sm)
out = cmd(sm, f"cast immobilize {victim_name}")
check("rooted to the spot" in out.lower(), "immobilize lands a real bind")

check("You conjure" in cmd(s, f"load obj {DAGGER}"), "dagger loaded")
cmd(s, "drop dagger")
check("you get" in cmd(sv, "get dagger").lower(), "the victim picks up a dagger to wield")
cmd(sv, "wield dagger")
get_component(s, sm)
out = cmd(sm, f"cast fumble {victim_name}")
check("clatters to the floor" in out.lower(), "fumble lands a real weapon-knock")

get_component(s, sm)
out = cmd(sm, f"cast suffocate {victim_name}")
check("nothing happens" not in out.lower(), "suffocate deals real damage, not a no-op")

gold_before = cmd(sm, "score")
get_component(s, sm)
out = cmd(sm, "cast spontaneous generation xy")
check("more specific" in out.lower(), "spontaneous generation routes through the real materialize gate, not a no-op")

get_component(s, sm)
out = cmd(sm, "cast divination dagger")
check("more specific" not in out.lower(), "divination routes to real identify, not the too-short refusal")
check("nothing happens" not in out.lower(), "divination isn't a no-op")

get_component(s, sm)
out = cmd(sm, f"cast silence {victim_name}")
check("voice dies" in out.lower(), "silence lands a real affect")
silout = cmd(sv, "cast infravision")
check("no sound comes out" in silout.lower(), "a silenced victim really can't cast")

get_component(s, sm)
out = cmd(sm, "cast ethereal gate StubAudit1348 Gate Target")
check("shimmering portal" in out.lower(), "ethereal gate opens a real portal")
check("Gate Target" in cmd(sm, "look"), "ethereal gate really teleports the caster to the named room")

s.close(); sc.close(); sv.close(); sm.close()

sql(f"DELETE FROM room WHERE vnum IN ({ROOM}, {ROOM2});")
sql(f"DELETE FROM obj WHERE vnum IN ({SYMBOL}, {COMPONENT}, {DAGGER});")

announce_done("smoke_test_stub_audit_13_48", host, port)
print("=== ALL CHECKS PASSED ===")
