#!/usr/bin/env python3
"""Smoke test for character-name proper-casing: a name typed in any case at
creation (all-lowercase, all-caps, mixed) is normalized to "first letter
uppercase, rest lowercase" before it's stored, so every later display
(the account menu, `score`, `who`, `look`'s room-occupant listing) shows a
consistently-cased name.

    python3 tests/smoke_test_name_case.py [host] [port]
"""
import socket
import sys
import time

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000
_suffix = str(int(time.time()) % 100000)


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


def create_account_and_character(tag, typed_name):
    """Registers a fresh account and creates one character with the given
    (as-typed) name. Returns the connected, still-playing socket."""
    account = f"CaseTester{tag}{_suffix}"
    pw = "casetestpw123"
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, account)
    recv_all(s)
    send_line(s, pw)
    recv_all(s)
    send_line(s, "new")
    recv_all(s)
    send_line(s, typed_name)
    recv_all(s)
    send_line(s, "done")
    recv_all(s)
    return s


# All-lowercase input -> "Lowerguy".
lower_name = f"lowerguy{_suffix}"
expected_lower = f"Lowerguy{_suffix}"
sLower = create_account_and_character("Lower", lower_name)

send_line(sLower, "score")
out = recv_all(sLower)
check(f"-- {expected_lower} --" in out, "all-lowercase input is capitalized in score's header")
check(lower_name not in out, "the raw lowercase form does not appear in score")

send_line(sLower, "who")
out = recv_all(sLower)
check(expected_lower in out, "all-lowercase input is capitalized in who")

# ALL-CAPS input -> "Upperguy" (not left as "UPPERGUY").
upper_name = f"UPPERGUY{_suffix}"
expected_upper = f"Upperguy{_suffix}"
sUpper = create_account_and_character("Upper", upper_name)

send_line(sUpper, "score")
out = recv_all(sUpper)
check(f"-- {expected_upper} --" in out, "all-caps input is capitalized, not left all-caps, in score")

# Mixed-case input -> "Mixedguy".
mixed_name = f"MiXeDgUy{_suffix}"
expected_mixed = f"Mixedguy{_suffix}"
sMixed = create_account_and_character("Mixed", mixed_name)

send_line(sMixed, "score")
out = recv_all(sMixed)
check(f"-- {expected_mixed} --" in out, "mixed-case input is normalized to proper case in score")

# `look` shows another player's name in proper case in the room listing --
# Upper and Mixed both land in the same default room on creation.
send_line(sMixed, "look")
out = recv_all(sMixed)
check(f"{expected_upper} is here" in out,
      "look shows another player's name in proper case in the room listing")

sLower.close()
sUpper.close()
sMixed.close()
print("=== ALL CHECKS PASSED ===")
