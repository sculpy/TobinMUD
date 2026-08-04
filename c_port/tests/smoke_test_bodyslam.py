#!/usr/bin/env python3
"""Smoke test for `bodyslam` (spell/skill functional-completeness
audit continued, Warrior level 10). See cmd_bodyslam.c's own header
comment for the real-upstream research and scope-down rationale.

    python3 tests/smoke_test_bodyslam.py [host] [port]
"""
import re
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


announce("smoke_test_bodyslam", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))

CLASS_WARRIOR = 2


def strip(s):
    return re.sub(r"\x1b\[[0-9;]*m", "", s)


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


def make_single(prefix, room=None):
    name, pw = f"{prefix}{_suffix}", f"{prefix}pw12345"
    s0 = make_char(name, pw)
    cmd(s0, "quit!"); s0.close()
    sql(f"UPDATE player SET class={CLASS_WARRIOR} WHERE name='{name}';")
    sql(f"UPDATE player_progress SET level=10, basic_disc_pct=100, combat_disc_pct=100, "
        f"hp=5000, max_hp=5000, vit=5000, max_vit=5000 WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")
    if room is not None:
        sql(f"UPDATE player SET load_room={room} WHERE name='{name}';")
    s = relog(name, pw)
    cmd(s, "toggle pk")
    return name, s


def seed_proficiency(name, skill_name, pct):
    sql(f"INSERT INTO player_skill (player_id, skill_name, pct, last_gain_at) "
        f"VALUES ((SELECT id FROM player WHERE name='{name}'), '{skill_name}', {pct}, {int(time.time())}) "
        f"ON DUPLICATE KEY UPDATE pct={pct};")


ROOM = 979900 + (int(time.time()) % 100)
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Bodyslam Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")

sockets = []
try:
    # --- 1: no target ---
    nameA, sA = make_single("Bsw", room=ROOM)
    sockets.append(sA)
    seed_proficiency(nameA, "bodyslam", 100)
    recv_all(sA)
    out1 = strip(cmd(sA, "bodyslam"))
    check("whom" in out1.lower(), "bodyslam with no target is refused")

    # --- 2: 100%-proficiency bodyslam succeeds, knocks the target down, deals damage ---
    nameB, sB = make_single("Bswo", room=ROOM)
    sockets.append(sB)
    recv_all(sB)
    out2 = strip(cmd(sA, f"bodyslam {nameB}"))
    check("lift" in out2.lower() and "slam" in out2.lower(), "100%-proficiency bodyslam succeeds")

    # --- 3: knocked-down target can't be bodyslammed again -- bodyslam's
    # own 2-round being_set_wait() lag on the attacker must clear first
    # or the dispatcher rejects the next attempt as "still recovering"
    # before target-state is even checked (same trap this session's own
    # smoke_test_chi.py hit). ---
    time.sleep(2.6)
    recv_all(sA, 0.3)
    out3 = strip(cmd(sA, f"bodyslam {nameB}"))
    check("already down" in out3.lower(), "an already-down target can't be bodyslammed again")
    sB.close()

    # --- 4: 0%-proficiency bodyslam fails, attacker ends up sitting too ---
    # sA was seeded to 100% back in check 1/2 and stays that way for its
    # whole session -- a fresh, never-seeded attacker (0% by default) is
    # needed to actually exercise the failure path.
    nameD, sD = make_single("Bszatk", room=ROOM)
    nameC, sC = make_single("Bsz", room=ROOM)
    sockets += [sD, sC]
    recv_all(sD); recv_all(sC)
    out4 = strip(cmd(sD, f"bodyslam {nameC}"))
    check("falling on your face" in out4.lower(), "0%-proficiency bodyslam always fails")
    sD.close(); sC.close()

    # --- 5: refuses a self-target (combat_find_room_target() excludes
    # self from its search, same as attack/kill/chi) ---
    out5 = strip(cmd(sA, f"bodyslam {nameA}"))
    check("aren't here" in out5.lower(),
          "bodyslam can't target yourself (combat_find_room_target excludes self)")
    sA.close()

    sockets = []
    announce_done("smoke_test_bodyslam", host, port)
    print("=== ALL CHECKS PASSED ===")
finally:
    for sock in sockets:
        try:
            sock.close()
        except OSError:
            pass
    for prefix in ("Bsw", "Bsz"):
        sql(f"DELETE FROM player_progress WHERE player_id IN (SELECT id FROM player WHERE name LIKE '{prefix}%{_suffix}');")
        sql(f"DELETE FROM player_attrs WHERE player_id IN (SELECT id FROM player WHERE name LIKE '{prefix}%{_suffix}');")
        sql(f"DELETE FROM player_skill WHERE player_id IN (SELECT id FROM player WHERE name LIKE '{prefix}%{_suffix}');")
        sql(f"DELETE FROM player WHERE name LIKE '{prefix}%{_suffix}';")
    sql(f"DELETE FROM room WHERE vnum={ROOM};")
