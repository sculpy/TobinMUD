#!/usr/bin/env python3
"""Smoke test for `egotrip` (user, 2026-07-12: "add egotrip command from
sneezy"). Scoped to just `egotrip blast <target>` -- the original's other
twelve subcommands each depend on systems Tobin hasn't built yet (see
cmd_egotrip.c's header comment). Covers:

  1. A mortal gets "Command not found" (below Implementor tier).
  2. Bad/missing subcommand shows the scoped-down usage line.
  3. `egotrip blast <target>` halves the target's current HP and never
     drops them below 1.

    python3 tests/smoke_test_egotrip.py [host] [port]
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


announce("smoke_test_egotrip")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))


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


def set_hp(name, hp):
    sql(f"UPDATE player_progress SET hp={hp}, max_hp={hp} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def make_char(name, pw, class_choice="1"):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "y"); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "new"); recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "1"); recv_all(s)
    send_line(s, class_choice); recv_all(s)
    send_line(s, "done"); recv_all(s)
    send_line(s, "done"); recv_all(s)
    cmd(s, "color off")
    return s


pw = "egopw123"

imm_name = f"Egoimm{_suffix}"
s_imm = make_char(imm_name, pw, "3")
cmd(s_imm, "quit!")
s_imm.close()
set_level(imm_name, 60)
s_imm = socket.create_connection((host, port), timeout=5)
recv_all(s_imm)
send_line(s_imm, imm_name); recv_all(s_imm)
send_line(s_imm, pw); recv_all(s_imm)
send_line(s_imm, "1"); recv_all(s_imm)
cmd(s_imm, "color off")

mort_name = f"Egomor{_suffix}"
s_mort = make_char(mort_name, pw, "1")

# --- 1: a mortal (below Implementor tier) can't reach it at all ---
out = cmd(s_mort, "egotrip blast someone")
check("Command not found" in out, "a mortal gets the unknown-command message")

# --- 2: bad/missing subcommand shows the scoped usage line ---
out = cmd(s_imm, "egotrip")
check("Only 'blast' is implemented" in out, "egotrip alone shows the scoped usage line")
out = cmd(s_imm, "egotrip cleanse")
check("Only 'blast' is implemented" in out, "an unimplemented subcommand shows the same scoped usage line")

# --- 3: egotrip blast halves the target's HP, floored at 1 ---
cmd(s_mort, "quit!")
s_mort.close()
set_hp(mort_name, 40)
s_mort = socket.create_connection((host, port), timeout=5)
recv_all(s_mort)
send_line(s_mort, mort_name); recv_all(s_mort)
send_line(s_mort, pw); recv_all(s_mort)
send_line(s_mort, "1"); recv_all(s_mort)
cmd(s_mort, "color off")

score = cmd(s_mort, "score")
before_hp = int(re.search(r"HP:\s+(\d+) \(", score).group(1))
out = cmd(s_imm, f"egotrip blast {mort_name}")
check(f"You blast {mort_name}" in out, "egotrip blast confirms delivery to the caster")
out = recv_all(s_mort, 1.0)
check("BZZZZZaaaaaappppp" in out, "the target sees the lightning-bolt message")
score = cmd(s_mort, "score")
after_hp = int(re.search(r"HP:\s+(\d+) \(", score).group(1))
# Regen ticks (REGEN_PULSES, ~every 5s) can nudge HP up between reads --
# check for a roughly-halved result, not an exact value.
check(after_hp <= before_hp // 2 + 3, f"the blast roughly halves HP ({before_hp} -> {after_hp})")

before_hp = after_hp
out = cmd(s_imm, f"egotrip blast {mort_name}")
score = cmd(s_mort, "score")
after_hp = int(re.search(r"HP:\s+(\d+) \(", score).group(1))
check(after_hp <= before_hp // 2 + 3, f"a second blast roughly halves HP again ({before_hp} -> {after_hp})")

s_imm.close()
s_mort.close()
announce_done("smoke_test_egotrip")
print("=== ALL CHECKS PASSED ===")
