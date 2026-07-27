#!/usr/bin/env python3
"""Smoke test for garrotte/throatslit (spell/skill functional-completeness
audit, 2026-07-27: Thief roster entries, skill.c level 1). See
cmd_garrotte.c/cmd_throatslit.c's own header comments for scope-down
rationale versus the real upstream (tool-item requirement / willKill()
instant-death roll, neither ported).

    python3 tests/smoke_test_thiefmurder.py [host] [port]
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


announce("smoke_test_thiefmurder")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))

CLASS_THIEF = 3
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


def set_class(name, cls):
    sql(f"UPDATE player SET class={cls} WHERE name='{name}';")


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


def make_single(prefix, cls, level=None):
    name, pw = f"{prefix}{_suffix}", f"{prefix}pw12345"
    s0 = make_char(name, pw)
    cmd(s0, "quit!"); s0.close()
    set_class(name, cls)
    if level is not None:
        sql(f"UPDATE player_progress SET level={level} WHERE player_id="
            f"(SELECT id FROM player WHERE name='{name}');")
    set_combat_disc(name, 100)
    set_basic_disc(name, 100)
    s = relog(name, pw)
    cmd(s, "toggle pk")
    return name, s


# =================== 1. garrotte ===================
(nameA, sA) = make_single("Grtt", CLASS_THIEF)
(nameB, sB) = make_single("Grttv", CLASS_WARRIOR)
seed_proficiency(nameA, "garrotte", 100)
send_line(sA, f"garrotte {nameB}")
out = strip(recv_all(sA, 0.4))
check("pull it tight" in out.lower(), "100%-proficiency garrotte succeeds")
out_affects = strip(cmd(sB, "affects"))
check("garrotte" in out_affects.lower(), "the victim now shows the Garrotte disease affect")
sA.close(); sB.close()

(nameC, sC) = make_single("Grttz", CLASS_THIEF)
(nameD, sD) = make_single("Grttzv", CLASS_WARRIOR)
seed_proficiency(nameC, "garrotte", 0)
out = strip(cmd(sC, f"garrotte {nameD}"))
check("twist free" in out.lower(), "0%-proficiency garrotte fails")
sC.close(); sD.close()

# =================== 2. throatslit ===================
(nameE, sE) = make_single("Slt", CLASS_THIEF)
(nameF, sF) = make_single("Sltv", CLASS_WARRIOR)
seed_proficiency(nameE, "throatslit", 100)
send_line(sE, f"throatslit {nameF}")
out = strip(recv_all(sE, 0.4))
check("slice open" in out.lower(), "100%-proficiency throatslit succeeds and lands")
out_f = strip(recv_all(sF, 0.4))
check("slices open your throat" in out_f.lower(), "the victim sees the throatslit message")
sE.close(); sF.close()

(nameG, sG) = make_single("Sltz", CLASS_THIEF)
(nameH, sH) = make_single("Sltzv", CLASS_WARRIOR)
seed_proficiency(nameG, "throatslit", 0)
out = strip(cmd(sG, f"throatslit {nameH}"))
check("sense you coming" in out.lower(), "0%-proficiency throatslit fails but still starts a fight")
sG.close(); sH.close()

announce_done("smoke_test_thiefmurder")
print("=== ALL CHECKS PASSED ===")
