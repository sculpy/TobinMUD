#!/usr/bin/env python3
"""Smoke test for the two-tier quit: `quit!` while playing returns to the
account menu (without disconnecting), and `quit!` at the account menu
actually closes the connection. Only the exact literal "quit!" works --
"quit" alone is deliberately excluded from abbreviation matching.

    python3 tests/smoke_test_quit_menu.py [host] [port]
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


announce("smoke_test_quit_menu")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
account_name = f"QMTester{_suffix}"
char_name = f"QMChar{_suffix}"
# The server normalizes character names to proper case at creation (first
# letter uppercase, rest lowercase) -- see being_normalize_name() -- so
# every displayed occurrence uses this form, not the as-typed char_name.
expected_name = char_name[0].upper() + char_name[1:].lower()
password = "qmtestpw123"


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


def step(sock, label, line):
    send_line(sock, line)
    out = recv_all(sock)
    print(f"=== {label} ===")
    print(out)
    return out


def check(condition, message):
    if not condition:
        raise AssertionError(message)
    print(f">>> OK: {message}")


s = socket.create_connection((host, port), timeout=5)
recv_all(s)
step(s, "account name", account_name)
step(s, "confirm new account creation", "y")
step(s, "password (first entry)", password)
step(s, "confirm password -> menu", password)
step(s, "new", "new")
step(s, "char name -> race screen", char_name)
step(s, "race: human", "1")
step(s, "class: mage -> attr screen", "1")
step(s, "done -> alignment screen", "done")
out = step(s, "alignment: neutral -> playing", "2")
check(f"Welcome, {expected_name}" in out, "character created and playing")

# Bare "quit" (no bang) must NOT do anything -- confirm the char stays put.
out = step(s, "bare quit should be rejected", "quit")
check("Command not found" in out, "bare 'quit' falls through to the unknown-command handler")

# `quit!` while playing should NOT disconnect -- it should return to the menu.
out = step(s, "quit! while playing", "quit!")
check("return to the character menu" in out, "quit!-while-playing message shown")
check("Connect Player" in out, "account menu is shown after quitting the character")
check(expected_name in out, "the character just played still shows up in the menu")

# Bare "quit" at the account menu must also be rejected (not disconnect).
out = step(s, "bare quit at the menu should be rejected", "quit")
check("Goodbye" not in out, "bare 'quit' at the account menu does not disconnect")
check("Huh?" in out, "bare 'quit' at the account menu falls through to the usage hint")

# The connection must still be open: prove it by playing the character again.
out = step(s, "play character #1 again", "1")
check(f"Welcome, {expected_name}" in out, "can re-enter the same character after quitting to menu")

# Quit! to menu again, then quit! AGAIN from the menu -- this should disconnect.
step(s, "quit! to menu (second time)", "quit!")
out = step(s, "quit! from the account menu", "quit!")
check("Goodbye" in out, "account-menu quit! sends a goodbye message")

# Confirm the server actually closed its end (EOF), not just silence.
s.settimeout(2.0)
trailing = s.recv(4096)
check(trailing == b"", "server closed the connection after quitting from the account menu")

s.close()
announce_done("smoke_test_quit_menu")
print("=== ALL CHECKS PASSED ===")
