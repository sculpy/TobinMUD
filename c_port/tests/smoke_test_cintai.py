#!/usr/bin/env python3
"""Smoke test for cintai (spell/skill functional-completeness audit
continued, Monk level 5). See combat.c's own header comment at the
hook point for the real-upstream research (attackRound(), misc/
combat.cc, found in the fuller peel-sneezymud reference clone) --
a general to-hit bonus, not gated on being unarmed despite skill.c's
original "while unarmed" flavor text (now corrected). Verified the
same statistical way as kubo/oomlat (smoke_test_monkpassives.py): a
fixed dex mismatch gives a known baseline hit rate, cintai's own bonus
should reliably move a 40-round hit count in the expected direction.

    python3 tests/smoke_test_cintai.py [host] [port]
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


announce("smoke_test_cintai")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))

CLASS_MONK = 5


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


def set_class(name, cls):
    sql(f"UPDATE player SET class={cls} WHERE name='{name}';")


def set_dex(name, dex):
    sql(f"UPDATE player_attrs SET dexterity={dex} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def set_hp(name, hp, max_hp):
    sql(f"UPDATE player_progress SET hp={hp}, max_hp={max_hp} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def set_discs(name):
    sql(f"UPDATE player_progress SET basic_disc_pct=100, combat_disc_pct=100 "
        f"WHERE player_id=(SELECT id FROM player WHERE name='{name}');")


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


def make_pair(prefix, room=None):
    nameA, pwA = f"{prefix}a{_suffix}", f"{prefix}apw12345"
    nameB, pwB = f"{prefix}b{_suffix}", f"{prefix}bpw12345"
    sA0 = make_char(nameA, pwA)
    sB0 = make_char(nameB, pwB)
    cmd(sA0, "quit!"); sA0.close()
    cmd(sB0, "quit!"); sB0.close()
    for nm in (nameA, nameB):
        set_class(nm, CLASS_MONK)
        set_hp(nm, 5000, 5000)
        set_discs(nm)
        if room is not None:
            sql(f"UPDATE player SET load_room={room} WHERE name='{nm}';")
    sA = relog(nameA, pwA)
    sB = relog(nameB, pwB)
    cmd(sA, "toggle pk"); cmd(sB, "toggle pk")
    return (nameA, sA), (nameB, sB)


def attack_and_settle(sock, target_name):
    cmd(sock, f"attack {target_name}")
    time.sleep(1.3)


def run_rounds(sock, rounds):
    full = ""
    for _ in range(rounds):
        time.sleep(1.3)
        full += strip(recv_all(sock, 0.3))
    return full


ROOM = 976000 + (int(time.time()) % 10000)
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Cintai Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")

sockets = []
try:
    # A -20 dex-derived modifier (unclamped -- combat.c's own clamp only
    # bites past +/-44) gives a ~30% baseline hit chance with 0% cintai;
    # cintai's own +15ish (100*3/20) bonus at 100% pushes that to ~45%.
    # Deliberately NOT the same clamped-edge (-45, ~6% floor) calibration
    # smoke_test_monkpassives.py's kubo/oomlat checks use -- verified
    # live that the low-single-digit hit counts a 6% baseline produces
    # over 40 rounds are too noisy for cintai's smaller effect size (one
    # real run came back backwards, 15 vs 18); this wider, more central
    # baseline gives bigger absolute hit counts on both sides and a much
    # more reliable separation.
    (nameA, sA), (nameB, sB) = make_pair("Cnz", room=ROOM)  # control, no cintai
    sockets += [sA, sB]
    set_dex(nameA, 70); set_dex(nameB, 150)
    attack_and_settle(sA, nameB)
    out_control = run_rounds(sA, 40)
    control_misses = out_control.lower().count(f"you miss {nameB.lower()}")
    sA.close(); sB.close()

    (nameC, sC), (nameD, sD) = make_pair("Cnw", room=ROOM)  # cintai 100%
    sockets += [sC, sD]
    set_dex(nameC, 70); set_dex(nameD, 150)
    seed_proficiency(nameC, "cintai", 100)
    attack_and_settle(sC, nameD)
    out_cintai = run_rounds(sC, 40)
    cintai_misses = out_cintai.lower().count(f"you miss {nameD.lower()}")
    sC.close(); sD.close()

    cintai_hits = 40 - cintai_misses
    control_hits = 40 - control_misses
    check(cintai_hits > control_hits,
          f"100%-cintai lands more hits over 40 rounds than a 0%-cintai control "
          f"({cintai_hits} vs {control_hits})")

    sockets = []
    announce_done("smoke_test_cintai")
    print("=== ALL CHECKS PASSED ===")
finally:
    for sock in sockets:
        try:
            sock.close()
        except OSError:
            pass
    for prefix in ("Cnz", "Cnw"):
        sql(f"DELETE FROM player_progress WHERE player_id IN (SELECT id FROM player WHERE name LIKE '{prefix}%{_suffix}');")
        sql(f"DELETE FROM player_attrs WHERE player_id IN (SELECT id FROM player WHERE name LIKE '{prefix}%{_suffix}');")
        sql(f"DELETE FROM player_skill WHERE player_id IN (SELECT id FROM player WHERE name LIKE '{prefix}%{_suffix}');")
        sql(f"DELETE FROM player WHERE name LIKE '{prefix}%{_suffix}';")
    sql(f"DELETE FROM room WHERE vnum={ROOM};")
