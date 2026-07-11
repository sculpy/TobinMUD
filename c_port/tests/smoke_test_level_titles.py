#!/usr/bin/env python3
"""Smoke test for the immortal rank-title display (score and who): 51-53
"Immortal", 54-57 "God", 58 "Greater God", 59 "Administrator", 60
"Implementor"; mortals (1-50) show "Level: N" instead.

    python3 tests/smoke_test_level_titles.py [host] [port]
"""
import re
import socket
import subprocess
import sys
import time

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

def announce(test_name, host=host, port=port):
    """Tell the running MUD which smoke test is executing: emits a [TEST]
    log line (visible to online immortals and in the day's log file) via
    the loopback-only `@test` server hook. Best-effort -- never fails the
    test. Self-contained (own socket, doesn't depend on this file's other
    helpers) so it can run at any point in the script."""
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
    """Companion to announce() -- emits a [TEST] "finished" log line. Call
    once at the very end of a smoke test, just before it reports success."""
    announce(f"done {test_name}", host, port)


announce("smoke_test_level_titles")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))

_ANSI = re.compile(r"\x1b\[[0-9;]*m")


def strip_ansi(s):
    """Immortal rank tiers now color the who bracket / score Level field, so
    format assertions run on the color-stripped text (color presence itself
    is covered by smoke_test_immmisc.py)."""
    return _ANSI.sub("", s)


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
    send_line(s, "y")
    recv_all(s)
    send_line(s, pw)
    recv_all(s)
    send_line(s, pw)  # confirm password (Session 21)
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
s1, name1 = make_player("Mone")
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
    s, name = make_player("L" + "".join(chr(ord("a") + int(dd)) for dd in str(level)))
    set_level(name, level)
    s = reconnect(s, name)

    send_line(s, "score")
    raw = recv_all(s)
    out = strip_ansi(raw)
    check(f"Level:         {expected_title}" in out, f"level {level} score shows '{expected_title}'")
    check(f"Level:         {level}" not in out, f"level {level} score does NOT show the raw number")

    send_line(s, "who")
    raw = recv_all(s)
    out = strip_ansi(raw)
    width = 13
    pad = width - len(expected_title)
    left, right = pad // 2, pad - pad // 2
    bracketed = f"[{' ' * left}{expected_title}{' ' * right}]"
    check(f"{bracketed} {proper(name)}" in out, f"level {level} who shows '{bracketed}' next to the name")
    # The name must NOT be colored; the rank color wraps the bracket instead.
    # In the raw stream an ANSI reset should sit right after the bracket's ']'
    # and immediately before the (uncolored) name.
    check(f"]\x1b[0m {proper(name)}" in raw,
          f"level {level} who colors the bracket, not the name")

    s.close()

announce_done("smoke_test_level_titles")
print("=== ALL CHECKS PASSED ===")
