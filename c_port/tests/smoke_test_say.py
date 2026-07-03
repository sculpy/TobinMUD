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
    send_line(s, pw)
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
send_line(sA, "say Hello there, everyone!")
outA = recv_all(sA)
outB = recv_all(sB)
check('You say, "Hello there, everyone!"' in outA, "the speaker sees their own message with 'You say,'")
check(f'{proper(nameA)} says, "Hello there, everyone!"' in outB,
      "the other player in the room sees '<Name> says,' with the same message")

# --- Part 2: `'` shorthand, no space required ---
send_line(sB, "'Hi back at you!")
outA = recv_all(sA)
outB = recv_all(sB)
check('You say, "Hi back at you!"' in outB, "the ' shorthand shows the speaker their own message")
check(f'{proper(nameB)} says, "Hi back at you!"' in outA,
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
print("=== ALL CHECKS PASSED ===")
