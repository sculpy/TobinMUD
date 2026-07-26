#!/usr/bin/env python3
"""Smoke test for `wiznet`'s `@<level>` targeting (Sneezy → Tobin feature
audit, "OOC channels" -- ported from Sneezy's real `commune @<level>`).
Covers:
  1. Bare `wiznet <msg>` reaches every online immortal, high and low level.
  2. `wiznet @<level> <msg>` reaches only immortals at or above that level.
  3. The `;` shorthand still works and composes with `@<level>`.

    python3 tests/smoke_test_wiznet.py [host] [port]
"""
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


announce("smoke_test_wiznet")

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


def set_level(name, level):
    sql(f"UPDATE player_progress SET level={level} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def make_char(sock, name, pw):
    recv_all(sock)
    send_line(sock, name); recv_all(sock)
    send_line(sock, "y"); recv_all(sock)
    send_line(sock, pw); recv_all(sock)
    send_line(sock, pw); recv_all(sock)
    send_line(sock, "new"); recv_all(sock)
    send_line(sock, name); recv_all(sock)
    send_line(sock, "1"); recv_all(sock)
    send_line(sock, "1"); recv_all(sock)
    send_line(sock, "done"); recv_all(sock)
    send_line(sock, "done"); recv_all(sock)


def login(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


low_name, low_pw = f"Wiznlo{_suffix}", "wiznlopw1234"
hi_name, hi_pw = f"Wiznhib{_suffix}", "wiznhipw1234"

s = socket.create_connection((host, port), timeout=5)
make_char(s, low_name, low_pw)
set_level(low_name, 51)
s.close()
slow = login(low_name, low_pw)

s = socket.create_connection((host, port), timeout=5)
make_char(s, hi_name, hi_pw)
set_level(hi_name, 59)
s.close()
shi = login(hi_name, hi_pw)

# --- 1: bare wiznet reaches everyone immortal ---
cmd(shi, "wiznet hello everyone")
out_low = recv_all(slow, timeout=1.0)
check("hello everyone" in out_low, "bare wiznet reaches a level-51 immortal")

# --- 2: @<level> narrows delivery ---
cmd(shi, "wiznet @59 admins only")
out_low2 = recv_all(slow, timeout=1.0)
check("admins only" not in out_low2, "wiznet @59 does NOT reach a level-51 immortal")

out_hi = cmd(shi, "wiznet @59 admins only again")
check("admins only again" in out_hi, "the sender (already 59+) sees their own @59 message")

# --- 3: ; shorthand composes with @<level> ---
cmd(shi, ";@59 shorthand test")
out_low3 = recv_all(slow, timeout=1.0)
check("shorthand test" not in out_low3, "the ; shorthand still respects @<level> targeting")

slow.close()
shi.close()
announce_done("smoke_test_wiznet")
print("=== ALL CHECKS PASSED ===")
