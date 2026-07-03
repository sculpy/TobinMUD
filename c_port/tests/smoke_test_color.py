#!/usr/bin/env python3
"""Smoke test for color code translation, checked at the raw-byte level
(not just substring matching, same discipline as the Session 9 CRLF test).

Requires room vnum 1's description to currently contain "<r>"/"<b>" tags --
this test does NOT set that up itself (the server caches rooms in memory
after first load, so a DB UPDATE mid-test wouldn't take effect without a
server restart; see STATUS.md session log for how this was staged by hand).

    python3 tests/smoke_test_color.py [host] [port]
"""
import socket
import sys
import time

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000
_suffix = str(int(time.time()) % 100000)
account_name = f"ColorTester{_suffix}"
char_name = f"Colorful{_suffix}"
password = "colortestpw123"


def recv_all_bytes(sock, timeout=1.0):
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
    return b"".join(chunks)


def send_line(sock, line):
    sock.sendall((line + "\r\n").encode())


def check(condition, message):
    if not condition:
        raise AssertionError(message)
    print(f">>> OK: {message}")


s = socket.create_connection((host, port), timeout=5)
recv_all_bytes(s)
send_line(s, account_name)
recv_all_bytes(s)
send_line(s, password)
recv_all_bytes(s)
send_line(s, "new")
recv_all_bytes(s)
send_line(s, char_name)
recv_all_bytes(s)
send_line(s, "done")
recv_all_bytes(s)  # auto-look on creation, before we've confirmed the tagged room, ignore

# Color defaults to ON -- look should show real ANSI escapes, no raw "<r>" text.
send_line(s, "look")
raw = recv_all_bytes(s)
print(f"=== look, color on (raw bytes) ===\n{raw!r}")
check(b"\x1b[31m" in raw, "red ANSI escape (\\x1b[31m) present when color is on")
check(b"\x1b[34m" in raw, "blue ANSI escape (\\x1b[34m) present when color is on")
check(b"\x1b[0m" in raw, "reset ANSI escape (\\x1b[0m) present when color is on")
check(b"<r>" not in raw, "raw '<r>' tag text does not leak through when translated")
check(b"<1>" not in raw, "raw '<1>' tag text does not leak through when translated")

# Turn color off -- the tags should be stripped entirely, no ANSI, no raw tags either.
send_line(s, "color off")
recv_all_bytes(s)
send_line(s, "look")
raw = recv_all_bytes(s)
print(f"=== look, color off (raw bytes) ===\n{raw!r}")
check(b"\x1b[" not in raw, "no ANSI escapes at all when color is off")
check(b"<r>" not in raw and b"<1>" not in raw and b"<b>" not in raw,
      "raw tag text is stripped (not leaked) when color is off")
check(b"red" in raw and b"blue" in raw, "the surrounding plain text survives with color off")

s.close()
print("=== ALL CHECKS PASSED ===")
