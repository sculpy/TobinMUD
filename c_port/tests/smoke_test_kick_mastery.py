#!/usr/bin/env python3
"""Smoke test for Monk kick mastery unlocking advanced kicking
automatically (user, 2026-08-04: "once kick is maxed then advanced kick
should take over as an automatic attack"). `being_knows_skill()`
(skill.c) now treats a Monk as knowing "advanced kicking" once their own
"kick" proficiency (player_skill.pct) reaches 100%, even below the
skill's normal level-25 practice-point gate -- combat_process_run()'s
existing bonus-strike check (combat.c, the same one haste/chain attack/
blur already use) treats "advanced kicking" as one of its triggers for a
barehanded, ~50%-chance EXTRA strike each round, so mastering kick now
unlocks that automatically.

`being_knows_skill()` has no direct player-facing surface of its own
(cmd_skills.c's listing re-implements its own level/discipline gating
independently, a disclosed pre-existing gap this change doesn't touch)
-- the only observable effect is combat itself. Verified statistically:
a level-1 (well below the normal level-25 unlock) Monk with kick maxed,
fighting a harmless punching-bag mob for a fixed number of real combat
rounds, should land noticeably MORE own-swing lines than there were
rounds -- without any bonus-strike source, a barehanded fighter gets
exactly one swing per round, so any consistent excess is the mastery
unlock firing (no other bonus-strike skill is available to a level-1
Monk).

    python3 tests/smoke_test_kick_mastery.py [host] [port]
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
ROOM = 963000 + (int(time.time()) % 30000)
MOB = ROOM + 1


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


announce("smoke_test_kick_mastery", host, port)

imm_name, imm_pw = f"Kkmi{_suffix}", "kkmipw123456"
monk_name, monk_pw = f"Kkmm{_suffix}", "kkmmpw123456"

s1 = make_char(imm_name, imm_pw, 3)  # Warrior (level 51+ needed for `load`/`goto`)
cmd(s1, "quit!"); s1.close()
sql(f"UPDATE player_progress SET level=59 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{imm_name}');")
s1 = relog(imm_name, imm_pw)

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Kick Mastery Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
check("Kick Mastery Sandbox" in cmd(s1, f"goto {ROOM}"), "goto lands in the SQL-bootstrapped sandbox room")

# A harmless punching bag: huge HP, negligible damage -- the fight must
# survive many rounds without the mob ever landing a real hit or dying.
# Built as a dict (column name -> value) rather than hand-typed parallel
# comma strings, so it's trivial to eyeball-verify and a count mismatch
# is structurally impossible.
_mob_row = {
    "vnum": str(MOB),
    "name": f"'punchbag{_suffix}'",
    "short_desc": "'a punching bag dummy'",
    "long_desc": "'A punching bag dummy hangs here.'",
    "description": "'A stuffed practice dummy.'",
    "actions": "0", "affects": "0", "faction": "0", "fact_perc": "0",
    "letter": "'A'", "attacks": "1.0", "class": "0", "level": "1",
    "tohit": "-999", "ac": "9999.9", "hpbonus": "0.1",
    "damage_level": "0", "damage_precision": "0", "gold": "0",
    "race": "0", "weight": "0", "height": "0",
    "str": "0", "bra": "0", "con": "0", "dex": "0", "agi": "0",
    "intel": "0", "wis": "0", "foc": "0", "per": "0", "cha": "0", "kar": "0", "spe": "0",
    "pos": "10", "def_position": "10", "sex": "1",
    "spec_proc": "0", "skin": "0", "vision": "0", "can_be_seen": "1", "max_exist": "1",
}
sql(f"INSERT INTO mob ({', '.join(_mob_row.keys())}) VALUES ({', '.join(_mob_row.values())});")

sm = make_char(monk_name, monk_pw, 6)  # Monk
cmd(sm, "quit!"); sm.close()
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{monk_name}';")
sql(f"INSERT INTO player_skill (player_id, skill_name, pct) "
    f"SELECT id, 'kick', 100 FROM player WHERE name='{monk_name}' "
    f"ON DUPLICATE KEY UPDATE pct=100;")
sm = relog(monk_name, monk_pw)
check("Kick Mastery Sandbox" in cmd(sm, "look"), "the level-1 Monk lands in the sandbox room")

check("You conjure" in cmd(s1, f"load mob {MOB}"), "the punching bag dummy is loaded")

out = cmd(sm, "attack punchbag", 0.5)
check("you attack" in out.lower(), "the Monk engages the dummy")

# Collect ROUNDS worth of combat output (short per-read timeouts -- combat
# rounds land every ~1.2s, faster than a longer default read window would
# tolerate, same lesson smoke_test_copyover_fight.py's own helpers document).
ROUNDS = 10
full = ""
for _ in range(ROUNDS):
    full += cmd(sm, "", 1.3)

full_lower = full.lower()
swing_lines = len(re.findall(r"you (?:hit|miss) ", full_lower))
check(swing_lines > ROUNDS,
      f"a kick-mastered level-1 Monk lands more swings ({swing_lines}) than combat rounds elapsed "
      f"({ROUNDS}) -- the mastery-unlocked bonus strike is firing")

sm.close()
s1.close()

announce_done("smoke_test_kick_mastery", host, port)
print("=== ALL CHECKS PASSED ===")
