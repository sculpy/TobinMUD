#!/usr/bin/env python3
"""Smoke test for `say` and its `'` one-character shorthand:
  1. `say <message>` shows the speaker "You say, "<message>"" and shows
     everyone else in the room "<Name> says, "<message>"".
  2. `'<message>` (no space required) does the exact same thing.
  3. An empty `say`/`'` is rejected with a "WHAT do you want to say?" hint,
     not silently ignored or crashing.

    python3 tests/smoke_test_say.py [host] [port]
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


announce("smoke_test_say")

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


def proper(name):
    return name[:1].upper() + name[1:].lower()


def make_player(tag):
    name = f"Say{tag}{_suffix}"
    pw = "saytestpw123"
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name)
    recv_all(s)
    send_line(s, "y")
    recv_all(s)
    send_line(s, pw)
    recv_all(s)
    send_line(s, pw)  # confirm password (Session 21)
    recv_all(s)
    send_line(s, "new")
    recv_all(s)
    send_line(s, name)
    recv_all(s)
    send_line(s, "done")
    recv_all(s)
    return s, name


sA, nameA = make_player("A")
sB, nameB = make_player("B")

# --- Part 1: `say` ---
# The say wrapper is cyan (Session 21): ANSI escapes sit between the
# framing and the message, so check the pieces, not one literal string.
send_line(sA, "say Hello there, everyone!")
outA = recv_all(sA)
outB = recv_all(sB)
check('You say, "' in outA and 'Hello there, everyone!' in outA,
      "the speaker sees their own message with 'You say,'")
check(f'{proper(nameA)} says, "' in outB and 'Hello there, everyone!' in outB,
      "the other player in the room sees '<Name> says,' with the same message")
check("\x1b[0;36m" in outB, "the say framing arrives cyan")

# --- Part 2: `'` shorthand, no space required ---
send_line(sB, "'Hi back at you!")
outA = recv_all(sA)
outB = recv_all(sB)
check('You say, "' in outB and 'Hi back at you!' in outB,
      "the ' shorthand shows the speaker their own message")
check(f'{proper(nameB)} says, "' in outA and 'Hi back at you!' in outA,
      "the ' shorthand reaches the other player in the room with the same message")

# --- Part 3: empty say is rejected, not silently ignored ---
out = recv_all(sA)  # drain
send_line(sA, "say")
out = recv_all(sA)
check("WHAT do you want to say" in out, "bare 'say' with no message is rejected with a hint")

send_line(sA, "'")
out = recv_all(sA)
check("WHAT do you want to say" in out, "bare ' with no message is also rejected with a hint")

sA.close()
sB.close()
announce_done("smoke_test_say")
print("=== ALL CHECKS PASSED ===")
