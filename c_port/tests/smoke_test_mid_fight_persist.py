#!/usr/bin/env python3
"""Smoke test for mid-fight HP persistence (combat.c's combat_process_run(),
TODO.md "Mid-fight persistence"). Before this fix, HP was only saved at
defeat/quit (descriptor_leave_to_menu()) -- a mid-fight disconnect (crash, or
a losing player quietly pulling the plug) reloaded at whatever HP was last
saved before the fight even started, silently undoing all damage taken.

Flow: A (mortal) fights a deliberately tanky, weak-hitting sandbox mob so the
fight runs many rounds without either side dying. Once A visibly takes
damage, A's socket is closed ABRUPTLY (no `quit!`) -- a raw disconnect, same
as a crash. Since no OTHER live descriptor drives this fight (the mob has
none), the pair is frozen the instant A drops -- so a fresh reconnect must
show the EXACT same damaged HP, not the pre-fight max, proving the periodic
in-round save landed.

    python3 tests/smoke_test_mid_fight_persist.py [host] [port]
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


announce("smoke_test_mid_fight_persist")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
MOB_VNUM = 900000 + (int(time.time()) % 80000)


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


def sql(stmt):
    subprocess.run(["mariadb", "sneezy", "-e", stmt], check=True)


def set_level(name, level):
    sql(f"UPDATE player_progress SET level={level} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def set_hp(name, hp, max_hp):
    sql(f"UPDATE player_progress SET hp={hp}, max_hp={max_hp} WHERE player_id="
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
    send_line(s, "1"); recv_all(s)  # race: human
    send_line(s, "1"); recv_all(s)  # class: mage
    send_line(s, "done"); recv_all(s)
    send_line(s, "2"); recv_all(s)  # alignment: neutral
    return s


def relog(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


def hp_from_score(out):
    m = re.search(r"HP:\s+(\d+)/(\d+)", out)
    return (int(m.group(1)), int(m.group(2))) if m else None


name = f"Persist{_suffix}"
pw = "persistpw12345"
imm_name = f"Persistimm{_suffix}"
imm_pw = "persistimmpw123"

s = make_char(name, pw)
s.close()
# Give A a big HP buffer so a tanky, weak-hitting mob can land occasional
# real damage across many rounds without ever threatening to kill A --
# this is legitimate test setup (done before combat starts), not the
# mechanism under test.
set_hp(name, 300, 300)
s = relog(name, pw)

s_imm = make_char(imm_name, imm_pw)
s_imm.close()
set_level(imm_name, 51)
s_imm = relog(imm_name, imm_pw)

cmd(s_imm, f"goto {name}")

# Tanky (huge hpbonus), weak-hitting (default tohit/damage) sandbox mob --
# needs to survive dozens of rounds so it gets a chance to land a hit on A
# before A can possibly kill it first. max_hp = 20 + level*5 + hpbonus*10
# (being.c) = 20 + 5 + 2000 = 2025.
cols = {
    "vnum": MOB_VNUM, "name": "'tankmob'", "short_desc": "'a tanky sandbag'",
    "long_desc": "'A tanky sandbag stands here.'", "description": "'desc'",
    "actions": 0, "affects": 0, "faction": 0, "fact_perc": 0,
    "letter": "'A'", "attacks": 1.0,
    "class": 0, "level": 1, "tohit": 0, "ac": 0, "hpbonus": 200,
    "damage_level": 0, "damage_precision": 0, "gold": 0, "race": 0,
    "weight": 0, "height": 0, "str": 0, "bra": 0, "con": 0, "dex": 0,
    "agi": 0, "intel": 0, "wis": 0, "foc": 0, "per": 0, "cha": 0,
    "kar": 0, "spe": 0, "pos": 10, "def_position": 10, "sex": 1,
    "spec_proc": 0, "skin": 0, "vision": 0, "can_be_seen": 1,
    "max_exist": 1,
}
col_names = ",".join(cols.keys())
col_values = ",".join(str(v) for v in cols.values())
sql(f"INSERT INTO mob ({col_names}) VALUES ({col_values});")
cmd(s_imm, f"load mob {MOB_VNUM}")

out = cmd(s, "attack tankmob")
check("You attack" in out, "A engages the sandbox mob")

damaged_hp = None
for _ in range(12):
    time.sleep(1.5)
    out = cmd(s, "score")
    hp = hp_from_score(out)
    check(hp is not None, "A is still alive and score shows an HP line")
    if hp[0] < hp[1]:
        damaged_hp = hp[0]
        break

check(damaged_hp is not None, "A took real damage from the tanky mob within the round budget")
print(f"A's HP right before the abrupt disconnect: {damaged_hp}/300")

# --- the abrupt disconnect: a raw socket close, NOT `quit!` -- same as a
# crash. No other live descriptor drives this A-vs-mob pair (the mob has
# none), so the fight is frozen the instant this socket dies. ---
s.close()
time.sleep(2.5)

s2 = relog(name, pw)
out = cmd(s2, "score")
hp_after = hp_from_score(out)
check(hp_after is not None, "reconnect lands back in the game with a live HP line, not dead/menu")
check(hp_after[0] == damaged_hp,
      f"HP survived the disconnect+reconnect exactly ({hp_after[0]} == {damaged_hp}), "
      "not reset to the pre-fight max -- mid-fight persistence works")
check(hp_after[0] < hp_after[1], "the reconnected HP is still genuinely damaged, not full")

s2.close()
s_imm.close()
announce_done("smoke_test_mid_fight_persist")
print("=== ALL CHECKS PASSED ===")
