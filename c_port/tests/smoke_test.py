#!/usr/bin/env python3
"""Manual smoke test for the walking skeleton: drives a raw TCP session
through account creation, the account menu, character creation (default
attributes), `look`, `who`, and `score`. Not a unit test (no framework
needed, see STATUS.md) -- run by hand against a live tobin_c + seeded DB:

    python3 tests/smoke_test.py [host] [port] [name]
"""
import socket
import sys
import time

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000
name = sys.argv[3] if len(sys.argv) > 3 else "SmokeTester"


def recv_all(sock, timeout=1.0):
    _deadline = time.monotonic() + max(8.0, timeout * 8)
    chunks = []
    try:
        while True:
            _remaining = _deadline - time.monotonic()
            if _remaining <= 0:
                break
            sock.settimeout(min(timeout, _remaining))
            data = sock.recv(4096)
            if not data:
                break
            chunks.append(data)
    except socket.timeout:
        pass
    return b"".join(chunks)


def send_line(sock, line):
    sock.sendall((line + "\r\n").encode())


s = socket.create_connection((host, port), timeout=5)

banner = recv_all(s)
print("=== connect ===")
print(banner.decode(errors="replace"))

send_line(s, name)
print("=== after account name ===")
print(recv_all(s).decode(errors="replace"))

send_line(s, "y")  # confirm new account
print("=== confirm new account ===")
print(recv_all(s).decode(errors="replace"))

send_line(s, "smoketestpw123")
print(recv_all(s).decode(errors="replace"))
send_line(s, "smoketestpw123")  # confirm password (Session 21)
print("=== after password (new account) -> color prompt ===")
print(recv_all(s).decode(errors="replace"))

send_line(s, "y")  # enable color
print("=== color prompt -> time zone prompt ===")
print(recv_all(s).decode(errors="replace"))

send_line(s, "")  # accept default time zone
print("=== time zone prompt -> account menu ===")
print(recv_all(s).decode(errors="replace"))

send_line(s, "new")
print("=== choose 'new' ===")
print(recv_all(s).decode(errors="replace"))

send_line(s, name)
print("=== character name -> race screen ===")
print(recv_all(s).decode(errors="replace"))

send_line(s, "1")
print("=== race: human -> class screen ===")
print(recv_all(s).decode(errors="replace"))

send_line(s, "1")
print("=== class: mage -> attr screen ===")
print(recv_all(s).decode(errors="replace"))

send_line(s, "done")
print("=== accept default attrs, finish -> options menu ===")
print(recv_all(s).decode(errors="replace"))

send_line(s, "done")
print("=== accept default options, finish (auto look) ===")
print(recv_all(s).decode(errors="replace"))

send_line(s, "who")
print("=== who ===")
print(recv_all(s).decode(errors="replace"))

send_line(s, "look")
print("=== look ===")
print(recv_all(s).decode(errors="replace"))

send_line(s, "score")
print("=== score ===")
print(recv_all(s).decode(errors="replace"))

send_line(s, "gibberish")
print("=== unknown command ===")
print(recv_all(s).decode(errors="replace"))

s.close()
print("=== done ===")
