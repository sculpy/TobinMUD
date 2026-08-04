#!/usr/bin/env python3
"""Smoke test for `shove` (spell/skill functional-completeness audit
continued, Warrior level 6). See cmd_shove.c's own header comment for
the real-upstream research (disc_dueling.cc's doShove()/shove()/
throwChar()) and scope-down rationale.

    python3 tests/smoke_test_shove.py [host] [port]
"""
import re
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


announce("smoke_test_shove", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))

CLASS_WARRIOR = 2


def strip(s):
    return re.sub(r"\x1b\[[0-9;]*m", "", s)


def set_dex(name, dex):
    sql(f"UPDATE player_attrs SET dexterity={dex} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


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


ROOM_A = 978000 + (int(time.time()) % 1000)
ROOM_B = ROOM_A + 1
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) VALUES "
    f"({ROOM_A},0,0,0,'Shove Sandbox A','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0),"
    f"({ROOM_B},1,0,0,'Shove Sandbox B','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO roomexit (vnum,direction,name,description,type,condition_flag,"
    f"lock_difficulty,weight,key_num,destination) VALUES "
    f"({ROOM_A},0,'','',0,0,0,0,0,{ROOM_B}),"   # north
    f"({ROOM_B},2,'','',0,0,0,0,0,{ROOM_A});")  # south -- return path

sockets = []
try:
    # --- 1: no target/direction is refused ---
    nameA, sA = make_single("Shvw", room=ROOM_A)
    sockets.append(sA)
    seed_proficiency(nameA, "shove", 100)
    recv_all(sA)
    out1 = strip(cmd(sA, "shove"))
    check("whom" in out1.lower(), "shove with no target/direction is refused")

    # --- 2: 100%-proficiency shove (favorable dex+level) pushes the
    # target into the adjacent room ---
    nameB, sB = make_single("Shvwo", room=ROOM_A)
    sockets.append(sB)
    set_dex(nameA, 200); set_dex(nameB, 70)
    recv_all(sA); recv_all(sB)
    out2 = strip(cmd(sA, f"shove {nameB} north"))
    check("push" in out2.lower(), "a favorable shove succeeds and pushes the target")
    time.sleep(0.3)
    look_b = strip(cmd(sB, "look"))
    check("Shove Sandbox B" in look_b, "the shoved target actually lands in the adjacent room")
    cmd(sB, "south"); recv_all(sB, 0.3)  # walk back for later checks

    # --- 3: 0%-proficiency shove with an unfavorable matchup starts a fight instead ---
    nameC, sC = make_single("Shvz", room=ROOM_A)
    nameD, sD = make_single("Shvzo", room=ROOM_A)
    sockets += [sC, sD]
    set_dex(nameC, 70); set_dex(nameD, 200)
    recv_all(sC); recv_all(sD)
    out3 = strip(cmd(sC, f"shove {nameD} north"))
    check("no avail" in out3.lower(), "an unfavorable shove fails")
    # A failed shove baits a fight (real upstream design, ported as-is --
    # see cmd_shove.c), which lags the shover via being_set_wait() same
    # as any other combat action -- wait it out before the next command
    # or "look" gets rejected with "still recovering" instead of run.
    time.sleep(1.3)
    out3b = strip(cmd(sC, "look"))
    check("Shove Sandbox A" in out3b, "a failed shove does NOT relocate anyone")

    # --- 4: shove refuses while fighting ---
    cmd(sC, f"attack {nameD}")
    time.sleep(1.3)
    out4 = strip(cmd(sC, f"shove {nameD} north"))
    check("not while fighting" in out4.lower(), "shove refuses while the shover is fighting")

    sockets = []
    for sock in (sA, sB, sC, sD):
        try:
            sock.close()
        except OSError:
            pass

    announce_done("smoke_test_shove", host, port)
    print("=== ALL CHECKS PASSED ===")
finally:
    for sock in sockets:
        try:
            sock.close()
        except OSError:
            pass
    for prefix in ("Shvw", "Shvz"):
        sql(f"DELETE FROM player_progress WHERE player_id IN (SELECT id FROM player WHERE name LIKE '{prefix}%{_suffix}');")
        sql(f"DELETE FROM player_attrs WHERE player_id IN (SELECT id FROM player WHERE name LIKE '{prefix}%{_suffix}');")
        sql(f"DELETE FROM player_skill WHERE player_id IN (SELECT id FROM player WHERE name LIKE '{prefix}%{_suffix}');")
        sql(f"DELETE FROM player WHERE name LIKE '{prefix}%{_suffix}';")
    sql(f"DELETE FROM roomexit WHERE vnum IN ({ROOM_A}, {ROOM_B});")
    sql(f"DELETE FROM room WHERE vnum IN ({ROOM_A}, {ROOM_B});")
