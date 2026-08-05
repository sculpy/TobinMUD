#!/usr/bin/env python3
"""Smoke test for missing-skill audit batch 2 (generic/cross-class):
toughness (passive damage reduction), focused avoidance (passive
to-hit reduction), evaluate (new command, tiered item appraisal).

    python3 tests/smoke_test_missing_skills_2.py [host] [port]
"""
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done
from mud_creation import create_character

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 943000 + (int(time.time()) % 25000)
TRINKET = ROOM + 1
MOB = ROOM + 2

WEAR_TAKE = 1
WEAR_HOLD = 16384


def set_level(name, level):
    sql(f"UPDATE player_progress SET level={level} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def seed_proficiency(name, skill_name, pct):
    sql(f"INSERT INTO player_skill (player_id, skill_name, pct, last_gain_at) "
        f"VALUES ((SELECT id FROM player WHERE name='{name}'), '{skill_name}', {pct}, {int(time.time())}) "
        f"ON DUPLICATE KEY UPDATE pct={pct};")


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


announce("smoke_test_missing_skills_2", host, port)

imm_name, imm_pw = f"Smtwoimm{_suffix}", "smtwoimmpw1"
s = make_char(imm_name, imm_pw, "1")
cmd(s, "quit!")
s.close()
sql(f"UPDATE player_progress SET level=51 WHERE player_id=(SELECT id FROM player WHERE name='{imm_name}');")
s = relog(imm_name, imm_pw)

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'MissingSkill2 Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,price,max_struct,cur_struct,"
    f"material,can_be_seen) VALUES ({TRINKET},'trinket cheap','a cheap trinket',"
    f"'A cheap trinket is lying here.',12,{WEAR_TAKE | WEAR_HOLD},200,20,20,5,1);")
sql(f"INSERT INTO mob (vnum,name,short_desc,long_desc,description,actions,affects,"
    f"faction,fact_perc,letter,attacks,class,level,tohit,ac,hpbonus,damage_level,"
    f"damage_precision,gold,race,weight,height,str,bra,con,dex,agi,intel,wis,foc,"
    f"per,cha,kar,spe,pos,def_position,sex,spec_proc,skin,vision,can_be_seen,max_exist) "
    f"VALUES ({MOB},'dummy training','a training dummy','A training dummy stands here.',"
    f"'desc',0,0,0,0,'A',1.0,0,1,50000,0,0.3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"
    f"10,10,1,0,0,0,1,1);")
check("MissingSkill2" in cmd(s, f"goto {ROOM}"), "the immortal goto's into the sandbox room")

# --- toughness: passive damage reduction, verify combat still resolves ---
war_name, war_pw = f"Smtwowar{_suffix}", "smtwowarpw1"
sw = make_char(war_name, war_pw, "3")
cmd(sw, "quit!")
sw.close()
set_level(war_name, 51)
sql(f"UPDATE player_progress SET basic_disc_pct=100, combat_disc_pct=100, advanced_disc_pct=100 "
    f"WHERE player_id=(SELECT id FROM player WHERE name='{war_name}');")
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{war_name}';")
sw = relog(war_name, war_pw)
check("MissingSkill2" in cmd(sw, "look"), "the warrior lands in the sandbox room")

check("You conjure" in cmd(s, f"load mob {MOB}"), "the training dummy is loaded")
out = cmd(sw, "kill dummy")
check("nothing happens" not in out.lower(), "combat starts normally before toughness is known")
time.sleep(1)

seed_proficiency(war_name, "toughness", 100)
out2 = cmd(sw, "hit dummy")
check("nothing happens" not in out2.lower(), "combat with toughness known still resolves normally, not a no-op")
time.sleep(1)

# --- focused avoidance: passive to-hit reduction against attacker,
#     verify combat still resolves without error (same ongoing fight) ---
seed_proficiency(war_name, "focused avoidance", 100)
out3 = cmd(sw, "hit dummy")
check("nothing happens" not in out3.lower(), "combat with focused avoidance known still resolves normally")
cmd(sw, "flee")
sw.close()

# --- evaluate: tiered appraisal ---
thf_name, thf_pw = f"Smtwothf{_suffix}", "smtwothfpw1"
st = make_char(thf_name, thf_pw, "4")
cmd(st, "quit!")
st.close()
set_level(thf_name, 10)
sql(f"UPDATE player_progress SET basic_disc_pct=100 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{thf_name}');")
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{thf_name}';")
st = relog(thf_name, thf_pw)
check("MissingSkill2" in cmd(st, "look"), "the thief lands in the sandbox room")

out4 = cmd(st, "evaluate")
check("evaluate what" in out4.lower(), "evaluate with no argument gives a real usage prompt")

check("You conjure" in cmd(s, f"load obj {TRINKET}"), "the trinket is loaded")
cmd(s, "drop trinket")
check("you get" in cmd(st, "get trinket").lower(), "the thief picks up the trinket")

seed_proficiency(thf_name, "evaluate", 5)
low = cmd(st, "evaluate trinket")
check("worth somewhere around" in low.lower(), "low-proficiency evaluate gives a rough price guess")
check("made of" not in low.lower(), "low-proficiency evaluate withholds material detail")

seed_proficiency(thf_name, "evaluate", 100)
high = cmd(st, "evaluate trinket")
check("worth somewhere around 200 gold" in high.lower(), "high-proficiency evaluate gives the exact price")
check("made of" in high.lower(), "high-proficiency evaluate reveals material tier")
check("it looks" in high.lower(), "high-proficiency evaluate reveals condition")
st.close()

s.close()

sql(f"DELETE FROM room WHERE vnum={ROOM};")
sql(f"DELETE FROM obj WHERE vnum={TRINKET};")
sql(f"DELETE FROM mob WHERE vnum={MOB};")

announce_done("smoke_test_missing_skills_2", host, port)
print("=== ALL CHECKS PASSED ===")
