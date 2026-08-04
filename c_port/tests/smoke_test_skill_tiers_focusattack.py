#!/usr/bin/env python3
"""Smoke test for two related user requests (2026-08-03):

  1. "all other skills belong in basic and advanced class discipline" /
     "just weapon proficiency in combat" / "barehand slash stab
     proficiency etc" -- SKILL_TIER_COMBAT (skill.c) now holds ONLY the
     5 literal "<type> proficiency" rows (slash/blunt/pierce/barehand/
     ranged) per class; every other skill that used to be Combat tier
     (riding, parry, dodge, kick, backstab, steal, sneak, search, etc)
     moved to SKILL_TIER_CLASS (Basic) or SKILL_TIER_ADVANCED, using
     each skill's own existing min_level as the split (< 25 -> Basic,
     >= 25 -> Advanced).
  2. "in tobin its a warrior skill" / "focused attack should be
     automatic" -- `focus attack` (already a Warrior roster entry, never
     implemented) now fires automatically during ordinary melee combat
     (combat_strike(), combat.c), same "reroll toward a MAJOR limb"
     mechanic `critical hitting` (Monk) already uses -- no separate
     command, no manual arming step, matching the real upstream's own
     effect (forces a crit) without its manual-trigger-plus-cooldown
     shape.

Covers:
  1. A Mage's `skills` Combat section lists ONLY the 5 proficiency
     skills -- `riding` (previously Combat) is not among them.
  2. `riding` now shows up in the Mage's general (Basic-tier) skill
     listing instead.
  3. A Warrior at 100% Combat/Basic discipline with `focus attack`
     forced to full proficiency lands at least one "focus intensely"
     flavor message over several real combat rounds against a durable
     sandbox mob.

    python3 tests/smoke_test_skill_tiers_focusattack.py [host] [port]
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
ROOM = 900000 + (int(time.time() * 1000) % 70000)
MOB_VNUM = ROOM + 1


announce("smoke_test_skill_tiers_focusattack", host, port)


def cmd_paged(sock, line, timeout=1.0):
    out = cmd(sock, line, timeout)
    full = out
    while "enter" in out.lower() and "more" in out.lower():
        out = cmd(sock, "", timeout)
        full += out
    return full


# Territory (2026-08-03) is a forced step right after race: race, then
# homeland (1-3), then class.
def make_char(name, pw, class_num):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw, "new", name, "1", "1", str(class_num), "done", "done"):
        send_line(s, step)
        recv_all(s)
    return s


# --- 1/2: Combat tier is proficiency-only; riding moved to the general list ---
mage_name, mage_pw = f"Tiermag{_suffix}", "tiermagpw12345"
s = make_char(mage_name, mage_pw, 1)  # Mage
cmd(s, "color off")
out = cmd_paged(s, "skills")
combat_section = out.split("-- Mage Skills --")[0]
check(all(p in combat_section for p in
          ("slash proficiency", "blunt proficiency", "pierce proficiency",
           "barehand proficiency", "ranged proficiency")),
      "Combat section lists all 5 weapon/barehand proficiency skills")
check("riding" not in combat_section.lower(),
      "'riding' is no longer in the Combat section")
check("riding" in out.lower(),
      "'riding' still appears somewhere in the full skills listing (moved to Basic)")
s.close()

# --- 3: focus attack fires automatically in real combat ---
war_name, war_pw = f"Tierwar{_suffix}", "tierwarpw12345"
s = make_char(war_name, war_pw, 3)  # Warrior
cmd(s, "quit!")
s.close()
sql(f"UPDATE player_progress SET level=51, basic_disc_pct=100, combat_disc_pct=100, "
    f"hp=5000, max_hp=5000, vit=5000, max_vit=5000 "
    f"WHERE player_id=(SELECT id FROM player WHERE name='{war_name}');")
s = socket.create_connection((host, port), timeout=5)
recv_all(s)
for step in (war_name, war_pw, "1"):
    send_line(s, step)
    recv_all(s)
cmd(s, "color off")

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'FocusAttack Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
check("FocusAttack Sandbox" in cmd(s, f"goto {ROOM}"), "goto lands in the sandbox room")

cols = {
    "vnum": MOB_VNUM, "name": "'focusdummy'", "short_desc": "'a focus-attack dummy'",
    "long_desc": "'A focus-attack dummy stands here.'", "description": "'desc'",
    "actions": 0, "affects": 0, "faction": 0, "fact_perc": 0,
    "letter": "'A'", "attacks": 1.0,
    "class": 0, "level": 1, "tohit": -50, "ac": 0, "hpbonus": 300,
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
cmd(s, f"load mob {MOB_VNUM}")
cmd(s, "toggle pk")

out = cmd(s, "hit focusdummy")
check("you attack" in out.lower() or "you engage" in out.lower(), "combat starts against the sandbox dummy")

seen_focus = False
for _ in range(15):
    out = cmd(s, "look")
    if "focus intensely" in out.lower():
        seen_focus = True
        break

check(seen_focus, "focus attack's flavor message appears automatically during ordinary combat, "
                   "with no command typed to trigger it")
s.close()

announce_done("smoke_test_skill_tiers_focusattack", host, port)
print("=== ALL CHECKS PASSED ===")
