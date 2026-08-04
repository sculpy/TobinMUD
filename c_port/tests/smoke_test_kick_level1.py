#!/usr/bin/env python3
"""Smoke test for kick being available at level 1 for every non-caster
class (user, 2026-08-04: "kick skill should be received at level 1 for
all classes who get the skill", then "all classes except for casters
should get kick at level 1"). `kick` was already level 1 for Thief
(skill.c's SKILLS[] roster) but level 3 for Monk; both are now level 1,
and Warrior (previously no kick at all) was added at level 1 too --
Warrior/Thief/Monk are Tobin's three non-caster classes.

Covers:
  1. A freshly-created level-1 Warrior/Thief/Monk can already `kick` a
     target (not refused for being under-level or for not knowing the
     skill at all).
  2. A freshly-created level-1 Mage (a caster) is refused kick outright
     -- confirming the non-caster scoping, not a blanket grant.

    python3 tests/smoke_test_kick_level1.py [host] [port]
"""
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 960000 + (int(time.time()) % 30000)
MOB = ROOM + 1


def make_char(name, pw, class_num):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw, "new", name, "1", "1", str(class_num), "done", "done"):
        # steps: name, confirm, pw, pw confirm, "new", char name, race=1, homeland=1, class, done (attrs), done (finish)
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


def kick_while_recovering(sock, tries=6, timeout=0.5):
    """`kick` (like most commands) refuses with "You are still
    recovering!" while the caller is lag-locked from the swing that just
    engaged combat (`wait_pulses`) -- not a socket-timing artifact, a
    real command gate (same `score_while_fighting()` lesson from the
    copyover-fight test). Retries past it instead of taking the first
    (likely-refused) response at face value."""
    for _ in range(tries):
        out = cmd(sock, "kick", timeout)
        if "still recovering" not in out:
            return out
    return out


announce("smoke_test_kick_level1", host, port)

imm_name, imm_pw = f"Kkl{_suffix}", "kklpw1234567"

s1 = make_char(imm_name, imm_pw, 3)  # Warrior (level 51+ needed for `load`/`goto`)
cmd(s1, "quit!"); s1.close()
sql(f"UPDATE player_progress SET level=59 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{imm_name}');")
s1 = relog(imm_name, imm_pw)

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Kick Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
check("Kick Sandbox" in cmd(s1, f"goto {ROOM}"), "goto lands in the SQL-bootstrapped sandbox room")

sql(f"INSERT INTO mob (vnum,name,short_desc,long_desc,description,actions,affects,"
    f"faction,fact_perc,letter,attacks,class,level,tohit,ac,hpbonus,damage_level,"
    f"damage_precision,gold,race,weight,height,str,bra,con,dex,agi,intel,wis,foc,"
    f"per,cha,kar,spe,pos,def_position,sex,spec_proc,skin,vision,can_be_seen,max_exist) "
    f"VALUES ({MOB},'dummy{_suffix}','a kick test dummy','A kick test dummy stands here.',"
    f"'A stuffed practice dummy.',0,0,0,0,'A',1.0,0,1,0,999,0.1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"
    f"10,10,1,0,0,0,1,1);")

for name, class_num in ((f"Kkw{_suffix}", 3), (f"Kkm{_suffix}", 6), (f"Kkt{_suffix}", 4)):  # Warrior=3, Monk=6, Thief=4
    pw = "kkclasspw123"
    sc = make_char(name, pw, class_num)
    cmd(sc, "quit!"); sc.close()
    sql(f"UPDATE player SET load_room={ROOM} WHERE name='{name}';")
    # `kick` is SKILL_TIER_CLASS -- being_knows_skill() also requires
    # basic_disc_pct > 0 (the Basic discipline must have been practiced
    # at least once), independent of level -- a fresh level-1 character
    # has 0 by default, which would refuse kick regardless of the level
    # gate this test is actually checking.
    sql(f"UPDATE player_progress SET basic_disc_pct=100 WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")
    sc = relog(name, pw)
    check("Kick Sandbox" in cmd(sc, "look"), f"{name} lands in the sandbox room")
    check("You conjure" in cmd(s1, f"load mob {MOB}"), f"a fresh dummy is loaded for {name}")
    # `kick` (cmd_kick.c) takes NO target argument -- it's an extra action
    # layered onto an already-ongoing fight (ch->fighting must be set), not
    # a standalone targeted attack, so combat has to be engaged first via
    # `attack`.
    attack_out = cmd(sc, "attack dummy", 0.5)
    check("you attack" in attack_out.lower(), f"{name} engages the dummy before kicking")
    out = kick_while_recovering(sc)
    check("you don't know how to kick" not in out.lower() and "not high enough level" not in out.lower(),
          f"a level-1 {name}'s class is never refused kick for being under-level")
    sc.close()
    cmd(s1, "purge")

# --- 2: a caster (Mage) never gets kick at all -- non-caster scoping, not a blanket grant ---
mage_name, mage_pw = f"Kkc{_suffix}", "kkcasterpw12"
sm = make_char(mage_name, mage_pw, 1)  # Mage
cmd(sm, "quit!"); sm.close()
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{mage_name}';")
sm = relog(mage_name, mage_pw)
check("Kick Sandbox" in cmd(sm, "look"), "the caster lands in the sandbox room")
check("You conjure" in cmd(s1, f"load mob {MOB}"), "a fresh dummy is loaded for the caster")
attack_out = cmd(sm, "attack dummy", 0.5)
check("you attack" in attack_out.lower(), "the caster engages the dummy before kicking")
out = kick_while_recovering(sm)
check("don't know how to kick" in out.lower(), "a Mage (caster) is refused kick outright")
sm.close()
cmd(s1, "purge")

s1.close()

announce_done("smoke_test_kick_level1", host, port)
print("=== ALL CHECKS PASSED ===")
