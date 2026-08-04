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
from mud_test_utils import send_line, recv_all, check, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

announce("smoke_test_say", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))


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
    send_line(s, "1"); recv_all(s)  # race: human (zero stat modifier)
    send_line(s, "1"); recv_all(s)  # territory: urban
    send_line(s, "1"); recv_all(s)  # class: mage
    send_line(s, "done")
    recv_all(s)
    send_line(s, "done"); recv_all(s)  # alignment: neutral
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
announce_done("smoke_test_say", host, port)
print("=== ALL CHECKS PASSED ===")
