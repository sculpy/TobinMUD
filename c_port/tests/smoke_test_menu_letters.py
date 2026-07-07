#!/usr/bin/env python3
"""Smoke test for the lettered account menu (Session 21):
  C connect (bare with one char, by number, by name), N new, D delete,
  Q quit -- case-insensitive; bare q quits ONLY here. Old inputs
  (numbers, "new", "delete <name>", "quit!") keep working (covered by
  every other test's login flow).

    python3 tests/smoke_test_menu_letters.py [host] [port]
"""
import socket
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


announce("smoke_test_menu_letters")

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


def check(condition, message):
    if not condition:
        raise AssertionError(message)
    print(f">>> OK: {message}")


acct = f"Menular{_suffix}"
name1 = f"Menuone{_suffix}"
name2 = f"Menutwo{_suffix}"
s = socket.create_connection((host, port), timeout=5)
recv_all(s)
send_line(s, acct)
recv_all(s)
send_line(s, "menupw")
recv_all(s)
send_line(s, "menupw")  # confirm password (Session 21)
recv_all(s)             # color preference prompt (asked at account creation)
send_line(s, "y")       # accept color -> account menu
out = recv_all(s)
check("C [number|name]" in out, "the menu shows the lettered command table")

# N (uppercase) starts creation.
send_line(s, "N")
out = recv_all(s)
check("New character name" in out, "'N' starts character creation")
send_line(s, name1)
recv_all(s)
send_line(s, "done")
out = recv_all(s)
check(f"Welcome, {name1.capitalize()}" in out, "creation completes")

send_line(s, "quit!")
recv_all(s)

# Bare c with exactly one character connects it.
send_line(s, "c")
out = recv_all(s)
check(f"Welcome, {name1.capitalize()}" in out, "bare 'c' connects the only character")
send_line(s, "quit!")
recv_all(s)

# Second character; then C by name and by number.
send_line(s, "n")
recv_all(s)
send_line(s, name2)
recv_all(s)
send_line(s, "done")
recv_all(s)
send_line(s, "quit!")
recv_all(s)

send_line(s, "c")
out = recv_all(s)
check("Connect which one?" in out, "bare 'c' with two characters asks which")

send_line(s, f"C {name2}")
out = recv_all(s)
check(f"Welcome, {name2.capitalize()}" in out, "'C <name>' connects by name (case-insensitive letter)")
send_line(s, "quit!")
recv_all(s)

send_line(s, "c 1")
out = recv_all(s)
check("Welcome," in out, "'c 1' connects by menu number")
send_line(s, "quit!")
recv_all(s)

# D <name> deletes (with the YES confirmation).
send_line(s, f"d {name2}")
out = recv_all(s)
check("Really delete" in out, "'d <name>' asks for confirmation")
send_line(s, "YES")
out = recv_all(s)
check("Enter your account password" in out, "YES prompts for account password reconfirmation")
send_line(s, "menupw")
out = recv_all(s)
check("deleted" in out.lower() and name2.capitalize() not in out.split("--")[-1],
      "the character is deleted and gone from the menu")

# Q quits -- the one place a bare q works.
send_line(s, "Q")
out = recv_all(s)
check("Goodbye" in out, "'Q' quits from the menu")
try:
    s.settimeout(2.0)
    check(s.recv(1024) == b"", "the connection is closed after Q")
except ConnectionError:
    check(True, "the connection is closed after Q")
s.close()
announce_done("smoke_test_menu_letters")
print("=== ALL CHECKS PASSED ===")
