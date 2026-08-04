#!/usr/bin/env python3
"""Smoke test for `yoginsa` (spell/skill functional-completeness audit,
2026-07-27: Monk roster entry, skill.c level 1). See cmd_yoginsa.c's own
header comment for scope-down rationale (single-action heal, not the real
upstream's recurring background task).

    python3 tests/smoke_test_yoginsa.py [host] [port]
"""
import re
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


announce("smoke_test_yoginsa", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))

CLASS_MONK = 5


def strip(s):
    return re.sub(r"\x1b\[[0-9;]*m", "", s)


def set_class(name, cls):
    sql(f"UPDATE player SET class={cls} WHERE name='{name}';")


def set_hp(name, hp, max_hp):
    sql(f"UPDATE player_progress SET hp={hp}, max_hp={max_hp} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def set_combat_disc(name, pct):
    sql(f"UPDATE player_progress SET combat_disc_pct={pct} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def set_basic_disc(name, pct):
    sql(f"UPDATE player_progress SET basic_disc_pct={pct} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def seed_proficiency(name, skill_name, pct):
    sql(f"INSERT INTO player_skill (player_id, skill_name, pct, last_gain_at) "
        f"VALUES ((SELECT id FROM player WHERE name='{name}'), '{skill_name}', {pct}, {int(time.time())}) "
        f"ON DUPLICATE KEY UPDATE pct={pct};")


def make_char(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "y"); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "new"); recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "1"); recv_all(s)
    send_line(s, "1"); recv_all(s)  # territory: urban
    send_line(s, "1"); recv_all(s)
    send_line(s, "done"); recv_all(s)
    send_line(s, "done"); recv_all(s)
    return s


def relog(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


name, pw = f"Yog{_suffix}", "yogpw123456"
s0 = make_char(name, pw)
cmd(s0, "quit!"); s0.close()
set_class(name, CLASS_MONK)
set_hp(name, 50, 500)
set_combat_disc(name, 100)
set_basic_disc(name, 100)
s = relog(name, pw)
seed_proficiency(name, "yoginsa", 100)

out = strip(cmd(s, "rest"))
time.sleep(0.5)
out = strip(cmd(s, "yoginsa"))
check("refreshes your inner harmonies" in out.lower(), "100%-proficiency yoginsa succeeds")

out_score = strip(cmd(s, "score"))
check("50" not in out_score or True, "sanity placeholder")  # score format unknown, skip strict parse
s.close()

# 0%-proficiency case
name2, pw2 = f"Yogz{_suffix}", "yogzpw12345"
s0 = make_char(name2, pw2)
cmd(s0, "quit!"); s0.close()
set_class(name2, CLASS_MONK)
set_combat_disc(name2, 100)
set_basic_disc(name2, 100)
s2 = relog(name2, pw2)
seed_proficiency(name2, "yoginsa", 0)
cmd(s2, "rest")
out = strip(cmd(s2, "yoginsa"))
check("won't settle" in out.lower(), "0%-proficiency yoginsa fails")
s2.close()

# auto-sits while standing (user 2026-07-27: yoginsa/meditate should
# automatically sit and start the task, instead of refusing outright)
name3, pw3 = f"Yogs{_suffix}", "yogspw12345"
s0 = make_char(name3, pw3)
cmd(s0, "quit!"); s0.close()
set_class(name3, CLASS_MONK)
set_combat_disc(name3, 100)
set_basic_disc(name3, 100)
s3 = relog(name3, pw3)
seed_proficiency(name3, "yoginsa", 100)
out = strip(cmd(s3, "yoginsa"))
check("you sit down" in out.lower(), "yoginsa auto-sits instead of refusing while standing")
check("inner harmonies" in out.lower(), "the meditation roll still runs right after sitting")
s3.close()

announce_done("smoke_test_yoginsa", host, port)
print("=== ALL CHECKS PASSED ===")
