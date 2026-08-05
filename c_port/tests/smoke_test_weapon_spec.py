#!/usr/bin/env python3
"""Smoke test for the 5 weapon specialization skills (missing-skill
audit, "Generic / cross-class" list; user, 2026-08-04: "all level 1 ...
all of those should be automatic"). Warrior-only (matching real
SneezyMUD's SKILL_WARRIOR ownership), auto-known from character
creation with no guildmaster visit needed, proficiency climbing purely
by landing hits with a matching weapon (or bare-handed), same
learn-by-doing shape as Monk's kubo/cintai.

Covers:
  1. A fresh, never-practiced Warrior's `skills` listing shows all 5
     specializations as already known (a real [N%] line, not grayed out
     "practice Combat discipline first") -- combat_disc_pct is 0 for a
     brand-new character, so this proves the auto-known bypass, not
     coincidental normal practice.
  2. A non-Warrior class (Mage) does NOT get these -- class-restricted,
     matching real upstream.
  3. Landing hits with a slashing weapon raises "slash specialization"
     proficiency from 0%, proving the passive learn-by-doing hook in
     combat_strike() actually fires.

    python3 tests/smoke_test_weapon_spec.py [host] [port]
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
ROOM = 941000 + (int(time.time()) % 30000)
SWORD = ROOM + 1
DUMMY = ROOM + 2

WEAR_TAKE = 1
WEAR_HOLD = 16384
TYPE_WEAPON = 5


def make_char(name, pw, class_choice):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw):
        send_line(s, step)
        recv_all(s)
    create_character(s, name, send_line, recv_all, char_class=class_choice)
    return s


announce("smoke_test_weapon_spec", host, port)

# --- 1: a fresh Warrior already shows all 5 specializations known ---
war_name, war_pw = f"Wspwar{_suffix}", "wspwarpw12345"
sw = make_char(war_name, war_pw, "3")
out = cmd(sw, "skills")
if "ENTER for more" in out:
    out += cmd(sw, "")
for spec in ("slash specialization", "blunt specialization", "pierce specialization",
             "ranged specialization", "barehand specialization"):
    m = re.search(re.escape(spec) + r".*?\[(\d+)%\]", out, re.IGNORECASE)
    check(m is not None, f"a fresh Warrior's skills list shows '{spec}' as known (not grayed out)")

# --- 2: a Mage does NOT get these ---
mage_name, mage_pw = f"Wspmag{_suffix}", "wspmagpw12345"
sm = make_char(mage_name, mage_pw, "1")
out = cmd(sm, "skills")
if "ENTER for more" in out:
    out += cmd(sm, "")
check("slash specialization" not in out.lower(), "a Mage's own skills list has no weapon specializations")

# --- 3: landing hits raises slash specialization proficiency ---
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'WeaponSpec Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,price,material,can_be_seen) "
    f"VALUES ({SWORD},'wspsword sword','a wspsword test sword','A wspsword test sword is lying here.',"
    f"{TYPE_WEAPON},{WEAR_TAKE | WEAR_HOLD},100,0,1);")
sql(f"INSERT INTO mob (vnum,name,short_desc,long_desc,description,actions,affects,"
    f"faction,fact_perc,letter,attacks,class,level,tohit,ac,hpbonus,damage_level,"
    f"damage_precision,gold,race,weight,height,str,bra,con,dex,agi,intel,wis,foc,"
    f"per,cha,kar,spe,pos,def_position,sex,spec_proc,skin,vision,can_be_seen,max_exist) "
    f"VALUES ({DUMMY},'wspdummy dummy','a wspdummy test dummy','A wspdummy test dummy stands here.',"
    f"'A stuffed practice dummy.',0,0,0,0,'A',1.0,0,1,0,0,0.3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"
    f"10,10,1,0,0,0,1,1);")

imm_name, imm_pw = f"Wspimm{_suffix}", "wspimmpw12345"
si = make_char(imm_name, imm_pw, "1")
cmd(si, "quit!"); si.close()
sql(f"UPDATE player_progress SET level=51 WHERE player_id=(SELECT id FROM player WHERE name='{imm_name}');")
si = socket.create_connection((host, port), timeout=5)
recv_all(si)
for step in (imm_name, imm_pw, "1"):
    send_line(si, step)
    recv_all(si)
cmd(si, "color off")

check("WeaponSpec" in cmd(si, f"goto {ROOM}"), "the immortal goto's into the sandbox room")
check("You conjure" in cmd(si, f"load obj {SWORD}"), "the sword is loaded")
cmd(si, "drop sword")
check("You conjure" in cmd(si, f"load mob {DUMMY}"), "the harmless practice dummy is loaded")

# Fresh, distinct Warrior for the combat check (not `sw` from part 1,
# which already had a live interactive session at Center Square before
# this point -- found live that a character's room placement via
# `UPDATE player SET load_room=...` only reliably takes effect if it
# lands BEFORE that character's first real interactive command, not
# just before their next quit/relog).
war2_name, war2_pw = f"Wspwtw{_suffix}", "wspwtwpw12345"
sw2 = make_char(war2_name, war2_pw, "3")
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{war2_name}';")
cmd(sw2, "quit!"); sw2.close()
sw2 = socket.create_connection((host, port), timeout=5)
recv_all(sw2)
for step in (war2_name, war2_pw, "1"):
    send_line(sw2, step)
    recv_all(sw2)
cmd(sw2, "color off")
check("WeaponSpec" in cmd(sw2, "look"), "the fresh warrior lands in the sandbox room")
check("you get" in cmd(sw2, "get sword").lower(), "the warrior picks up the sword")
check("wield" in cmd(sw2, "wield sword").lower(), "the warrior wields the sword")

for _ in range(8):
    cmd(sw2, "kill dummy", timeout=1.5)
    time.sleep(1.5)

out = cmd(sw2, "skills")
if "ENTER for more" in out:
    out += cmd(sw2, "")
m = re.search(r"slash specialization.*?\[(\d+)%\]", out, re.IGNORECASE)
check(m is not None and int(m.group(1)) > 0,
      "landing hits with a slashing weapon raised slash specialization's proficiency above 0%")

sw.close(); sw2.close(); sm.close(); si.close()

sql(f"DELETE FROM room WHERE vnum={ROOM};")
sql(f"DELETE FROM obj WHERE vnum={SWORD};")
sql(f"DELETE FROM mob WHERE vnum={DUMMY};")

announce_done("smoke_test_weapon_spec", host, port)
print("=== ALL CHECKS PASSED ===")
