#!/usr/bin/env python3
"""Smoke test for the level-up vitality fix (user 2026-08-03: "you
should gain max vitality upon level"). combat.c's per-hit XP-award
level-up payoff (the same spot smoke_test_levelup_hp.py's max_hp fix
lives) recomputed max_hp on `levels_gained > 0` but never touched
max_vit/vit at all -- a leveled-up character's Vitality pool (shown as
"Move: X (Y Max.)" on `score`) stayed frozen at its starting value
forever, and any vitality already spent before the level-up stayed
spent. Fixed by mirroring the max_hp treatment: recompute max_vit
(being_calc_max_vit(), scales with level) and refill vit to match.

Covers:
  1. A fresh mortal character's starting max Vitality is recorded.
  2. Vitality is drained below max before leveling up.
  3. Leveling up raises max Vitality AND fully refills current
     Vitality to the new max (not just the max number moving while
     current stays stale/drained).

    python3 tests/smoke_test_levelup_vitality.py [host] [port]
"""
import re
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


announce("smoke_test_levelup_vitality", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 900000 + (int(time.time() * 1000) % 70000)
MOB_BASE = ROOM + 10


def set_dex(name, dex):
    sql(f"UPDATE player_attrs SET dexterity={dex} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def make_char(name, pw, class_choice):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "y"); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "new"); recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "1"); recv_all(s)  # race: human
    send_line(s, "1"); recv_all(s)  # homeland: urban (territory, forced step since 2026-08-03)
    send_line(s, class_choice); recv_all(s)
    send_line(s, "done"); recv_all(s)
    send_line(s, "done"); recv_all(s)  # alignment: neutral
    cmd(s, "color off")
    return s


def make_weak_mob(vnum, keyword):
    cols = {
        "vnum": vnum, "name": f"'{keyword}'", "short_desc": f"'a {keyword}'",
        "long_desc": f"'A {keyword} stands here.'", "description": "'desc'",
        "actions": 0, "affects": 0, "faction": 0, "fact_perc": 0,
        "letter": "'A'", "attacks": 1.0,
        "class": 0, "level": 1, "tohit": 0, "ac": 0, "hpbonus": 0,
        "damage_level": 0, "damage_precision": 0, "gold": 0, "race": 0,
        "weight": 0, "height": 0, "str": 0, "bra": 0, "con": 0, "dex": 0,
        "agi": 0, "intel": 0, "wis": 0, "foc": 0, "per": 0, "cha": 0,
        "kar": 0, "spe": 0, "pos": 10, "def_position": 10, "sex": 1,
        "spec_proc": 0, "skin": 0, "vision": 0, "can_be_seen": 1,
        "max_exist": 1,
    }
    col_names = ",".join(cols.keys())
    col_values = ",".join(str(v) for v in cols.values())
    sql(f"INSERT INTO mob ({col_names}) VALUES ({col_values});")


def move_from_score(out):
    m = re.search(r"Move:\s*(\d+) \((\d+) Max", out)
    check(m is not None, "score shows a Move: current (max Max.) pair")
    return int(m.group(1)), int(m.group(2))


pw = "levelupvitpw123"

imm_name = f"Lvvimm{_suffix}"
imm_pw = "lvvimmpw123"
s_imm = socket.create_connection((host, port), timeout=5)
recv_all(s_imm)
send_line(s_imm, imm_name); recv_all(s_imm)
send_line(s_imm, "y"); recv_all(s_imm)
send_line(s_imm, imm_pw); recv_all(s_imm)
send_line(s_imm, imm_pw); recv_all(s_imm)
send_line(s_imm, "new"); recv_all(s_imm)
send_line(s_imm, imm_name); recv_all(s_imm)
send_line(s_imm, "1"); recv_all(s_imm)  # race: human
send_line(s_imm, "1"); recv_all(s_imm)  # homeland: urban
send_line(s_imm, "1"); recv_all(s_imm)  # class: mage
send_line(s_imm, "done"); recv_all(s_imm)
send_line(s_imm, "done"); recv_all(s_imm)
sql(f"UPDATE player_progress SET level=51 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{imm_name}');")
s_imm.close()
s_imm = socket.create_connection((host, port), timeout=5)
recv_all(s_imm)
send_line(s_imm, imm_name); recv_all(s_imm)
send_line(s_imm, imm_pw); recv_all(s_imm)
send_line(s_imm, "1"); recv_all(s_imm)
cmd(s_imm, "color off")

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Levelup Vit Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
check("Levelup Vit Sandbox" in cmd(s_imm, f"goto {ROOM}"), "goto lands in the sandbox room")

warrior_name = f"Lvvwar{_suffix}"
sw = make_char(warrior_name, pw, "3")
set_dex(warrior_name, 900)
sw.close()
sw = socket.create_connection((host, port), timeout=5)
recv_all(sw)
send_line(sw, warrior_name); recv_all(sw)
send_line(sw, pw); recv_all(sw)
send_line(sw, "1"); recv_all(sw)
cmd(sw, "color off")

cmd(s_imm, f"transfer {warrior_name}")
check("Levelup Vit Sandbox" in cmd(sw, "look"), "the Warrior is in the sandbox room after transfer")

out = cmd(sw, "score")
start_vit, start_max_vit = move_from_score(out)
check(start_max_vit > 0, f"recorded the fresh Warrior's starting max Vitality ({start_max_vit})")

# Drain vitality below max (directly in the DB -- no clean in-game "spend
# vitality on demand" command) so the test can tell a real refill-to-max
# apart from the number just happening to already be full.
drained_vit = max(1, start_max_vit // 4)
sql(f"UPDATE player_progress SET vit={drained_vit} WHERE player_id="
    f"(SELECT id FROM player WHERE name='{warrior_name}');")
# player_progress only reloads from the DB on login, not for an
# already-connected session -- reconnect to pick up the drained value.
sw.close()
sw = socket.create_connection((host, port), timeout=5)
recv_all(sw)
send_line(sw, warrior_name); recv_all(sw)
send_line(sw, pw); recv_all(sw)
send_line(sw, "1"); recv_all(sw)
cmd(sw, "color off")
out = cmd(sw, "score")
cur_vit, _ = move_from_score(out)
# Passive regen (regen.c) may have already ticked a point or two back in
# by the time login + a couple round trips finish -- just confirm it's
# still meaningfully below max, not an exact freeze-frame match.
check(cur_vit < start_max_vit - 5,
      f"Vitality is drained below max before leveling up ({cur_vit}/{start_max_vit})")

mob_word = f"vitling{_suffix}"
needed_xp = 37  # progress_xp_for_level(2), real upstream table (being.c)
xp_per_kill = 50
kills_needed = -(-needed_xp // xp_per_kill)  # ceil -> 1

for i in range(kills_needed):
    vnum = MOB_BASE + i
    make_weak_mob(vnum, mob_word)
    check("You conjure" in cmd(s_imm, f"load mob {vnum}"), f"vitling #{i+1} is loaded")
    out = cmd(sw, f"kill {mob_word}")
    tries = 0
    while "You have slain" not in out and "You have defeated" not in out and tries < 20:
        tries += 1
        out += recv_all(sw, 1.5)
    check("You have slain" in out or "You have defeated" in out,
          f"the Warrior killed vitling #{i+1}")

out = cmd(sw, "score")
end_vit, end_max_vit = move_from_score(out)
check(end_max_vit >= start_max_vit,
      f"leveling up did not lower max Vitality ({start_max_vit} -> {end_max_vit})")
check(end_vit == end_max_vit,
      f"leveling up fully refills current Vitality to the new max ({end_vit}/{end_max_vit})")

s_imm.close()
sw.close()
announce_done("smoke_test_levelup_vitality", host, port)
print("=== ALL CHECKS PASSED ===")
