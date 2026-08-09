#!/usr/bin/env python3
"""Smoke test for `kick <target>` starting a fight (user, 2026-08-05:
"kick should be a way to start a fight", implemented 2026-08-08).
`kick` previously required ch->fighting already set (an extra action
layered onto an ongoing fight only); now `kick <target>` also opens a
fight from scratch, same target-lookup + fighting-pointer-swap shape
cmd_attack.c uses. See cmd_kick.c.

Covers:
  1. `kick <target>` while not fighting anyone starts the fight (both
     sides' `fighting` pointer set -- confirmed via a follow-up bare
     `kick` succeeding against the same opponent with no target arg).
  2. Bare `kick` with no target and no ongoing fight is still refused
     ("Kick whom?"), not silently accepted.

    python3 tests/smoke_test_kick_starts_fight.py [host] [port]
"""
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 962000 + (int(time.time()) % 30000)
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


announce("smoke_test_kick_starts_fight", host, port)

imm_name, imm_pw = f"Kfimm{_suffix}", "kfimmpw12345"
s1 = make_char(imm_name, imm_pw, 3)
cmd(s1, "quit!"); s1.close()
sql(f"UPDATE player_progress SET level=60 WHERE player_id=(SELECT id FROM player WHERE name='{imm_name}');")
s1 = relog(imm_name, imm_pw)

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Kick-Start Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
_mob_cols = {
    "vnum": MOB, "name": "'dummy'", "short_desc": "'a training dummy'",
    "long_desc": "'A training dummy stands here.'", "description": "'desc'",
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
sql(f"INSERT INTO mob ({','.join(_mob_cols.keys())}) VALUES "
    f"({','.join(str(v) for v in _mob_cols.values())});")

wt_name, wt_pw = f"Kfw{_suffix}", "kfwpw12345"
sw = make_char(wt_name, wt_pw, 3)  # Warrior
cmd(sw, "quit!"); sw.close()
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{wt_name}';")
sql(f"UPDATE player_progress SET basic_disc_pct=100 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{wt_name}');")
sw = relog(wt_name, wt_pw)

# --- 2: no target, not fighting -> refused ---
out = cmd(sw, "kick", 1.0)
check("kick whom" in out.lower(), "bare `kick` with no ongoing fight and no target is refused")

# --- 1: kick <target> opens the fight ---
cmd(s1, f"goto {ROOM}")
cmd(s1, f"load mob {MOB}")
out = cmd(sw, "kick dummy", 1.0)
check("kick whom" not in out.lower() and "aren't here" not in out.lower(),
      "`kick <target>` while not fighting is accepted")
# Fight actually opened: a bare `kick` (no target) now succeeds against
# the same opponent without re-specifying them.
time.sleep(2.5)  # clear kick's own post-hit wait
out2 = cmd(sw, "kick", 1.0)
check("kick whom" not in out2.lower(), "a follow-up bare `kick` (no target) now works -- the fight is real")

sw.close()
s1.close()

announce_done("smoke_test_kick_starts_fight", host, port)
print("=== ALL CHECKS PASSED ===")
