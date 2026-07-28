#!/usr/bin/env python3
"""Smoke test for `spin` (spell/skill functional-completeness audit
continued, Warrior level 17). See cmd_spin.c's own header comment for the
real-upstream research and scope-down rationale (one skill_roll_success()
roll, no countermove/focused-avoidance defense rolls, "needs a free hand"
per Tobin's own roster flavor text).

Not live-tested here: the flying-target refusal (`canSpin()`'s "You can
only spin fliers that are fighting you.") -- Tobin's AFFECT_FLYING isn't a
DB-persisted row, only reachable live via a Mage casting `levitate`
(level 11) on the target first, which would add a whole extra character/
class just for one edge case. Verified by code review instead (cmd_spin.c
checks `being_has_affect(target, AFFECT_FLYING) && target->fighting != ch`
before the position check), matching the fear/slumber precedent of not
asserting every real-world side effect live.

    python3 tests/smoke_test_spin.py [host] [port]
"""
import re
import socket
import subprocess
import sys
import time

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


def announce(test_name, host=host, port=port):
    try:
        s = socket.create_connection((host, port), timeout=3)
        s.settimeout(0.5)
        try:
            while s.recv(4096):
                pass
        except socket.timeout:
            pass
        s.sendall(f"@test {test_name}\r\n".encode())
        s.settimeout(0.5)
        try:
            while s.recv(4096):
                pass
        except socket.timeout:
            pass
        s.close()
    except OSError:
        pass


def announce_done(test_name, host=host, port=port):
    announce(f"done {test_name}", host, port)


announce("smoke_test_spin")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))

CLASS_WARRIOR = 2
TYPE_WEAPON = 5
WEAR_TAKE = 1
WEAR_HOLD = 16384


def recv_all(sock, timeout=1.0):
    sock.settimeout(timeout)
    chunks = []
    try:
        while True:
            data = sock.recv(4096)
            if not data:
                break
            chunks.append(data)
    except socket.timeout:
        pass
    return b"".join(chunks).decode(errors="replace")


def send_line(sock, line):
    sock.sendall((line + "\r\n").encode())


def cmd(sock, line, timeout=1.0):
    send_line(sock, line)
    return recv_all(sock, timeout)


def check(condition, message):
    if not condition:
        raise AssertionError(message)
    print(f">>> OK: {message}")


def strip(s):
    return re.sub(r"\x1b\[[0-9;]*m", "", s)


def sql(stmt):
    subprocess.run(["mariadb", "tobin", "-e", stmt], check=True)


def seed_proficiency(name, skill_name, pct):
    sql(f"INSERT INTO player_skill (player_id, skill_name, pct, last_gain_at) "
        f"VALUES ((SELECT id FROM player WHERE name='{name}'), '{skill_name}', {pct}, {int(time.time())}) "
        f"ON DUPLICATE KEY UPDATE pct={pct};")


def make_char(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw, "new", name, "1", "1", "done", "done"):
        send_line(s, step)
        recv_all(s)
    s.close()


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
    make_char(name, pw)
    sql(f"UPDATE player SET class={CLASS_WARRIOR} WHERE name='{name}';")
    sql(f"UPDATE player_progress SET level=20, basic_disc_pct=100, "
        f"hp=5000, max_hp=5000, vit=5000, max_vit=5000 WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")
    if room is not None:
        sql(f"UPDATE player SET load_room={room} WHERE name='{name}';")
    s = relog(name, pw)
    cmd(s, "toggle pk")
    return name, s


ROOM = 976500 + (int(time.time()) % 1000)
SWORD = ROOM + 1
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Spin Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({SWORD},'sword testsword','a test sword','A test sword is lying here.',"
    f"{TYPE_WEAPON},{WEAR_TAKE | WEAR_HOLD},1);")

sockets = []
try:
    # --- 1: no target is refused ---
    nameA, sA = make_single("Spnw", room=ROOM)
    sockets.append(sA)
    seed_proficiency(nameA, "spin", 100)
    recv_all(sA)
    out1 = strip(cmd(sA, "spin"))
    check("spin whom" in out1.lower(), "spin with no target is refused")

    # --- 2: 100%-proficiency spin succeeds and deals damage ---
    nameB, sB = make_single("Spnwo", room=ROOM)
    sockets.append(sB)
    recv_all(sB)
    out2 = strip(cmd(sA, f"spin {nameB}"))
    check("spinning" in out2.lower() and "ground" in out2.lower(), "100%-proficiency spin succeeds")

    # --- 3: 0%-proficiency spin always misses ---
    nameC, sC = make_single("Spnz", room=ROOM)
    nameD, sD = make_single("Spnzo", room=ROOM)
    sockets += [sC, sD]
    seed_proficiency(nameC, "spin", 0)
    recv_all(sC); recv_all(sD)
    out3 = strip(cmd(sC, f"spin {nameD}"))
    check("lose your footing" in out3.lower(), "0%-proficiency spin always misses")

    # --- 4: spin refuses a self-target (combat_find_room_target() excludes
    # self, same precedent as bodyslam/headbutt) -- wait out check 3's own
    # wait first. ---
    time.sleep(2.6)
    recv_all(sC, 0.3)
    out4 = strip(cmd(sC, f"spin {nameC}"))
    check("aren't here" in out4.lower(), "spin can't target yourself")

    # --- 5: spin refuses with something held in the primary hand ---
    nameE, sE = make_single("Spne", room=ROOM)
    nameF, sF = make_single("Spnf", room=ROOM)
    sockets += [sE, sF]
    seed_proficiency(nameE, "spin", 100)
    recv_all(sE); recv_all(sF)

    # Spawn a live instance of the sword prototype into the sandbox room
    # via a throwaway immortal helper (same pattern smoke_test_weapon_
    # messaging.py uses) -- the earlier `INSERT INTO obj` only created the
    # prototype row, not a room instance `get` can pick up.
    immName, immPw = f"Spnimm{_suffix}", "spnimmpw123"
    make_char(immName, immPw)
    sql(f"UPDATE player SET load_room={ROOM} WHERE name='{immName}';")
    sql(f"UPDATE player_progress SET level=51 WHERE player_id="
        f"(SELECT id FROM player WHERE name='{immName}');")
    sImm = relog(immName, immPw)
    check("You conjure" in cmd(sImm, f"load obj {SWORD}"), "the test sword is loaded")
    cmd(sImm, "drop sword")
    cmd(sImm, "quit!")
    sImm.close()

    out5a = strip(cmd(sE, "get sword"))
    check("you get" in out5a.lower(), "the spinner picks up the test sword")
    out5b = strip(cmd(sE, "wield sword"))
    check("wield" in out5b.lower(), "wield equips the test sword")
    out5c = strip(cmd(sE, f"spin {nameF}"))
    check("free hand" in out5c.lower(), "spin refuses while a weapon is held in the primary hand")

    announce_done("smoke_test_spin")
    print("=== ALL CHECKS PASSED ===")
finally:
    for sock in sockets:
        try:
            sock.close()
        except OSError:
            pass
    for prefix in ("Spnw", "Spnz", "Spne", "Spnf", "Spnimm"):
        sql(f"DELETE FROM player_progress WHERE player_id IN (SELECT id FROM player WHERE name LIKE '{prefix}%{_suffix}');")
        sql(f"DELETE FROM player_attrs WHERE player_id IN (SELECT id FROM player WHERE name LIKE '{prefix}%{_suffix}');")
        sql(f"DELETE FROM player_skill WHERE player_id IN (SELECT id FROM player WHERE name LIKE '{prefix}%{_suffix}');")
        sql(f"DELETE FROM player_inventory WHERE player_id IN (SELECT id FROM player WHERE name LIKE '{prefix}%{_suffix}');")
        sql(f"DELETE FROM player WHERE name LIKE '{prefix}%{_suffix}';")
    sql(f"DELETE FROM room WHERE vnum={ROOM};")
    sql(f"DELETE FROM obj WHERE vnum={SWORD};")
