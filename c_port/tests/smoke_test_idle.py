#!/usr/bin/env python3
"""Smoke test for the idle flag in `who`.

An active player is NOT tagged (idle); the (idle) tag itself appears only
after five minutes of no input (verified by logic/manual test, too slow for
the sweep), and any command clears it.

    python3 tests/smoke_test_idle.py [host] [port]
"""
import socket
import sys
import time

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000
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


name = f"Idle{_suffix}"
s = socket.create_connection((host, port), timeout=5)
recv_all(s)
for step in (name, "idlepw", "idlepw", "new", name, "done"):
    send_line(s, step); recv_all(s)
cmd(s, "color off")

out = cmd(s, "who")
check(name in out, "who lists the active player")
check("(idle)" not in out, "an active player is not tagged (idle)")

s.close()
print("=== ALL CHECKS PASSED ===")
