#!/usr/bin/env python3
"""Smoke test for duplicate character session prevention.

Tests that logging in as the same character from two connections results in
the old connection being taken over (SneezyMUD-style reclaim behavior),
preventing two instances of the same character from existing simultaneously.

    python3 tests/smoke_test_duplicate_sessions.py [host] [port]
"""
import re
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


def strip(s):
    return re.sub(r"\x1b\[[0-9;]*m", "", s)


def make_char(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "y"); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "y"); recv_all(s)   # enable color
    send_line(s, ""); recv_all(s)    # timezone: none
    send_line(s, "new"); recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "1"); recv_all(s)   # race
    send_line(s, "1"); recv_all(s)   # class
    send_line(s, "done"); recv_all(s)  # attrs
    send_line(s, "done"); recv_all(s)  # options
    return s


def connect_and_login(name, pw, char_choice="1"):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    out = cmd(s, char_choice, 0.7)
    time.sleep(0.2)
    return s, out


print("=== Duplicate Session Prevention Test ===\n")

name, pw = f"Dupt{_suffix}", "duptestpw123"
s0 = make_char(name, pw)
cmd(s0, "quit!"); s0.close()
time.sleep(0.2)

print("Test 1: Log in on first connection...")
s1, out1 = connect_and_login(name, pw)
check("welcome" in strip(out1).lower(), "First connection succeeds and gets welcome message")

print("Test 2: Log in as same character on second connection...")
s2, out2 = connect_and_login(name, pw)
time.sleep(0.3)

check("welcome" in strip(out2).lower(), "Second connection also succeeds (takes over)")

time.sleep(0.2)
out1_after = strip(recv_all(s1, 0.5))
check("connection has been taken over" in out1_after.lower(),
      "Old connection sees 'connection taken over' message")

print("Test 3: Verify old connection is disconnected...")
try:
    s1.sendall(b"look\r\n")
    time.sleep(0.1)
    result = recv_all(s1, 0.3)
    check(not result or "connection has been taken over" in result.lower(),
          "Old connection is no longer active")
except (BrokenPipeError, ConnectionResetError, OSError):
    check(True, "Old connection was forcibly closed")

print("Test 4: Verify new connection works...")
out = strip(cmd(s2, "look", 0.5))
check(len(out.strip()) > 0, "New connection can execute commands normally")

cmd(s2, "quit!")
s1.close()
s2.close()

print("\n=== ALL CHECKS PASSED ===")
