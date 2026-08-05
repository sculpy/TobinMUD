#!/usr/bin/env python3
"""Smoke test for the level 11-19 Mage stub-audit fixes: accelerate,
sense life, ensorcer, stealth, mage repair, faerie fog, falcon wings,
galvanize, powerstone, calm.

    python3 tests/smoke_test_stub_audit_11_19.py [host] [port]
"""
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done
from mud_creation import create_character

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 942500 + (int(time.time()) % 30000)
COMPONENT = ROOM + 1
MOB = ROOM + 2
DAGGER = ROOM + 3

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


def get_component(imm_sock, mage_sock):
    check("You conjure" in cmd(imm_sock, f"load obj {COMPONENT}"), "component loaded")
    cmd(imm_sock, "drop pouch")
    check("you get" in cmd(mage_sock, "get pouch").lower(), "the mage picks up a component")


announce("smoke_test_stub_audit_11_19", host, port)

imm_name, imm_pw = f"Ssaonimm{_suffix}", "ssa19immpw1"
s = make_char(imm_name, imm_pw, "1")
cmd(s, "quit!")
s.close()
sql(f"UPDATE player_progress SET level=51 WHERE player_id=(SELECT id FROM player WHERE name='{imm_name}');")
s = relog(imm_name, imm_pw)

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'StubAudit1119 Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({COMPONENT},'pouch component reagent','a pouch of spell components',"
    f"'A pouch of spell components is lying here.',12,{WEAR_TAKE},1);")
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,max_struct,cur_struct,can_be_seen) "
    f"VALUES ({DAGGER},'dagger battered small','a battered small dagger',"
    f"'A battered small dagger is lying here.',5,{WEAR_TAKE},20,5,1);")
sql(f"INSERT INTO mob (vnum,name,short_desc,long_desc,description,actions,affects,"
    f"faction,fact_perc,letter,attacks,class,level,tohit,ac,hpbonus,damage_level,"
    f"damage_precision,gold,race,weight,height,str,bra,con,dex,agi,intel,wis,foc,"
    f"per,cha,kar,spe,pos,def_position,sex,spec_proc,skin,vision,can_be_seen,max_exist) "
    f"VALUES ({MOB},'goblin feral','a feral goblin','A feral goblin stands here.',"
    f"'desc',{ACT_AGGRESSIVE},0,0,0,'A',1.0,0,1,50000,0,0.3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"
    f"10,10,1,0,0,0,1,1);")
check("StubAudit1119" in cmd(s, f"goto {ROOM}"), "the immortal goto's into the sandbox room")

mage_name, mage_pw = f"Ssaonmag{_suffix}", "ssa19magpw1"
sm = make_char(mage_name, mage_pw, "1")
cmd(sm, "quit!")
sm.close()
set_level(mage_name, 51)
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{mage_name}';")
sm = relog(mage_name, mage_pw)
check("StubAudit1119" in cmd(sm, "look"), "the mage lands in the sandbox room")

victim_name, victim_pw = f"Ssaonvic{_suffix}", "ssa19vicpw1"
sv = make_char(victim_name, victim_pw, "1")
cmd(sv, "quit!")
sv.close()
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{victim_name}';")
sv = relog(victim_name, victim_pw)
check("StubAudit1119" in cmd(sv, "look"), "the victim lands in the sandbox room")

# --- accelerate: room-wide haste ---
get_component(s, sm)
out = cmd(sm, "cast accelerate")
check("burst of speed" in out.lower(), "accelerate lands a real room-wide buff, not a no-op")
check("haste" in cmd(sv, "affects").lower(), "accelerate really gives the OTHER occupant Haste")

# --- sense life: delegates to real scan ---
get_component(s, sm)
out = cmd(sm, "cast sense life")
check("sensing the living" in out.lower(), "sense life casts for real, not a no-op")

# --- ensorcer: real charm on a mob ---
check("You conjure" in cmd(s, f"load mob {MOB}"), "the feral goblin is loaded")
get_component(s, sm)
out = cmd(sm, "cast ensorcer goblin")
check("bends to your will" in out.lower(), "ensorcer lands a real charm, not a no-op")
follow = cmd(sm, "group").lower()
check("goblin" in follow, "the charmed goblin really shows up as a follower")

# --- stealth: real sneaking toggle ---
get_component(s, sm)
out = cmd(sm, "cast stealth")
check("footsteps fall silent" in out.lower(), "stealth casts for real, not a no-op")

# --- mage repair: real object-target struct restore ---
check("You conjure" in cmd(s, f"load obj {DAGGER}"), "the battered dagger is loaded")
cmd(s, "drop dagger")
check("you get" in cmd(sm, "get dagger").lower(), "the mage picks up the battered dagger")
get_component(s, sm)
out = cmd(sm, "cast mage repair dagger")
check("knits itself back together" in out.lower(), "mage repair lands a real fix, not a no-op")

# --- faerie fog: real AFFECT_BLIND on hostile mobs ---
get_component(s, sm)
out = cmd(sm, "cast faerie fog")
check("illusory mist" in out.lower(), "faerie fog casts for real, not a no-op")

# --- falcon wings: real room-wide flying ---
get_component(s, sm)
out = cmd(sm, "cast falcon wings")
check("rises gently" in out.lower(), "falcon wings lands a real room-wide buff, not a no-op")
check("flying" in cmd(sv, "affects").lower(), "falcon wings really gives the OTHER occupant Flying")

# --- galvanize: reuses enhance weapon's real AFFECT_ENHANCE_WEAPON ---
get_component(s, sm)
out = cmd(sm, "cast galvanize")
check("supernaturally sure" in out.lower(), "galvanize lands a real buff, not a no-op")
check("enhance" in cmd(sm, "affects").lower() or "weapon" in cmd(sm, "affects").lower(),
      "galvanize really applies the Enhanced Weapon affect")

# --- powerstone: reuses charge stave's real mechanic ---
get_component(s, sm)
out = cmd(sm, "cast powerstone")
check("nothing happens yet" not in out.lower(), "powerstone doesn't fall through to the generic no-op placeholder")

# --- calm: real ceasefire on any target ---
get_component(s, sm)
out = cmd(sm, f"cast calm {victim_name}")
check("violence draining away" in out.lower(), "calm lands a real ceasefire, not a no-op")

s.close(); sm.close(); sv.close()

sql(f"DELETE FROM room WHERE vnum={ROOM};")
sql(f"DELETE FROM obj WHERE vnum IN ({COMPONENT}, {DAGGER});")
sql(f"DELETE FROM mob WHERE vnum={MOB};")

announce_done("smoke_test_stub_audit_11_19", host, port)
print("=== ALL CHECKS PASSED ===")
