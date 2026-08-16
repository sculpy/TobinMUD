#!/usr/bin/env python3
"""Smoke test for the 5 weapon specialization skills after they were moved
to the Advanced discipline (user: "specialization skills in advanced";
the weapon *proficiency* skills stay in the Combat discipline). Warrior-only
(real SneezyMUD's SKILL_WARRIOR ownership). No longer auto-known: a spec is
locked until the Warrior has mastered Basic AND Combat and begun Advanced
practice, and its learn-by-doing proficiency is then capped by
advanced_disc_pct (the "max potential" the skills/practice views show) --
this is the fix for "slash specialization 77/71 ... shouldn't go past max
potential".

Covers:
  1. A fresh Warrior's `skills` shows the specializations under the Advanced
     tier, LOCKED ("master Basic and Combat first"), not as a live [N%].
  2. A non-Warrior class (Mage) does NOT get these at all.
  3. A Warrior who has mastered Basic+Combat and started Advanced sees the
     specializations as KNOWN [N%], and `practice advanced` shows their max
     potential equal to advanced_disc_pct (the ceiling), not 100.
  4. Landing hits with a slashing weapon (once unlocked) raises slash
     specialization proficiency above 0%.

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

SPECS = ("slash specialization", "blunt specialization", "pierce specialization",
         "ranged specialization", "barehand specialization")


def make_char(name, pw, class_choice):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw):
        send_line(s, step)
        recv_all(s)
    create_character(s, name, send_line, recv_all, char_class=class_choice)
    cmd(s, "color off")
    return s


def feed(sock, command):
    """Read a full (possibly long, paginated) listing, draining every page.
    Uses raw socket reads with a settle window -- the skills roster is long
    enough that a bare cmd()/idle-timeout drain truncates it mid-page."""
    send_line(sock, command)
    buf = ""
    for _ in range(80):
        time.sleep(0.2)
        sock.settimeout(0.5)
        try:
            while True:
                d = sock.recv(8192)
                if not d:
                    break
                buf += d.decode(errors="replace")
        except socket.timeout:
            pass
        if "more" in buf[-40:].lower():
            send_line(sock, "")
        else:
            break
    return buf


def relog(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, pw, "1"):
        send_line(s, step)
        recv_all(s)
    cmd(s, "color off")
    return s


announce("smoke_test_weapon_spec", host, port)

# --- 1: a fresh Warrior's specializations are LOCKED under Advanced ---
war_name, war_pw = f"Wspwar{_suffix}", "wspwarpw12345"
sw = make_char(war_name, war_pw, "3")
out = feed(sw, "skills")
for spec in SPECS:
    m = re.search(re.escape(spec) + r"[^\r\n]*", out, re.IGNORECASE)
    check(m is not None, f"a fresh Warrior's skills list still lists '{spec}' (under Advanced)")
    check("%]" not in m.group(0),
          f"'{spec}' shows no live [N%] percentage for a fresh, un-disciplined Warrior (it is locked)")
check(re.search(r"slash specialization[^\r\n]*master Basic and Combat", out, re.IGNORECASE) is not None,
      "a fresh Warrior's slash specialization shows the Advanced lock reason (master Basic and Combat first)")

# --- 2: a Mage does NOT get these ---
mage_name, mage_pw = f"Wspmag{_suffix}", "wspmagpw12345"
sm = make_char(mage_name, mage_pw, "1")
out = feed(sm, "skills")
check("slash specialization" not in out.lower(), "a Mage's own skills list has no weapon specializations")
sm.close()

# --- bootstrap sandbox room + sword + dummy for the combat check ---
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
si = relog(imm_name, imm_pw)
check("WeaponSpec" in cmd(si, f"goto {ROOM}"), "the immortal goto's into the sandbox room")
check("You conjure" in cmd(si, f"load obj {SWORD}"), "the sword is loaded")
cmd(si, "drop sword")
check("You conjure" in cmd(si, f"load mob {DUMMY}"), "the harmless practice dummy is loaded")

# --- 3: a Warrior who has mastered Basic+Combat and begun Advanced sees the
#        specializations as KNOWN, with max potential == advanced_disc_pct ---
war2_name, war2_pw = f"Wspwtw{_suffix}", "wspwtwpw12345"
sw2 = make_char(war2_name, war2_pw, "3")
cmd(sw2, "quit!"); sw2.close()
# Mortal (level 50 so discipline gates still apply), Basic+Combat mastered,
# Advanced started at 40 -> specializations unlock and their ceiling is 40.
sql(f"UPDATE player_progress SET level=50, basic_disc_pct=100, combat_disc_pct=100, "
    f"advanced_disc_pct=40 WHERE player_id=(SELECT id FROM player WHERE name='{war2_name}');")
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{war2_name}';")
sw2 = relog(war2_name, war2_pw)
check("WeaponSpec" in cmd(sw2, "look"), "the disciplined warrior lands in the sandbox room")

out = feed(sw2, "skills")
m = re.search(r"slash specialization[^\r\n]*\[(\d+)%\]", out, re.IGNORECASE)
check(m is not None, "a Basic+Combat-mastered, Advanced-started Warrior sees slash specialization as KNOWN [N%]")

pav = feed(sw2, "practice advanced")
check(re.search(r"slash specialization[^\r\n]*\(\d+/40\)", pav, re.IGNORECASE) is not None,
      "practice advanced shows slash specialization's max potential as advanced_disc_pct (40), not 100")

# --- 4: landing hits (now unlocked) raises slash specialization above 0% ---
check("you get" in cmd(sw2, "get sword").lower(), "the warrior picks up the sword")
check("wield" in cmd(sw2, "wield sword").lower(), "the warrior wields the sword")
for _ in range(6):
    cmd(sw2, "kill dummy", timeout=1.5)
    time.sleep(1.2)
out = feed(sw2, "skills")
m = re.search(r"slash specialization[^\r\n]*\[(\d+)%\]", out, re.IGNORECASE)
check(m is not None and int(m.group(1)) > 0,
      "landing hits with a slashing weapon raised slash specialization above 0% (learn-by-doing hook fires once unlocked)")
check(m is not None and int(m.group(1)) <= 40,
      "slash specialization proficiency stays at or below its advanced_disc_pct ceiling (40)")

sw.close(); sw2.close(); si.close()

sql(f"DELETE FROM room WHERE vnum={ROOM};")
sql(f"DELETE FROM obj WHERE vnum={SWORD};")
sql(f"DELETE FROM mob WHERE vnum={DUMMY};")

announce_done("smoke_test_weapon_spec", host, port)
print("=== ALL CHECKS PASSED ===")
