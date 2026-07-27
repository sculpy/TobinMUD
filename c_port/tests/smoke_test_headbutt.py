#!/usr/bin/env python3
"""Smoke test for `headbutt` (spell/skill functional-completeness audit
continued, Warrior level 15). See cmd_headbutt.c's own header comment
for the real-upstream research and scope-down rationale (no height
mechanic -- Tobin has no height stat -- one skill_roll_success() roll
striking LIMB_HEAD).

    python3 tests/smoke_test_headbutt.py [host] [port]
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


announce("smoke_test_headbutt")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))

CLASS_WARRIOR = 2


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


ROOM = 975500 + (int(time.time()) % 1000)
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Headbutt Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")

sockets = []
try:
    # --- 1: no target is refused ---
    nameA, sA = make_single("Hbtw", room=ROOM)
    sockets.append(sA)
    seed_proficiency(nameA, "headbutt", 100)
    recv_all(sA)
    out1 = strip(cmd(sA, "headbutt"))
    check("whose head" in out1.lower(), "headbutt with no target is refused")

    # --- 2: 100%-proficiency headbutt succeeds and deals damage ---
    nameB, sB = make_single("Hbtwo", room=ROOM)
    sockets.append(sB)
    recv_all(sB)
    out2 = strip(cmd(sA, f"headbutt {nameB}"))
    check("headbutt" in out2.lower() and "skull" in out2.lower(), "100%-proficiency headbutt succeeds")

    # --- 3: 0%-proficiency headbutt always misses ---
    nameC, sC = make_single("Hbtz", room=ROOM)
    nameD, sD = make_single("Hbtzo", room=ROOM)
    sockets += [sC, sD]
    seed_proficiency(nameC, "headbutt", 0)
    recv_all(sC); recv_all(sD)
    out3 = strip(cmd(sC, f"headbutt {nameD}"))
    check("moves their head out of the way" in out3.lower(), "0%-proficiency headbutt always misses")

    # --- 4: headbutt refuses a self-target (combat_find_room_target()
    # excludes self, same as attack/kill) -- wait out the wait from
    # check 3's own attempt first, same trap this audit keeps hitting. ---
    time.sleep(1.3)
    recv_all(sC, 0.3)
    out4 = strip(cmd(sC, f"headbutt {nameC}"))
    check("aren't here" in out4.lower(), "headbutt can't target yourself")

    announce_done("smoke_test_headbutt")
    print("=== ALL CHECKS PASSED ===")
finally:
    for sock in sockets:
        try:
            sock.close()
        except OSError:
            pass
    for prefix in ("Hbtw", "Hbtz"):
        sql(f"DELETE FROM player_progress WHERE player_id IN (SELECT id FROM player WHERE name LIKE '{prefix}%{_suffix}');")
        sql(f"DELETE FROM player_attrs WHERE player_id IN (SELECT id FROM player WHERE name LIKE '{prefix}%{_suffix}');")
        sql(f"DELETE FROM player_skill WHERE player_id IN (SELECT id FROM player WHERE name LIKE '{prefix}%{_suffix}');")
        sql(f"DELETE FROM player WHERE name LIKE '{prefix}%{_suffix}';")
    sql(f"DELETE FROM room WHERE vnum={ROOM};")
