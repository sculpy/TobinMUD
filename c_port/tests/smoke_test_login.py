#!/usr/bin/env python3
"""Second-connection smoke test: logs into an EXISTING account (created by
smoke_test.py) to exercise the password-verification path and playing an
existing character via the account menu, while a first connection stays
open -- confirms `who` shows both concurrently connected players. Run
smoke_test.py first.

    python3 tests/smoke_test_login.py [host] [port] [existing_name] [password]
"""
import socket
import sys

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


announce("smoke_test_login")

name = sys.argv[3] if len(sys.argv) > 3 else "Fred"
password = sys.argv[4] if len(sys.argv) > 4 else "smoketestpw123"


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


# First connection: log in as an existing account, play the existing
# character (character #1 in the account menu) via the menu, and STAY connected.
s1 = socket.create_connection((host, port), timeout=5)
recv_all(s1)
send_line(s1, name)
print("=== conn1: after account name (existing account) ===")
print(recv_all(s1).decode(errors="replace"))

send_line(s1, password)
recv_all(s1)
send_line(s1, password)  # confirm password (Session 21)
print("=== conn1: after password -> account menu ===")
print(recv_all(s1).decode(errors="replace"))

send_line(s1, "1")
print("=== conn1: play character #1 (existing character, auto look) ===")
print(recv_all(s1).decode(errors="replace"))

# Second connection: a different fresh account + character, while conn1 is still open.
s2 = socket.create_connection((host, port), timeout=5)
recv_all(s2)
send_line(s2, "Second" + name)
print("=== conn2: after account name (new) ===")
print(recv_all(s2).decode(errors="replace"))
send_line(s2, "anotherpw123")
recv_all(s2)
send_line(s2, "anotherpw123")  # confirm password (Session 21)
print("=== conn2: after password -> account menu ===")
print(recv_all(s2).decode(errors="replace"))
send_line(s2, "new")
print("=== conn2: choose 'new' ===")
print(recv_all(s2).decode(errors="replace"))
send_line(s2, "Second" + name)
print("=== conn2: character name -> attr screen ===")
print(recv_all(s2).decode(errors="replace"))
send_line(s2, "done")
print("=== conn2: accept defaults, finish (auto look) ===")
print(recv_all(s2).decode(errors="replace"))

# Now check `who` from BOTH connections -- each should see both players.
send_line(s1, "who")
print("=== conn1 who (should show both) ===")
print(recv_all(s1).decode(errors="replace"))

send_line(s2, "who")
print("=== conn2 who (should show both) ===")
print(recv_all(s2).decode(errors="replace"))

# Test a WRONG password against the first account from a third connection.
s3 = socket.create_connection((host, port), timeout=5)
recv_all(s3)
send_line(s3, name)
recv_all(s3)
send_line(s3, "definitely-wrong-password")
print("=== conn3: wrong password on existing account ===")
print(recv_all(s3).decode(errors="replace"))
s3.close()

s1.close()
s2.close()
announce_done("smoke_test_login")
print("=== done ===")
