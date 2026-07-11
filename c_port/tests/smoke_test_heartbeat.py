#!/usr/bin/env python3
"""Smoke test for the half-hour real-time heartbeat tick (Session 43
continued, user: "every hour on the half hour send a blank line of
uinput to the game so a tick becomes apparent to the player without any
messages"). heartbeat.c's bucket-boundary logic (fires once per real
wall-clock half-hour, not once per ~60s pulse) was manually verified
this session with a temporarily shortened bucket window (15s instead of
3600s) and a fast pulse interval, confirming: (1) a blank line is
actually delivered, (2) it does NOT re-fire on every pulse within the
same bucket. That test isn't practical to keep as an automated smoke
test -- waiting for a real half-hour boundary takes up to ~30 minutes,
and shortening the interval requires editing/rebuilding the binary,
which the full sweep can't do per-test.

What this test CAN verify without waiting an hour:
  1. A normal short observation window produces no unexpected blank-only
     bursts (the tick isn't mis-firing constantly).

    python3 tests/smoke_test_heartbeat.py [host] [port]
"""
import socket
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


announce("smoke_test_heartbeat")

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


name = f"Hbchk{_suffix}"
pw = "hbchkpw123"

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
cmd(s, "done")
cmd(s, "done")

# --- a short window shouldn't produce a flood of blank-only bursts ---
blank_bursts = 0
t0 = time.time()
while time.time() - t0 < 5:
    out = recv_all(s, 1.0)
    if out and out.replace("\r", "").replace("\n", "") == "":
        blank_bursts += 1
check(blank_bursts <= 1,
      f"a short 5s window doesn't flood blank-only bursts (saw {blank_bursts}, "
      "the tick fires at most once per real half-hour)")

s.close()
announce_done("smoke_test_heartbeat")
print("=== ALL CHECKS PASSED ===")
