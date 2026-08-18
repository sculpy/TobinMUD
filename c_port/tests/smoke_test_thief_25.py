#!/usr/bin/env python3
"""Smoke test for the Session-158-backlog level-25 Thief cluster
(2026-08-18): skulk, track, poison weapon. See cmd_skulk.c, cmd_track.c,
cmd_poison_weapon.c.

Covers:
  * SKULK        -> toggles on/off; a character without the skill is
                    refused; the skill trains from a successful toggle.
  * TRACK        -> points the first-hop direction toward a quarry in an
                    adjacent room; "right here" when co-located; "no sign"
                    for an absent name; refused without the skill.
  * POISON WEAPON-> coats a wielded weapon (Poisoned Blade affect); recast
                    refused while coated; refused with no weapon in hand;
                    and a coated weapon poisons a victim it hits in combat.

    python3 tests/smoke_test_thief_25.py [host] [port]
"""
import re
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26 ** i) % 26) for i in range(4))
ROOM_A = 956000 + (int(time.time()) % 15000)
ROOM_B = ROOM_A + 1
MOB = ROOM_A + 5
SWORD = ROOM_A + 6

WEAR_TAKE = 1
WEAR_HOLD = 16384
TYPE_WEAPON = 5


def query(stmt):
    return subprocess.run(["mariadb", "-N", "tobin", "-e", stmt],
                          check=True, capture_output=True, text=True).stdout.strip()


def skill_pct(name, skill_name):
    out = query(f"SELECT pct FROM player_skill WHERE player_id="
                f"(SELECT id FROM player WHERE name='{name}') AND skill_name='{skill_name}';")
    return int(out) if out else 0


def make_char(name, pw, class_num):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw, "new", name, "1", "1", str(class_num), "done", "done"):
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


def mkroom(vnum, name):
    sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
        f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
        f"VALUES ({vnum},0,0,0,'{name}','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")


announce("smoke_test_thief_25", host, port)

mkroom(ROOM_A, "Thief25 Sandbox A")
mkroom(ROOM_B, "Thief25 Sandbox B")
# An exit east from A -> B (no door), so track has a one-hop path.
sql(f"INSERT INTO roomexit (vnum,direction,name,description,type,condition_flag,"
    f"lock_difficulty,weight,key_num,destination) VALUES ({ROOM_A},1,'','',0,0,0,0,0,{ROOM_B});")
sql(f"INSERT INTO mob (vnum,name,short_desc,long_desc,description,actions,affects,"
    f"faction,fact_perc,letter,attacks,class,level,tohit,ac,hpbonus,damage_level,"
    f"damage_precision,gold,race,weight,height,str,bra,con,dex,agi,intel,wis,foc,"
    f"per,cha,kar,spe,pos,def_position,sex,spec_proc,skin,vision,can_be_seen,max_exist) "
    f"VALUES ({MOB},'quarry{_suffix}','a tracking quarry','A tracking quarry stands here.',"
    f"'A quarry to be tracked.',0,0,0,0,'A',1.0,0,1,0,999,0.1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"
    f"10,10,1,0,0,0,1,1);")
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({SWORD},'sword testsword','a test sword','A test sword is lying here.',"
    f"{TYPE_WEAPON},{WEAR_TAKE | WEAR_HOLD},1);")

# Immortal helper (level 60) -- loads mobs/objs, and is the tracked/attacked party.
imm, ipw = f"Tfi{_suffix}", "tfipw1234567"
si = make_char(imm, ipw, 4)  # Thief
cmd(si, "quit!"); si.close()
sql(f"UPDATE player_progress SET level=60 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{imm}');")
si = relog(imm, ipw)

# =================== SKULK ===================
sk_name, sk_pw = f"Tfs{_suffix}", "tfspw1234567"
ss = make_char(sk_name, sk_pw, 4)  # Thief
cmd(ss, "quit!"); ss.close()
sql(f"UPDATE player SET load_room={ROOM_A} WHERE name='{sk_name}';")
sql(f"UPDATE player_progress SET level=30, basic_disc_pct=100, combat_disc_pct=100, "
    f"advanced_disc_pct=100 WHERE player_id=(SELECT id FROM player WHERE name='{sk_name}');")
ss = relog(sk_name, sk_pw)
check("Thief25 Sandbox A" in cmd(ss, "look"), "the skulking thief lands in sandbox A")
check(skill_pct(sk_name, "skulk") == 0, "skulk starts untrained")
out = cmd(ss, "skulk")
check("begin skulking" in out.lower(), f"skulk toggles on: {out[:70]!r}")
check("stop skulking" in cmd(ss, "skulk").lower(), "skulk toggles back off")
check(skill_pct(sk_name, "skulk") >= 1, "skulk trains from a successful toggle")

# A non-thief is refused.
mg_name, mg_pw = f"Tfm{_suffix}", "tfmpw1234567"
sm = make_char(mg_name, mg_pw, 1)  # Mage
cmd(sm, "quit!"); sm.close()
sql(f"UPDATE player SET load_room={ROOM_A} WHERE name='{mg_name}';")
sm = relog(mg_name, mg_pw)
check("don't know how to skulk" in cmd(sm, "skulk").lower(), "a non-thief is refused skulk")
cmd(sm, "quit!"); sm.close()

# =================== TRACK ===================
# Load the quarry mob into room B (immortal must be there to load it).
cmd(si, f"goto {ROOM_B}")
check("You conjure" in cmd(si, f"load mob {MOB}"), "the quarry mob is loaded into room B")
cmd(si, f"goto {ROOM_A}")
# Immortal tracks the quarry one room east.
out = cmd(si, f"track quarry{_suffix}")
check("leads east" in out.lower(), f"track points east toward the quarry: {out[:80]!r}")
# Co-located: track the mob from its own room.
cmd(si, f"goto {ROOM_B}")
out = cmd(si, f"track quarry{_suffix}")
check("right here" in out.lower(), f"track reports a co-located quarry as here: {out[:70]!r}")
# Absent name.
out = cmd(si, "track nobodyxyz")
check("no sign" in out.lower(), f"track finds no trail for an absent name: {out[:70]!r}")
# A character without the skill is refused (reuse the Mage).
sm = relog(mg_name, mg_pw)
check("don't know how to track" in cmd(sm, "track quarry").lower(), "a non-thief is refused track")
cmd(sm, "quit!"); sm.close()

# =================== POISON WEAPON ===================
# A mortal Thief who knows the skill, wielding a weapon, coats it.
pw_name, pw_pw = f"Tfp{_suffix}", "tfppw1234567"
sp = make_char(pw_name, pw_pw, 4)  # Thief
cmd(sp, "quit!"); sp.close()
sql(f"UPDATE player SET load_room={ROOM_A} WHERE name='{pw_name}';")
sql(f"UPDATE player_progress SET level=30, basic_disc_pct=100, advanced_disc_pct=100, "
    f"combat_disc_pct=100, hp=800, max_hp=800 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{pw_name}');")
# Max out the poison-weapon proficiency so the coat roll is deterministic
# (a freshly-known advanced skill otherwise fumbles most attempts).
sql(f"INSERT INTO player_skill (player_id, skill_name, pct) "
    f"SELECT id, 'poison weapon', 100 FROM player WHERE name='{pw_name}' "
    f"ON DUPLICATE KEY UPDATE pct=100;")
sp = relog(pw_name, pw_pw)
check("Thief25 Sandbox A" in cmd(sp, "look"), "the poisoner thief lands in sandbox A")

# No weapon in hand -> refused.
check("need a weapon in hand" in cmd(sp, "poison weapon").lower(),
      "poison weapon is refused with no weapon wielded")

# Wield the sword, then coat it.
cmd(si, f"goto {ROOM_A}")
cmd(si, f"load obj {SWORD}"); recv_all(si, 0.3)
# hand the sword to the poisoner: load a second copy directly to them.
cmd(sp, "look")
check("You conjure" in cmd(si, f"load obj {SWORD}"), "a test sword is loaded")
cmd(si, "drop sword"); recv_all(si, 0.3)
cmd(sp, "get sword"); recv_all(sp, 0.3)
cmd(sp, "wield sword"); recv_all(sp, 0.3)
out = cmd(sp, "poison weapon")
check("smear a slick of venom" in out.lower(), f"poison weapon coats the blade: {out[:70]!r}")
time.sleep(1.5)  # let the skill's combat-lag round clear before the next command
recv_all(sp, 0.2)
check("poisoned blade" in cmd(sp, "affects").lower(), "the Poisoned Blade affect is active")
check("already coated" in cmd(sp, "poison weapon").lower(), "recast is refused while coated")

# The coated weapon poisons a MORTAL victim in combat (the proc never
# envenoms an immortal, by design). Create a huge-HP mortal to soak the
# rounds while the 40%-per-hit proc gets its chance.
vic_name, vic_pw = f"Tfv{_suffix}", "tfvpw1234567"
svq = make_char(vic_name, vic_pw, 3)  # Warrior
cmd(svq, "quit!"); svq.close()
sql(f"UPDATE player SET load_room={ROOM_A} WHERE name='{vic_name}';")
sql(f"UPDATE player_progress SET level=40, hp=999999, max_hp=999999 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{vic_name}');")
svq = relog(vic_name, vic_pw)
check("Thief25 Sandbox A" in cmd(svq, "look"), "the poison victim lands in sandbox A")
cmd(sp, "toggle pk"); cmd(svq, "toggle pk")
# Ensure the coating is still live at the moment combat opens (a no-op
# "already coated" refusal if it hasn't worn off; a fresh coat if it has).
cmd(sp, "poison weapon"); recv_all(sp, 0.2)
cmd(sp, f"attack {vic_name}"); recv_all(sp, 0.4)
poisoned = False
for _ in range(12):
    time.sleep(1.2)
    recv_all(sp, 0.2)
    if "poison" in cmd(svq, "affects").lower():
        poisoned = True
        break
check(poisoned, "a poisoned weapon eventually envenoms the mortal victim it hits")

# Cleanup.
cmd(svq, "quit!"); svq.close()
cmd(sp, "quit!"); sp.close()
cmd(si, "quit!"); si.close()
sql(f"DELETE FROM player_progress WHERE player_id IN "
    f"(SELECT id FROM player WHERE name IN ('{imm}','{sk_name}','{mg_name}','{pw_name}','{vic_name}'));")
sql(f"DELETE FROM player WHERE name IN ('{imm}','{sk_name}','{mg_name}','{pw_name}','{vic_name}');")
sql(f"DELETE FROM roomexit WHERE vnum={ROOM_A};")
sql(f"DELETE FROM room WHERE vnum IN ({ROOM_A},{ROOM_B});")
sql(f"DELETE FROM mob WHERE vnum={MOB};")
sql(f"DELETE FROM obj WHERE vnum={SWORD};")

announce_done("smoke_test_thief_25", host, port)
print("=== ALL CHECKS PASSED ===")
