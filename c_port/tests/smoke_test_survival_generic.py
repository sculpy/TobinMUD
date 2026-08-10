#!/usr/bin/env python3
"""Smoke test for the generic survival/extraction batch (missing-skill
audit, generic/cross-class, 2026-08-10): climbing, lumberjack, dissect,
generic (non-Druid) skinning, mend, read magic. See cmd_move.c/
cmd_lumberjack.c/cmd_dissect.c/cmd_skin.c/cmd_repair.c/cmd_use.c.

    python3 tests/smoke_test_survival_generic.py [host] [port]
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
ROOM = 954000 + (int(time.time()) % 20000)
MTN_A = ROOM + 1
MTN_B = ROOM + 2


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


def set_skill(name, skill_name, pct):
    sql(f"INSERT INTO player_skill (player_id, skill_name, pct, last_gain_at) VALUES "
        f"((SELECT id FROM player WHERE name='{name}'), '{skill_name}', {pct}, {int(time.time())}) "
        f"ON DUPLICATE KEY UPDATE pct={pct};")


def set_level_class(name, level, cls):
    sql(f"UPDATE player_progress SET level={level}, basic_disc_pct=100, combat_disc_pct=100, "
        f"advanced_disc_pct=100, hp=500, max_hp=500 WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")
    sql(f"UPDATE player SET class={cls} WHERE name='{name}';")


announce("smoke_test_survival_generic", host, port)

# a plain sandbox + two TEMPERATE MOUNTAINS rooms (sector 22) linked E/W,
# and one TEMPERATE FOREST room (sector 23) for lumberjack.
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) VALUES "
    f"({ROOM},0,0,0,'Survival Sandbox','A forest clearing.\\n',NULL,1,23,0,0,0,0,0,0,0,0),"
    f"({MTN_A},0,0,0,'Mountain Ledge A','A rocky ledge.\\n',NULL,1,22,0,0,0,0,0,0,0,0),"
    f"({MTN_B},0,0,0,'Mountain Ledge B','Another rocky ledge.\\n',NULL,1,22,0,0,0,0,0,0,0,0);")
# east from A -> B, west from B -> A (direction 1=east, 3=west)
sql(f"INSERT INTO roomexit (vnum,direction,name,description,type,condition_flag,"
    f"lock_difficulty,weight,key_num,destination) VALUES "
    f"({MTN_A},1,'','',0,0,0,0,0,{MTN_B}),"
    f"({MTN_B},3,'','',0,0,0,0,0,{MTN_A});")

wn, pw = f"Svgen{_suffix}", "svgenpw12345"
sw = make_char(wn, pw, "2")   # Warrior (a non-Druid, non-Mage class)
cmd(sw, "quit!"); sw.close()
set_level_class(wn, 40, 2)
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{wn}';")
sw = relog(wn, pw)

# ---- 1: generic skinning -- a Warrior now passes the class gate ----
out = strip(cmd(sw, "skin"))
check("don't know how to skin" not in out.lower() and "corpse" in out.lower(),
      f"a non-Druid Warrior may now skin (past the class gate): {out!r}")

# ---- 2: dissect -- wired + class-gated open, reports no corpse ----
out = strip(cmd(sw, "dissect"))
check("don't know how to dissect" not in out.lower() and "corpse" in out.lower(),
      f"dissect is wired and open to a Warrior: {out!r}")

# ---- 3: mend -- wired, past the class gate (asks what to mend) ----
out = strip(cmd(sw, "mend"))
check("don't know how to mend" not in out.lower() and "mend what" in out.lower(),
      f"mend is wired and open to a Warrior: {out!r}")

# ---- 4: lumberjack -- end to end, fells a real log in a forest ----
set_skill(wn, "lumberjack", 100)   # deterministic success for the harvest
out = strip(cmd(sw, "lumberjack"))
check("fell a usable log" in out.lower() or "fells timber" in out.lower(),
      f"lumberjack fells a log in wooded terrain: {out!r}")
cmd(sw, "get log")
inv = strip(cmd(sw, "inventory"))
check("log" in inv.lower(), f"the felled log is a real, pickup-able object: {inv!r}")
check(skill_pct(wn, "lumberjack") >= 1, "lumberjack skill is trained/known")

# ---- 5: lumberjack refuses in non-wooded terrain ----
cmd(sw, "quit!"); sw.close()
sql(f"UPDATE player SET load_room={MTN_A} WHERE name='{wn}';")
sw = relog(wn, pw)
out = strip(cmd(sw, "lumberjack"))
check("nothing here worth cutting" in out.lower(),
      f"lumberjack refuses on bare mountain terrain: {out!r}")

# ---- 6: climbing -- trains from a move across vertical terrain ----
check(skill_pct(wn, "climbing") == 0, "climbing starts untrained")
cmd(sw, "east")     # MTN_A -> MTN_B, both mountain sectors
time.sleep(0.4)
recv_all(sw, 0.3)
check(skill_pct(wn, "climbing") >= 1,
      "climbing trains from a move across mountain terrain")

# ---- 7: help topics load ----
for topic in ("climbing", "dissect", "read magic", "lumberjack", "mend"):
    out = strip(cmd(sw, f"help {topic}", timeout=1.5))
    check("-- Help:" in out, f"help {topic} loads a real body: {out[:50]!r}")

cmd(sw, "quit!"); sw.close()
announce_done("smoke_test_survival_generic", host, port)
print("=== ALL CHECKS PASSED ===")
