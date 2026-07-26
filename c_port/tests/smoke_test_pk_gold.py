#!/usr/bin/env python3
"""Smoke test for gold-on-kill (combat.c's combat_defeat(), TODO.md "Split
gold on kill" / "Split victim's gold among the group on kill"). Solo case
only -- there's no group/party system yet, so a PK winner simply takes the
loser's ENTIRE gold; the "split it between all group members if grouped"
half remains a separate, still-blocked item.

Flow: A (0 gold) and B (500 gold, deliberately 1 HP so the very first landed
hit finishes the fight, keeping this deterministic) both opt into PK
(`toggle pk`) and A attacks B. Checks:
  1. A's incoming text shows the loot message and B's shows the matching one.
  2. A's gold ends at 500, B's at 0 (both via direct DB read, since B is
     ejected to the account menu on defeat).

    python3 tests/smoke_test_pk_gold.py [host] [port]
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


announce("smoke_test_pk_gold")

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
    subprocess.run(["mariadb", "tobin", "-e", stmt], check=True)


def query(stmt):
    return subprocess.run(["mariadb", "-N", "tobin", "-e", stmt],
                          check=True, capture_output=True, text=True).stdout.strip()


def gold_of(name):
    return int(query(f"SELECT gold FROM player_progress WHERE player_id="
                      f"(SELECT id FROM player WHERE name='{name}');"))


def set_gold(name, amount):
    sql(f"UPDATE player_progress SET gold={amount} WHERE player_id="
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
    send_line(s, "done"); recv_all(s)  # alignment: neutral
    return s


def relog(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


nameA = f"Goldwin{_suffix}"
nameB = f"Goldlos{_suffix}"
pw = "goldtestpw1234"

sA = make_char(nameA, pw); sA.close()
sB = make_char(nameB, pw); sB.close()

set_gold(nameA, 0)
set_gold(nameB, 500)
set_hp(nameB, 1, 1)  # any landed hit finishes B -- keeps this deterministic

sA = relog(nameA, pw)
sB = relog(nameB, pw)

step_out_a = cmd(sA, "toggle pk")
check("pk" in step_out_a.lower(), "A opts into PK")
step_out_b = cmd(sB, "toggle pk")
check("pk" in step_out_b.lower(), "B opts into PK")

out = cmd(sA, f"attack {nameB}")
looted = False
for _ in range(8):
    if "loot" in out.lower() or "slain" in out.lower() or "defeated" in out.lower():
        looted = True
        break
    out += recv_all(sA, 1.5)

check(looted, "the fight resolved (B, at 1 HP, went down to the first landed hit)")
check("You loot 500 gold" in out, "A's screen shows the loot message with the right amount")

sA.close()
sB.close()
time.sleep(0.5)  # let the server-side saves land

check(gold_of(nameA) == 500, "A ended up with all 500 gold")
check(gold_of(nameB) == 0, "B's gold was fully taken")

announce_done("smoke_test_pk_gold")
print("=== ALL CHECKS PASSED ===")
