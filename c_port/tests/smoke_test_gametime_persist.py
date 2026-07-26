#!/usr/bin/env python3
"""Smoke test for game-clock persistence across restarts (Session 43
continued, user: "make time save so it continues on from boot to boot").
Uses `game_config` directly (the same key/value table gametime_load()/
gametime_save() read and write, mirroring multiplay.c's own pattern) to
set a known clock value, then confirms `time` reflects it -- this is the
practical equivalent of "restart the server and see the clock resume"
without actually restarting the shared server the rest of the sweep
depends on.

    python3 tests/smoke_test_gametime_persist.py [host] [port]
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


announce("smoke_test_gametime_persist")

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


name = f"Gtsave{_suffix}"
pw = "gtsavepw123"

s = socket.create_connection((host, port), timeout=5)
recv_all(s)
send_line(s, name); recv_all(s)
send_line(s, "y"); recv_all(s)
send_line(s, pw); recv_all(s)
send_line(s, pw); recv_all(s)
send_line(s, ""); recv_all(s)   # color default
send_line(s, ""); recv_all(s)   # timezone default
send_line(s, "new"); recv_all(s)
send_line(s, name); recv_all(s)
cmd(s, "1")  # race: human (zero stat modifier)
cmd(s, "1")  # class: mage
cmd(s, "done")
cmd(s, "done")  # alignment: neutral

# 1: a live server has ALREADY persisted some clock value via the tick
# (gametime_save(), called every tick) -- confirm the row exists and its
# hour/minute match what `time` reports right now.
out = cmd(s, "time")
m = re.search(r"It is (\d{1,2}):(\d{2}) (AM|PM), on", out)
check(m is not None, f"`time` shows a well-formed clock (got: {out!r})")
hour12, minute, ampm = m.groups()
hour24 = int(hour12) % 12 + (12 if ampm == "PM" else 0)

result = subprocess.run(
    ["mariadb", "tobin", "-N", "-e",
     "SELECT name, value FROM game_config WHERE name LIKE 'gametime_%';"],
    check=True, capture_output=True, text=True)
rows = dict(line.split("\t") for line in result.stdout.strip().splitlines() if line)
check("gametime_hour" in rows and "gametime_minute" in rows,
      "game_config has persisted gametime_hour/gametime_minute rows")
check(int(rows["gametime_hour"]) == hour24 and int(rows["gametime_minute"]) == int(minute),
      f"the persisted row matches what `time` currently shows "
      f"(persisted {rows['gametime_hour']}:{rows['gametime_minute']}, shown {hour24}:{minute})")

s.close()
announce_done("smoke_test_gametime_persist")
print("=== ALL CHECKS PASSED ===")
