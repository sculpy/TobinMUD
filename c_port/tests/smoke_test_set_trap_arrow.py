#!/usr/bin/env python3
"""Smoke test for `set trap (arrow)` (TODO.md's "Common skills" backlog
item, closed 2026-08-22 now that cmd_shoot.c's ammo subsystem exists to
hang it on). See cmd_trap.c's own doc comment: `settrap arrow [item]`
rigs a carried loose arrow (obj.h's ARROW_TRAPPED, val[0]); cmd_shoot.c
springs it on a landed hit -- same flat random-limb damage as the door
trap, single-use, then the arrow is destroyed as ammo normally is
either way. `disarmtrap arrow` removes the rig without springing it.

Proves: settrap arrow rigs an arrow only once "set trap (arrow)" is
known; a trapped arrow shot at a mob springs the extra hit on top of
the normal shot damage; disarmtrap arrow safely clears the rig with no
spring; and both help topics load real bodies.

    python3 tests/smoke_test_set_trap_arrow.py [host] [port]
"""
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 977600 + (int(time.time()) % 1000)
BOW, ARROW = 170, 166


def query(stmt):
    return subprocess.run(["mariadb", "-N", "tobin", "-e", stmt],
                          check=True, capture_output=True, text=True).stdout.strip()


def make_char(name, pw, cls):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw, "new", name, "1", "1", cls, "done", "done"):
        send_line(s, step)
        recv_all(s)
    cmd(s, "quit!")
    s.close()


def relog(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


announce("smoke_test_set_trap_arrow", host, port)

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Trap Arrow Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")

imm_name, imm_pw = f"Taimm{_suffix}", "taimmpw12345"
make_char(imm_name, imm_pw, "1")
sql(f"UPDATE player_progress SET level=60 WHERE player_id=(SELECT id FROM player WHERE name='{imm_name}');")
si = relog(imm_name, imm_pw)
cmd(si, f"goto {ROOM}")

thf_name, thf_pw = f"Tathf{_suffix}", "tathfpw12345"
make_char(thf_name, thf_pw, "4")  # Thief
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{thf_name}';")
sql(f"UPDATE player_progress SET level=40, basic_disc_pct=100, combat_disc_pct=100, "
    f"advanced_disc_pct=100, hp=5000, max_hp=5000 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{thf_name}');")
sql(f"INSERT INTO player_skill (player_id, skill_name, pct) VALUES "
    f"((SELECT id FROM player WHERE name='{thf_name}'), 'set trap (arrow)', 100) "
    f"ON DUPLICATE KEY UPDATE pct=100;")
sql(f"INSERT INTO player_skill (player_id, skill_name, pct) VALUES "
    f"((SELECT id FROM player WHERE name='{thf_name}'), 'disarm trap', 100) "
    f"ON DUPLICATE KEY UPDATE pct=100;")
st = relog(thf_name, thf_pw)

sockets = [si, st]
try:
    cmd(si, f"load obj {BOW}")
    cmd(si, f"give bow {thf_name}")
    cmd(si, f"load obj {ARROW}")
    cmd(si, f"give arrow {thf_name}")
    cmd(si, f"load obj {ARROW}")
    cmd(si, f"give arrow {thf_name}")

    out = cmd(st, "settrap arrow")
    check("rig a trap into" in out, "settrap arrow rigs a carried arrow")

    out = cmd(st, "settrap arrow")
    check("already rigged" in out, "a second settrap arrow refuses -- one is already rigged")

    cmd(st, "wield bow")
    cmd(si, "load mob 1735")  # real seeded tank mob, same one smoke_test_shoot.py uses
    recv_all(st, 0.3)

    out = cmd(st, "shoot testmob", 1.5)
    check("You loose" in out, "the trapped shot actually lands")
    check("trap rigged into your arrow springs" in out, "the rig springs on a landed hit")

    # Fresh mob + `purge` before each further shot -- sidesteps any
    # ambiguity from ongoing melee-round damage on the still-fighting
    # first mob during the reload wait (not this test's concern).
    cmd(si, "purge")
    cmd(si, "load mob 1735")
    recv_all(st, 4.0)  # clear reload lag (RANGED_RELOAD_PULSES = 3*12 = 3.6s)
    out = cmd(st, "shoot testmob", 1.5)
    check("You loose" in out, "the plain second shot lands")
    check("trap rigged" not in out, "an untrapped arrow never springs")

    # --- disarm, no spring ---
    cmd(si, "purge")
    cmd(si, "load mob 1735")
    cmd(si, f"load obj {ARROW}")
    cmd(si, f"give arrow {thf_name}")
    cmd(st, "settrap arrow")
    out = cmd(st, "disarmtrap arrow")
    check("carefully disarm the trap" in out, "disarmtrap arrow clears the rig")
    recv_all(st, 4.0)
    out = cmd(st, "shoot testmob", 1.5)
    check("You loose" in out, "the disarmed shot still lands")
    check("trap rigged" not in out, "a disarmed arrow never springs")

    # --- help ---
    check("arrow" in cmd(si, "help settrap").lower(), "help settrap mentions the arrow form")
    check("arrow" in cmd(si, "help disarmtrap").lower(), "help disarmtrap mentions the arrow form")
    check("arrow" in cmd(si, "help set trap (arrow)").lower(), "help set trap (arrow) exists")

    announce_done("smoke_test_set_trap_arrow", host, port)
    print("=== ALL CHECKS PASSED ===")
finally:
    for sock in sockets:
        try:
            sock.close()
        except OSError:
            pass
    sql(f"DELETE FROM player_progress WHERE player_id IN (SELECT id FROM player WHERE name LIKE 'Ta%{_suffix}');")
    sql(f"DELETE FROM player_attrs WHERE player_id IN (SELECT id FROM player WHERE name LIKE 'Ta%{_suffix}');")
    sql(f"DELETE FROM player_skill WHERE player_id IN (SELECT id FROM player WHERE name LIKE 'Ta%{_suffix}');")
    sql(f"DELETE FROM player WHERE name LIKE 'Ta%{_suffix}';")
    sql(f"DELETE FROM room WHERE vnum={ROOM};")
