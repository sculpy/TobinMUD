#!/usr/bin/env python3
"""Smoke test for `chi` (spell/skill functional-completeness audit,
2026-07-27 continued: Monk roster entry, level 1). See cmd_chi.c's own
header comment for the real-upstream research and scope-down rationale
(single-target chi-blast only; self/room/object variants cut, no mana
pool to hang them on).

    python3 tests/smoke_test_chi.py [host] [port]
"""
import re
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


announce("smoke_test_chi", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))

CLASS_MONK = 5  # matches skill.c's Monk roster / player.class encoding


def strip(s):
    return re.sub(r"\x1b\[[0-9;]*m", "", s)


def set_class(name, cls):
    sql(f"UPDATE player SET class={cls} WHERE name='{name}';")


def set_room(name, room):
    sql(f"UPDATE player SET load_room={room} WHERE name='{name}';")


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


def make_single(prefix, level=None, room=None, pk=True):
    name, pw = f"{prefix}{_suffix}", f"{prefix}pw12345"
    s0 = make_char(name, pw)
    cmd(s0, "quit!"); s0.close()
    set_class(name, CLASS_MONK)
    if level is not None:
        sql(f"UPDATE player_progress SET level={level} WHERE player_id="
            f"(SELECT id FROM player WHERE name='{name}');")
    if room is not None:
        set_room(name, room)
    sql(f"UPDATE player_progress SET basic_disc_pct=100 WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")
    s = relog(name, pw)
    if pk:
        cmd(s, "toggle pk")
    return name, s


ROOM = 973000 + (int(time.time()) % 10000)
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Chi Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")

sockets = []
try:
    # --- 1: 100%-proficiency chi succeeds, initiates a fight, deals damage ---
    nameA, sA = make_single("Chiw", level=10, room=ROOM)
    nameB, sB = make_single("Chiwo", level=10, room=ROOM)
    sockets += [sA, sB]
    seed_proficiency(nameA, "chi", 100)
    recv_all(sA); recv_all(sB)

    out = strip(cmd(sA, f"chi {nameB}"))
    check("unleash your chi" in out.lower(), "100%-proficiency chi succeeds and lands")
    out_b = strip(recv_all(sB, 0.4))
    check("chi force" in out_b.lower(), "the victim sees the real chi-attack message")

    # --- 2: no target, already fighting -- defaults to current opponent ---
    out2 = strip(cmd(sA, "chi"))
    check(nameB.lower() in out2.lower() or "unleash your chi" in out2.lower(),
          "chi with no target defaults to the current opponent while fighting")
    sA.close(); sB.close()

    # --- 3: 0%-proficiency chi always fails ---
    nameC, sC = make_single("Chiz", level=10, room=ROOM)
    nameD, sD = make_single("Chizo", level=10, room=ROOM)
    sockets += [sC, sD]
    seed_proficiency(nameC, "chi", 0)
    recv_all(sC); recv_all(sD)
    out3 = strip(cmd(sC, f"chi {nameD}"))
    check("fail to harm" in out3.lower(), "0%-proficiency chi always fails")

    # --- 4: chi-ing yourself is refused (combat_find_room_target()
    # excludes self from its search, same as attack/kill) --
    # check 3's own chi attempt left a 2-round being_set_wait() lag on
    # sC (unconditional, win or lose, same as bash/backstab) -- wait it
    # out first or the dispatcher rejects this command as "still
    # recovering" before target resolution even runs.
    time.sleep(2.6)
    recv_all(sC, 0.3); recv_all(sD, 0.3)
    out4 = strip(cmd(sC, "chi " + nameC))
    check("focus your chi on whom" in out4.lower(), "chi refuses a self-target")
    sC.close(); sD.close()

    # --- 5: no target, not fighting -- refused outright ---
    nameE, sE = make_single("Chie", level=10, room=ROOM)
    sockets += [sE]
    out5 = strip(cmd(sE, "chi"))
    check("focus your chi on whom" in out5.lower(),
          "chi with no target and no current fight is refused")
    sE.close()

    # --- 6: PK gate -- a non-opted-in mortal target is unreachable ---
    nameF, sF = make_single("Chif", level=10, room=ROOM, pk=True)
    nameG, sG = make_single("Chig", level=10, room=ROOM, pk=False)  # never toggled pk
    sockets += [sF, sG]
    seed_proficiency(nameF, "chi", 100)
    out6 = strip(cmd(sF, f"chi {nameG}"))
    check("focus your chi on whom" in out6.lower(),
          "chi can't target a mortal who hasn't opted into PK")
    sF.close(); sG.close()

    sockets = []
    announce_done("smoke_test_chi", host, port)
    print("=== ALL CHECKS PASSED ===")
finally:
    for sock in sockets:
        try:
            sock.close()
        except OSError:
            pass
    sql(f"DELETE FROM player_progress WHERE player_id IN (SELECT id FROM player WHERE name LIKE 'Chi%{_suffix}');")
    sql(f"DELETE FROM player_attrs WHERE player_id IN (SELECT id FROM player WHERE name LIKE 'Chi%{_suffix}');")
    sql(f"DELETE FROM player_skill WHERE player_id IN (SELECT id FROM player WHERE name LIKE 'Chi%{_suffix}');")
    sql(f"DELETE FROM player WHERE name LIKE 'Chi%{_suffix}';")
    sql(f"DELETE FROM room WHERE vnum={ROOM};")
