#!/usr/bin/env python3
"""Smoke test for `advanced berserking` (Warrior, level 35, missing-skill
audit backlog: TODO.md's "Unimplemented skills/spells backlog"). See
combat.c's combat_process_run() -- while berserking (AFFECT_BERSERK) and
knowing the skill, each round gets a proficiency-scaled chance at a bonus
follow-up combat_strike(), same "genuine bonus combat_strike(), CHANCE-
gated per round" shape the pre-existing chain attack/blur/advanced
kicking block uses.

Proves the passive's learn-by-doing hook actually FIRES in real play while
berserking (skill starts at 0; a real round of berserk combat trains it to
the floor, same deterministic first-attempt guarantee
skill_learn_from_doing() documents and smoke_test_combat_passives_generic.py
already exercises for its own passives), and that the help topic loads a
real body describing the mechanic (not the old placeholder).

    python3 tests/smoke_test_advanced_berserking.py [host] [port]
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
ROOM = 954000 + (int(time.time()) % 30000)


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


def set_level_class(name, level, cls):
    sql(f"UPDATE player_progress SET level={level}, basic_disc_pct=100, combat_disc_pct=100, "
        f"advanced_disc_pct=100, hp=500, max_hp=500 WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")
    sql(f"UPDATE player SET class={cls} WHERE name='{name}';")


def poke_fight(sock, target_name, settle=1.6):
    send_line(sock, f"hit {target_name}")
    time.sleep(settle)
    recv_all(sock, 0.2)


announce("smoke_test_advanced_berserking", host, port)

imm_name, imm_pw = f"Abimm{_suffix}", "abimmpw12345"
si = make_char(imm_name, imm_pw)
cmd(si, "quit!"); si.close()
sql(f"UPDATE player_progress SET level=60 WHERE player_id=(SELECT id FROM player WHERE name='{imm_name}');")
si = relog(imm_name, imm_pw)

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Berserk Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")

# =====================================================================
# 1: advanced berserking trains from real combat rounds spent berserking.
#    `berserk` pct is seeded to 100 so entering the rage is deterministic
#    (skill_roll_success(100) always succeeds); `advanced berserking`
#    itself is left untrained to prove skill_learn_from_doing() fires
#    for it every round the attacker is berserking.
# =====================================================================
at_name, at_pw = f"Abat{_suffix}", "abatpw12345"
sat = make_char(at_name, at_pw)
cmd(sat, "quit!"); sat.close()
set_level_class(at_name, 40, 2)  # Warrior, level 40 (>=35 so advanced berserking known)
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{at_name}';")
sql(f"INSERT INTO player_skill (player_id, skill_name, pct) "
    f"VALUES ((SELECT id FROM player WHERE name='{at_name}'), 'berserk', 100) "
    f"ON DUPLICATE KEY UPDATE pct=100;")
sat = relog(at_name, at_pw)
check(skill_pct(at_name, "advanced berserking") == 0, "advanced berserking starts untrained")

cmd(si, f"goto {ROOM}")
out = strip(cmd(sat, "berserk", timeout=1.5))
check("berserk rage" in out.lower(), f"berserk rage triggers deterministically: {out[:80]!r}")

# `berserk` itself sets a 2*COMBAT_ROUND_PULSES (2.4s) wait-state on the
# attacker before their next command lands -- clear it before poking the
# fight, then give several full combat rounds (COMBAT_ROUND_PULSES =
# 1.2s/round) to run, well inside AFFECT_BERSERK's 8-round duration.
time.sleep(2.6)
recv_all(sat, 0.2)
poke_fight(sat, imm_name, settle=3.0)   # PC attacks the immortal, still berserking
recv_all(sat, 0.3)
check(skill_pct(at_name, "advanced berserking") >= 1,
      "advanced berserking trains from a real round of berserk combat")
cmd(sat, "quit!"); sat.close()

# =====================================================================
# 2: the help topic loads a real body describing the actual mechanic,
#    not the old "An upgraded berserk with a stronger effect" placeholder.
# =====================================================================
out = strip(cmd(si, "help advanced berserking", timeout=1.5))
check("bonus" in out.lower() and "round" in out.lower(),
      f"help advanced berserking describes the real mechanic: {out[:80]!r}")
check("upgraded berserk" not in out.lower(),
      "help advanced berserking no longer shows the old placeholder text")

si.close()
announce_done("smoke_test_advanced_berserking", host, port)
print("=== ALL CHECKS PASSED ===")
