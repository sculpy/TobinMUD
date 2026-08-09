#!/usr/bin/env python3
"""Smoke test for the first batch of docs/Spell Assignments.xlsx gap-audit
skills (TODO.md, user: "Implement missing skills from docs/Spell
Assignments.xlsx"): `cook`, `whittle`, `defense`, `praying`, `casting`,
`swim`. See skill.c/combat.c/cmd_cook.c/cmd_whittle.c/cmd_pray.c/
cmd_cast.c/vitals.c for the actual code, and TODO.md's own "Missing-skill
gap audit, batch A" section for the full disclosed scope.

Covers:
  1. `cook`/`whittle` now have a real skill gate (previously anyone
     could use either from level 1 regardless of class/discipline) --
     a fresh, never-practiced character is refused both.
  2. `defense` (a new, level-1, generic passive) trains from real combat
     the same way `toughness`/`focused avoidance` already do.
  3. `praying` (Cleric) trains passively on every `pray` attempt,
     alongside whatever prayer was actually cast.
  4. `casting` (Mage) trains passively on every `cast` attempt,
     alongside `wizardry`.
  5. `swim` actually reduces drowning damage (relative comparison: a
     swim-proficient mortal loses less HP per forced drowning tick than
     an otherwise-identical one with no swim proficiency), and trains
     from the attempt.
  6. `help defense`/`help praying`/`help casting`/`help swim` all exist.

    python3 tests/smoke_test_missing_skills_batch_a.py [host] [port]
"""
import re
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 950000 + (int(time.time()) % 40000)
ROOM_WATER = ROOM + 1
MOB_VNUM = ROOM + 2
SYMBOL_VNUM = ROOM + 3
POUCH_VNUM = ROOM + 4


def strip(s):
    return re.sub(r"\x1b\[[0-9;]*m", "", s)


def query(stmt):
    return subprocess.run(["mariadb", "-N", "tobin", "-e", stmt],
                          check=True, capture_output=True, text=True).stdout.strip()


def skill_pct(name, skill_name):
    out = query(f"SELECT pct FROM player_skill WHERE player_id="
                f"(SELECT id FROM player WHERE name='{name}') AND skill_name='{skill_name}';")
    return int(out) if out else 0


def make_char(name, pw, class_choice="1"):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw, "new", name, "1", "1", class_choice, "done", "done"):
        send_line(s, step)
        recv_all(s)
    return s


def relog(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, pw, "1"):
        send_line(s, step)
        recv_all(s)
    cmd(s, "color off")
    return s


def set_hp(name, hp, max_hp):
    sql(f"UPDATE player_progress SET hp={hp}, max_hp={max_hp} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def mob_row(vnum, name_tag):
    cols = {
        "vnum": vnum, "name": f"'{name_tag}'", "short_desc": f"'a {name_tag}'",
        "long_desc": f"'A {name_tag} is here.'", "description": "'desc'",
        "actions": 0, "affects": 0, "faction": 0, "fact_perc": 0,
        "letter": "'A'", "attacks": 1.0,
        "class": 0, "level": 1, "tohit": 0, "ac": 0, "hpbonus": -1.7,
        "damage_level": 0, "damage_precision": 0, "gold": 0, "race": 0,
        "weight": 0, "height": 0, "str": 0, "bra": 0, "con": 0, "dex": 0,
        "agi": 0, "intel": 0, "wis": 0, "foc": 0, "per": 0, "cha": 0,
        "kar": 0, "spe": 0, "pos": 10, "def_position": 10, "sex": 1,
        "spec_proc": 0, "skin": 0, "vision": 0, "can_be_seen": 1,
        "max_exist": 100,
    }
    col_names = ",".join(cols.keys())
    col_values = ",".join(str(v) for v in cols.values())
    return f"INSERT INTO mob ({col_names}) VALUES ({col_values});"


announce("smoke_test_missing_skills_batch_a", host, port)

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'BatchA Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM_WATER},0,0,0,'BatchA Underwater','Murky water presses in.\\n',NULL,1,27,0,0,0,0,0,0,0,0);")
sql(mob_row(MOB_VNUM, "battrainee"))

imm_name, imm_pw = f"Baimm{_suffix}", "baimmpw12345"
si = make_char(imm_name, imm_pw)
cmd(si, "quit!"); si.close()
sql(f"UPDATE player_progress SET level=51 WHERE player_id=(SELECT id FROM player WHERE name='{imm_name}');")
si = relog(imm_name, imm_pw)
cmd(si, f"goto {ROOM}")

# --- 6: help topics ---
for topic, needle in (("defense", "passive"), ("praying", "Cleric"), ("casting", "Mage"), ("swim", "drowning")):
    out = strip(cmd(si, f"help {topic}", timeout=1.5))
    check(needle in out, f"`help {topic}` exists and reads correctly")

# --- 1: cook/whittle gate refuses a never-practiced character ---
gate_name, gate_pw = f"Bagate{_suffix}", "bagatepw1234"
sG = make_char(gate_name, gate_pw)
cmd(sG, "quit!"); sG.close()
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{gate_name}';")
sG = relog(gate_name, gate_pw)

out = strip(cmd(sG, "cook mashed potatoes"))
check("don't know how to cook" in out.lower(), "`cook` refuses a character with 0% Combat discipline")
out = strip(cmd(sG, "whittle arrow"))
check("don't know how to whittle" in out.lower(), "`whittle` refuses a character with 0% Combat discipline")
sG.close()

# --- 2: defense trains from real combat ---
defA_name, defA_pw = f"Badef{_suffix}", "badefpw12345"
sD = make_char(defA_name, defA_pw)
cmd(sD, "quit!"); sD.close()
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{defA_name}';")
sql(f"UPDATE player_progress SET combat_disc_pct=100 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{defA_name}');")
set_hp(defA_name, 500, 500)
sD = relog(defA_name, defA_pw)

check(skill_pct(defA_name, "defense") == 0, "defense starts untrained")
cmd(si, f"load mob {MOB_VNUM}")
out = cmd(sD, "attack battrainee")
for _ in range(10):
    if "You have slain" in out or "You have defeated" in out:
        break
    out += recv_all(sD, 1.5)
check(skill_pct(defA_name, "defense") >= 1, "defense trains from a real combat round")
sD.close()

# --- 3: praying trains alongside a real prayer ---
prA_name, prA_pw = f"Bapray{_suffix}", "baspraypw123"
sP = make_char(prA_name, prA_pw, class_choice="2")  # Cleric
cmd(sP, "quit!"); sP.close()
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{prA_name}';")
sql(f"UPDATE player_progress SET basic_disc_pct=100 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{prA_name}');")
sP = relog(prA_name, prA_pw)

sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({SYMBOL_VNUM},'symbol holy silver','a tarnished silver holy symbol',"
    f"'A tarnished silver holy symbol is lying here.',12,1,1);")
cmd(si, f"load obj {SYMBOL_VNUM}")
cmd(si, "drop symbol")
cmd(sP, "get symbol")

check(skill_pct(prA_name, "praying") == 0, "praying starts untrained")
cmd(sP, "pray heal light")
check(skill_pct(prA_name, "praying") >= 1, "praying trains alongside a real prayer attempt")
sP.close()

# --- 4: casting trains alongside a real cast ---
caA_name, caA_pw = f"Bacast{_suffix}", "bacastpw123"
sC = make_char(caA_name, caA_pw, class_choice="1")  # Mage
cmd(sC, "quit!"); sC.close()
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{caA_name}';")
sql(f"UPDATE player_progress SET level=9, basic_disc_pct=100 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{caA_name}');")
sC = relog(caA_name, caA_pw)

sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({POUCH_VNUM},'pouch component reagent','a pouch of spell components',"
    f"'A pouch of spell components is lying here.',12,1,1);")
cmd(si, f"load obj {POUCH_VNUM}")
cmd(si, "drop pouch")
cmd(sC, "get pouch")

check(skill_pct(caA_name, "casting") == 0, "casting starts untrained")
cmd(sC, "cast gills of flesh")
check(skill_pct(caA_name, "casting") >= 1, "casting trains alongside a real cast attempt")
sC.close()

# --- 5: swim reduces drowning damage (relative) + trains ---
swA_name, swA_pw = f"Baswa{_suffix}", "baswapw12345"
sSA = make_char(swA_name, swA_pw)
cmd(sSA, "quit!"); sSA.close()
sql(f"UPDATE player SET load_room={ROOM_WATER} WHERE name='{swA_name}';")
sql(f"UPDATE player_progress SET combat_disc_pct=100 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{swA_name}');")
set_hp(swA_name, 5000, 5000)
sql(f"INSERT INTO player_skill (player_id, skill_name, pct, last_gain_at) VALUES "
    f"((SELECT id FROM player WHERE name='{swA_name}'), 'swim', 100, {int(time.time())});")
sSA = relog(swA_name, swA_pw)

swB_name, swB_pw = f"Baswb{_suffix}", "baswbpw12345"
sSB = make_char(swB_name, swB_pw)
cmd(sSB, "quit!"); sSB.close()
sql(f"UPDATE player SET load_room={ROOM_WATER} WHERE name='{swB_name}';")
sql(f"UPDATE player_progress SET combat_disc_pct=100 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{swB_name}');")
set_hp(swB_name, 5000, 5000)
sSB = relog(swB_name, swB_pw)

check(skill_pct(swB_name, "swim") == 0, "swim starts untrained")

TICKS = 20
for _ in range(TICKS):
    cmd(si, "aitick 1")
    recv_all(sSA, 0.3)
    recv_all(sSB, 0.3)

outA = cmd(sSA, "score", timeout=1.0)
outB = cmd(sSB, "score", timeout=1.0)
mA = re.search(r"HP:\s+(\d+)/(\d+)", outA)
mB = re.search(r"HP:\s+(\d+)/(\d+)", outB)
check(mA is not None and mB is not None, "both score outputs parsed for HP")
lostA = 5000 - int(mA.group(1))
lostB = 5000 - int(mB.group(1))
check(lostA > 0 and lostB > 0, f"both mortals actually took drowning damage over {TICKS} ticks")
check(lostA < lostB, f"a swim-proficient mortal loses less HP to drowning than one with none ({lostA} < {lostB})")
check(skill_pct(swB_name, "swim") >= 1, "swim trains from a real drowning-tick attempt")

sSA.close(); sSB.close()
si.close()

announce_done("smoke_test_missing_skills_batch_a", host, port)
print("=== ALL CHECKS PASSED ===")
