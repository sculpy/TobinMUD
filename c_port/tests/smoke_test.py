#!/usr/bin/env python3
"""Manual smoke test for the walking skeleton: drives a raw TCP session
through account creation, the account menu, character creation (default
attributes), `look`, `who`, and `score`. Not a unit test (no framework
needed, see STATUS.md) -- run by hand against a live tobin_c + seeded DB:

    python3 tests/smoke_test.py [host] [port] [name]
"""
import socket
import sys

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000
name = sys.argv[3] if len(sys.argv) > 3 else "SmokeTester"


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

send_line(s, "smoketestpw123")
print("=== after password (new account) -> account menu ===")
print(recv_all(s).decode(errors="replace"))

send_line(s, "new")
print("=== choose 'new' ===")
print(recv_all(s).decode(errors="replace"))

send_line(s, name)
print("=== character name -> attr screen ===")
print(recv_all(s).decode(errors="replace"))

send_line(s, "done")
print("=== accept default attrs, finish (auto look) ===")
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
