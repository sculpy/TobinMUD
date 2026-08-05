#!/usr/bin/env python3
"""Smoke test for `beast soother` (Druid, level 5), `faerie fire`
(Mage, level 6), and `feathery descent` (Mage, level 7) stub-audit
fixes.

    python3 tests/smoke_test_beastsoother_faeriefire_feathery.py [host] [port]
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
ROOM = 942300 + (int(time.time()) % 30000)
COMPONENT = ROOM + 1
MOB = ROOM + 2

WEAR_TAKE = 1
ACT_AGGRESSIVE = 1


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


announce("smoke_test_beastsoother_faeriefire_feathery", host, port)

imm_name, imm_pw = f"Sbffimm{_suffix}", "sbffimmpw1"
s = make_char(imm_name, imm_pw, "1")
cmd(s, "quit!")
s.close()
sql(f"UPDATE player_progress SET level=51 WHERE player_id=(SELECT id FROM player WHERE name='{imm_name}');")
s = relog(imm_name, imm_pw)

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'BeastFF Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({COMPONENT},'pouch component reagent','a pouch of spell components',"
    f"'A pouch of spell components is lying here.',12,{WEAR_TAKE},1);")
sql(f"INSERT INTO mob (vnum,name,short_desc,long_desc,description,actions,affects,"
    f"faction,fact_perc,letter,attacks,class,level,tohit,ac,hpbonus,damage_level,"
    f"damage_precision,gold,race,weight,height,str,bra,con,dex,agi,intel,wis,foc,"
    f"per,cha,kar,spe,pos,def_position,sex,spec_proc,skin,vision,can_be_seen,max_exist) "
    f"VALUES ({MOB},'wolf feral','a feral wolf','A feral wolf stands here.',"
    f"'desc',{ACT_AGGRESSIVE},0,0,0,'A',1.0,0,1,50000,0,0.3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"
    f"10,10,1,0,0,0,1,1);")
check("BeastFF" in cmd(s, f"goto {ROOM}"), "the immortal goto's into the sandbox room")

druid_name, druid_pw = f"Sbffdru{_suffix}", "sbffdrupw1"
sd = make_char(druid_name, druid_pw, "3")
cmd(sd, "quit!")
sd.close()
set_level(druid_name, 51)
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{druid_name}';")
sd = relog(druid_name, druid_pw)
check("BeastFF" in cmd(sd, "look"), "the druid lands in the sandbox room")

mage_name, mage_pw = f"Sbffmag{_suffix}", "sbffmagpw1"
sm = make_char(mage_name, mage_pw, "1")
cmd(sm, "quit!")
sm.close()
set_level(mage_name, 51)
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{mage_name}';")
sm = relog(mage_name, mage_pw)
check("BeastFF" in cmd(sm, "look"), "the mage lands in the sandbox room")

# --- beast soother (Druid, level 5): real ceasefire, and a calmed
# aggressive mob stays calm across forced AI ticks (same aitick-forcing
# technique smoke_test_alignment.py uses to make mob aggression
# deterministic) ---
check("You conjure" in cmd(s, f"load mob {MOB}"), "the feral wolf is loaded")
check("You conjure" in cmd(s, f"load obj {COMPONENT}"), "component 1 loaded")
cmd(s, "drop pouch")
check("you get" in cmd(sd, "get pouch").lower(), "the druid picks up component 1")

soothed_out = cmd(sd, "cast beast soother wolf")
check("grows calm and still" in soothed_out.lower(), "beast soother lands a real ceasefire, not a no-op")
check("nothing happens" not in soothed_out.lower(), "beast soother doesn't fall through to the no-op placeholder")
hp_before = re.search(r"HP:\s*(\d+)", cmd(sd, "score")).group(1)
cmd(s, "aitick 30")
hp_after = re.search(r"HP:\s*(\d+)", cmd(sd, "score")).group(1)
check(hp_before == hp_after, "a calmed aggressive wolf takes no swings at the druid across forced AI ticks (HP unchanged)")

# --- faerie fire (Mage, level 6): real AFFECT_FAERIE_FIRE on a target ---
check("You conjure" in cmd(s, f"load obj {COMPONENT}"), "component 2 loaded")
cmd(s, "drop pouch")
check("you get" in cmd(sm, "get pouch").lower(), "the mage picks up component 2")
ffout = cmd(sm, f"cast faerie fire {druid_name}")
check("shimmering pink aura" in ffout.lower(), "faerie fire lands a real debuff, not a no-op")
faff = cmd(sd, "affects").lower()
check("faerie fire" in faff, "the victim really carries the Faerie Fire affect")

# --- feathery descent (Mage, level 7): real room-wide AFFECT_FEATHERY_DESCENT ---
check("You conjure" in cmd(s, f"load obj {COMPONENT}"), "component 3 loaded")
cmd(s, "drop pouch")
check("you get" in cmd(sm, "get pouch").lower(), "the mage picks up component 3")
fdout = cmd(sm, "cast feathery descent")
check("lighter than air" in fdout.lower(), "feathery descent lands a real room-wide buff, not a no-op")
fdaff = cmd(sd, "affects").lower()
check("feathery descent" in fdaff, "the OTHER occupant (not just the caster) carries Feathery Descent")

s.close(); sd.close(); sm.close()

sql(f"DELETE FROM room WHERE vnum={ROOM};")
sql(f"DELETE FROM obj WHERE vnum={COMPONENT};")
sql(f"DELETE FROM mob WHERE vnum={MOB};")

announce_done("smoke_test_beastsoother_faeriefire_feathery", host, port)
print("=== ALL CHECKS PASSED ===")
