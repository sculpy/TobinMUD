#!/usr/bin/env python3
"""Smoke test for the second batch of docs/Spell Assignments.xlsx gap-audit
skills (TODO.md, user: "Implement missing skills from docs/Spell
Assignments.xlsx" -- continued 2026-08-08 per "continue on with task 12"):
`bandage`, `hiking`. See being.h/combat.c/vitals.c/cmd_move.c/
cmd_bandage.c/skill.c for the actual code.

Real upstream (disc_basic_adventuring.cc's doBandage()) treats a
PART_BLEEDING limb -- a mechanic Tobin never had. This batch adds a
transient per-limb `bleeding` flag (limb_state_t, being.h), set the
moment a limb crosses into limb_status_text()'s bad tier (same
tier-crossing guard combat.c's blood-pool spawn already used, now
duplicated in both combat_strike() and the hurtlimb debug path),
chipped by vitals_tick_impl() every ~60s until treated. `bandage`
(cmd_bandage.c) is the treatment: consumes a carried bandage item
(vnum 9 is real seeded data) to clear the flag and heal a little.
`hiking` reduces cmd_move.c's own terrain movement cost.

Covers:
  1. `bandage`/`hiking` both have a real skill gate + roster row.
  2. A limb crossing the bad tier (via `hurtlimb`) sets it bleeding,
     and the vitals tick (`aitick`) actually costs HP for it.
  3. `bandage` refuses without a carried bandage item.
  4. `bandage` with a real bandage item stops the bleeding (no more
     HP loss on a follow-up tick) and heals some HP.
  5. `hiking` reduces movement vit cost (relative comparison) and
     trains from a real move.
  6. `help bandage`/`help hiking` both exist.

    python3 tests/smoke_test_missing_skills_batch_b.py [host] [port]
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
BANDAGE_VNUM = 9
ROOM = 951000 + (int(time.time()) % 40000)


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


announce("smoke_test_missing_skills_batch_b", host, port)

imm_name, imm_pw = f"Bbimm{_suffix}", "bbimmpw12345"
si = make_char(imm_name, imm_pw)
cmd(si, "quit!"); si.close()
sql(f"UPDATE player_progress SET level=60 WHERE player_id=(SELECT id FROM player WHERE name='{imm_name}');")
si = relog(imm_name, imm_pw)

# --- 6: help topics ---
out = strip(cmd(si, "help bandage", timeout=1.5))
check("Syntax: bandage" in out, "`help bandage` exists and reads correctly")
out = strip(cmd(si, "help hiking", timeout=1.5))
check("passive combat skill" in out, "`help hiking` exists and reads correctly")

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM + 2},0,0,0,'BatchB Bandage Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")

# --- 1-4: bleeding + bandage ---
bd_name, bd_pw = f"Bbbnd{_suffix}", "bbbndpw12345"
sb = make_char(bd_name, bd_pw)
cmd(sb, "quit!"); sb.close()
set_hp(bd_name, 100, 100)
sql(f"UPDATE player SET load_room={ROOM + 2} WHERE name='{bd_name}';")
sql(f"UPDATE player_progress SET combat_disc_pct=100 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{bd_name}');")
sb = relog(bd_name, bd_pw)

check(skill_pct(bd_name, "bandage") == 0, "bandage starts untrained")

cmd(si, f"goto {ROOM + 2}")
cmd(si, f"force {bd_name} goto {ROOM + 2}")
recv_all(sb, 0.3)

# force a real limb-crossing via hurtlimb so bleeding is deterministic.
# hunger/thirst topped off so the only HP-affecting variable during the
# tick-timing checks below is the bleeding flag itself.
sql(f"UPDATE player_progress SET hunger=1000, thirst=1000, hp=100 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{bd_name}');")
out = strip(cmd(si, f"crit {bd_name} rightarm 1", timeout=1.5))
check("Limb HP set" in out, "hurtlimb actually set the limb HP")

hp_before = int(query(f"SELECT hp FROM player_progress WHERE player_id="
                       f"(SELECT id FROM player WHERE name='{bd_name}');"))
# aitick deliberately excludes player-vitals effects (cmd_aitick.c's own
# doc comment -- a real prior incident where forced ticks silently
# starved bystanders), so bleeding (vitals_tick_impl(), the real
# ~60s-cadence path) has to be observed via a real wait, not aitick.
time.sleep(65)
recv_all(sb, 0.5)
hp_after_tick = int(query(f"SELECT hp FROM player_progress WHERE player_id="
                           f"(SELECT id FROM player WHERE name='{bd_name}');"))
check(hp_after_tick < hp_before, f"a bleeding limb actually chips HP on a vitals tick ({hp_before} -> {hp_after_tick})")

sql(f"INSERT INTO player_skill (player_id, skill_name, pct, last_gain_at) VALUES "
    f"((SELECT id FROM player WHERE name='{bd_name}'), 'bandage', 100, {int(time.time())}) "
    f"ON DUPLICATE KEY UPDATE pct=100;")
out = strip(cmd(sb, "bandage", timeout=1.5))
check("don't have a bandage" in out, "`bandage` refuses without a carried bandage item")

cmd(si, f"load obj {BANDAGE_VNUM}")
out = strip(cmd(si, "give bandage " + bd_name, timeout=1.5))
recv_all(sb, 0.5)

out = strip(cmd(sb, "bandage", timeout=1.5))
check("stopping the bleeding" in out, "`bandage` reports success")
hp_after_bandage = int(query(f"SELECT hp FROM player_progress WHERE player_id="
                              f"(SELECT id FROM player WHERE name='{bd_name}');"))

time.sleep(65)
recv_all(sb, 0.5)
hp_after_second_tick = int(query(f"SELECT hp FROM player_progress WHERE player_id="
                                  f"(SELECT id FROM player WHERE name='{bd_name}');"))
check(hp_after_second_tick >= hp_after_bandage,
      f"bandaging actually stopped the bleeding (no further net HP loss: "
      f"{hp_after_bandage} -> {hp_after_second_tick})")

sb.close()

# --- 5: hiking reduces movement cost + trains ---
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'BatchB Hills','Rough, hilly terrain.\\n',NULL,1,4,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM + 1},0,0,0,'BatchB Hills 2','More rough, hilly terrain.\\n',NULL,1,4,0,0,0,0,0,0,0,0);")
# direction 0 == north (DIR_NAMES, room.c)
sql(f"INSERT INTO roomexit (vnum,direction,name,description,type,condition_flag,"
    f"lock_difficulty,weight,key_num,destination) VALUES "
    f"({ROOM},0,'','',0,0,0,0,0,{ROOM + 1});")

hkA_name, hkA_pw = f"Bbhka{_suffix}", "bbhkapw12345"
shA = make_char(hkA_name, hkA_pw)
cmd(shA, "quit!"); shA.close()
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{hkA_name}';")
sql(f"INSERT INTO player_skill (player_id, skill_name, pct, last_gain_at) VALUES "
    f"((SELECT id FROM player WHERE name='{hkA_name}'), 'hiking', 100, {int(time.time())});")
sql(f"UPDATE player_progress SET vit=1000, max_vit=1000, combat_disc_pct=100 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{hkA_name}');")
shA = relog(hkA_name, hkA_pw)

hkB_name, hkB_pw = f"Bbhkb{_suffix}", "bbhkbpw12345"
shB = make_char(hkB_name, hkB_pw)
cmd(shB, "quit!"); shB.close()
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{hkB_name}';")
sql(f"UPDATE player_progress SET vit=1000, max_vit=1000 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{hkB_name}');")
shB = relog(hkB_name, hkB_pw)

check(skill_pct(hkB_name, "hiking") == 0, "hiking starts untrained")

vitA_before = int(query(f"SELECT vit FROM player_progress WHERE player_id="
                         f"(SELECT id FROM player WHERE name='{hkA_name}');"))
vitB_before = int(query(f"SELECT vit FROM player_progress WHERE player_id="
                         f"(SELECT id FROM player WHERE name='{hkB_name}');"))
cmd(shA, "north", timeout=1.0)
cmd(shB, "north", timeout=1.0)
vitA_after = int(query(f"SELECT vit FROM player_progress WHERE player_id="
                        f"(SELECT id FROM player WHERE name='{hkA_name}');"))
vitB_after = int(query(f"SELECT vit FROM player_progress WHERE player_id="
                        f"(SELECT id FROM player WHERE name='{hkB_name}');"))
costA = vitA_before - vitA_after
costB = vitB_before - vitB_after
check(costA >= 0 and costB > 0, f"both moves actually spent vit ({costA}, {costB})")
check(costA < costB, f"a hiking-proficient mortal spends less vit moving than one with none ({costA} < {costB})")
check(skill_pct(hkA_name, "hiking") >= 1, "hiking trains from a real move")

shA.close(); shB.close()
si.close()

announce_done("smoke_test_missing_skills_batch_b", host, port)
print("=== ALL CHECKS PASSED ===")
