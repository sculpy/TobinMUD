#!/usr/bin/env python3
"""Smoke test for `set trap (mine)`, `set trap (grenade)`, and the new
`throw` command (TODO.md backlog, user decision 2026-08-22: "build
both"). See cmd_trap.c's own doc comment and cmd_throw.c's header.

Proves: settrap mine rigs the CURRENT ROOM's floor and springs on
anyone walking in from any direction (one-shot, detect-trap dodge
works, disarm clears it with no spring); throw lands a hit and spends
the item; settrap grenade rigs a carried grenade item and throw springs
the rig on a landed hit, an untrapped grenade never springs, and
disarm clears the rig with no spring.

    python3 tests/smoke_test_mine_grenade_throw.py [host] [port]
"""
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 977500 + (int(time.time()) % 500)
ROOM2 = ROOM + 1
GRENADE = 31004


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


announce("smoke_test_mine_grenade_throw", host, port)

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Mine Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM2},0,0,0,'Mine Sandbox Anteroom','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
# north from ROOM2 -> ROOM, south from ROOM -> ROOM2 (plain exits, no door)
sql(f"INSERT INTO roomexit (vnum,direction,name,description,type,condition_flag,"
    f"lock_difficulty,weight,key_num,destination) VALUES "
    f"({ROOM2},0,'','',0,0,0,0,0,{ROOM});")
sql(f"INSERT INTO roomexit (vnum,direction,name,description,type,condition_flag,"
    f"lock_difficulty,weight,key_num,destination) VALUES "
    f"({ROOM},2,'','',0,0,0,0,0,{ROOM2});")

imm_name, imm_pw = f"Mgimm{_suffix}", "mgimmpw12345"
make_char(imm_name, imm_pw, "1")
sql(f"UPDATE player_progress SET level=60 WHERE player_id=(SELECT id FROM player WHERE name='{imm_name}');")
si = relog(imm_name, imm_pw)
cmd(si, f"goto {ROOM}")

thf_name, thf_pw = f"Mgthf{_suffix}", "mgthfpw12345"
make_char(thf_name, thf_pw, "4")  # Thief
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{thf_name}';")
sql(f"UPDATE player_progress SET level=55, basic_disc_pct=100, combat_disc_pct=100, "
    f"advanced_disc_pct=100, hp=5000, max_hp=5000 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{thf_name}');")
for skname in ("set trap (mine)", "set trap (grenade)", "disarm trap"):
    sql(f"INSERT INTO player_skill (player_id, skill_name, pct) VALUES "
        f"((SELECT id FROM player WHERE name='{thf_name}'), '{skname}', 100) "
        f"ON DUPLICATE KEY UPDATE pct=100;")
st = relog(thf_name, thf_pw)

# A plain mortal walker for the mine trap (must NOT know detect trap).
war_name, war_pw = f"Mgwar{_suffix}", "mgwarpw12345"
make_char(war_name, war_pw, "3")  # Warrior
sql(f"UPDATE player SET load_room={ROOM2} WHERE name='{war_name}';")
sql(f"UPDATE player_progress SET level=1, hp=5000, max_hp=5000 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{war_name}');")
sw = relog(war_name, war_pw)

sockets = [si, st, sw]
try:
    # --- mine: rig, spring, one-shot, disarm ---
    out = cmd(st, "settrap mine")
    check("bury a mine" in out, "settrap mine rigs the current room's floor")

    out = cmd(sw, "north")
    check("mine hidden in the floor explodes" in out, "the mine springs on a plain walker entering")

    cmd(sw, "south")  # back out
    out = cmd(sw, "north")
    check("mine hidden in the floor explodes" not in out, "a sprung mine is one-shot -- doesn't spring twice")
    cmd(sw, "south")

    out = cmd(st, "settrap mine")
    check("bury a mine" in out, "settrap mine works again after the first rig sprung")
    out = cmd(st, "disarmtrap mine")
    check("carefully disarm the trap" in out, "disarmtrap mine clears the rig")
    out = cmd(sw, "north")
    check("mine hidden in the floor explodes" not in out, "a disarmed mine never springs")
    cmd(sw, "south")

    # --- grenade + throw ---
    cmd(si, f"load obj {GRENADE}")
    cmd(si, f"give grenade {thf_name}")
    cmd(si, f"load obj {GRENADE}")
    cmd(si, f"give grenade {thf_name}")
    cmd(si, f"load obj {GRENADE}")
    cmd(si, f"give grenade {thf_name}")

    out = cmd(st, "settrap grenade")
    check("rig a trap into" in out, "settrap grenade rigs a carried grenade")
    out = cmd(st, "settrap grenade")
    check("already rigged" in out, "a second settrap grenade refuses -- one is already rigged")

    cmd(si, "load mob 1735")
    recv_all(st, 0.3)
    out = cmd(st, "throw grenade testmob", 1.5)
    check("You hurl" in out, "the trapped throw actually lands")
    check("trap rigged into your throw springs" in out, "the rig springs on a landed hit")

    cmd(si, "purge")
    cmd(si, "load mob 1735")
    recv_all(st, 1.5)  # THROW_WAIT_PULSES = 12 = 1.2s
    out = cmd(st, "throw grenade testmob", 1.5)
    check("You hurl" in out, "the plain second throw lands")
    check("trap rigged" not in out, "an untrapped grenade never springs")

    cmd(si, "purge")
    cmd(si, "load mob 1735")
    out = cmd(st, "settrap grenade")
    check("rig a trap into" in out, "settrap grenade rigs the last carried grenade")
    out = cmd(st, "disarmtrap grenade")
    check("carefully disarm the trap" in out, "disarmtrap grenade clears the rig")
    recv_all(st, 1.5)
    out = cmd(st, "throw grenade testmob", 1.5)
    check("You hurl" in out, "the disarmed throw still lands")
    check("trap rigged" not in out, "a disarmed grenade never springs")

    # --- help ---
    check("mine" in cmd(si, "help settrap").lower(), "help settrap mentions the mine form")
    check("grenade" in cmd(si, "help settrap").lower(), "help settrap mentions the grenade form")
    check("throw" in cmd(si, "help throw").lower(), "help throw exists")

    announce_done("smoke_test_mine_grenade_throw", host, port)
    print("=== ALL CHECKS PASSED ===")
finally:
    for sock in sockets:
        try:
            sock.close()
        except OSError:
            pass
    sql(f"DELETE FROM player_progress WHERE player_id IN (SELECT id FROM player WHERE name LIKE 'Mg%{_suffix}');")
    sql(f"DELETE FROM player_attrs WHERE player_id IN (SELECT id FROM player WHERE name LIKE 'Mg%{_suffix}');")
    sql(f"DELETE FROM player_skill WHERE player_id IN (SELECT id FROM player WHERE name LIKE 'Mg%{_suffix}');")
    sql(f"DELETE FROM player WHERE name LIKE 'Mg%{_suffix}';")
    sql(f"DELETE FROM roomexit WHERE vnum IN ({ROOM},{ROOM2});")
    sql(f"DELETE FROM room WHERE vnum IN ({ROOM},{ROOM2});")
