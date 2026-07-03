#!/usr/bin/env python3
"""Smoke test for the immortal rank-title display (score and who): 51-53
"Immortal", 54-57 "God", 58 "Greater God", 59 "Administrator", 60
"Implementor"; mortals (1-50) show "Level: N" instead.

    python3 tests/smoke_test_level_titles.py [host] [port]
"""
import socket
import subprocess
import sys
import time

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000
_suffix = str(int(time.time()) % 100000)


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


def check(condition, message):
    if not condition:
        raise AssertionError(message)
    print(f">>> OK: {message}")


def proper(name):
    """The server normalizes character names to proper case at creation
    (see being_normalize_name()) -- this mirrors that so assertions against
    server-echoed text match, regardless of how the name was typed here."""
    return name[:1].upper() + name[1:].lower()


def make_player(tag):
    name = f"Title{tag}{_suffix}"
    pw = "titletestpw123"
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name)
    recv_all(s)
    send_line(s, pw)
    recv_all(s)
    send_line(s, "new")
    recv_all(s)
    send_line(s, name)
    recv_all(s)
    send_line(s, "done")
    recv_all(s)
    return s, name


def set_level(name, level):
    subprocess.run(
        ["mariadb", "sneezy", "-e",
         f"insert into player_progress (player_id, level, experience, hp, max_hp) "
         f"select id, {level}, 0, 100, 100 from player where name='{name}' "
         f"on duplicate key update level={level};"],
        check=True,
    )


def reconnect(s, name):
    s.close()
    s2 = socket.create_connection((host, port), timeout=5)
    recv_all(s2)
    send_line(s2, name)
    recv_all(s2)
    send_line(s2, "titletestpw123")
    recv_all(s2)
    send_line(s2, "1")
    recv_all(s2)
    return s2


# Mortal: level 1 default, and level 50 (the mortal ceiling).
s1, name1 = make_player("Mortal1")
send_line(s1, "score")
out = recv_all(s1)
check("Level:         1" in out, "fresh character shows 'Level: 1'")

set_level(name1, 50)
s1 = reconnect(s1, name1)
send_line(s1, "score")
out = recv_all(s1)
check("Level:         50" in out, "level 50 (mortal ceiling) shows 'Level: 50', not a title")
s1.close()

# Immortal tiers: 51, 53 (Immortal); 54, 57 (God); 58 (Greater God);
# 59 (Administrator); 60 (Implementor).
tiers = [
    (51, "Immortal"), (53, "Immortal"),
    (54, "God"), (57, "God"),
    (58, "Greater God"),
    (59, "Administrator"),
    (60, "Implementor"),
]

for level, expected_title in tiers:
    s, name = make_player(f"L{level}")
    set_level(name, level)
    s = reconnect(s, name)

    send_line(s, "score")
    out = recv_all(s)
    check(f"Level:         {expected_title}" in out, f"level {level} score shows '{expected_title}'")
    check(f"Level:         {level}" not in out, f"level {level} score does NOT show the raw number")

    send_line(s, "who")
    out = recv_all(s)
    width = 13
    pad = width - len(expected_title)
    left, right = pad // 2, pad - pad // 2
    bracketed = f"[{' ' * left}{expected_title}{' ' * right}]"
    check(f"{bracketed} {proper(name)}" in out, f"level {level} who shows '{bracketed}' next to the name")

    s.close()

print("=== ALL CHECKS PASSED ===")
