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
from mud_test_utils import send_line, recv_all, check, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

announce("smoke_test_name_case", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))


def create_account_and_character(tag, typed_name):
    """Registers a fresh account and creates one character with the given
    (as-typed) name. Returns the connected, still-playing socket."""
    account = f"CaseTester{tag}{_suffix}"
    pw = "casetestpw123"
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, account)
    recv_all(s)
    send_line(s, "y")
    recv_all(s)
    send_line(s, pw)
    recv_all(s)
    send_line(s, pw)  # confirm password (Session 21)
    recv_all(s)
    send_line(s, "new")
    recv_all(s)
    send_line(s, typed_name)
    recv_all(s)
    send_line(s, "1"); recv_all(s)  # race: human (zero stat modifier)
    send_line(s, "1"); recv_all(s)  # territory: urban
    send_line(s, "1"); recv_all(s)  # class: mage
    send_line(s, "done")
    recv_all(s)
    send_line(s, "done"); recv_all(s)  # alignment: neutral
    return s


# All-lowercase input -> "Lowerguy".
lower_name = f"lowerguy{_suffix}"
expected_lower = f"Lowerguy{_suffix}"
sLower = create_account_and_character("Lower", lower_name)

send_line(sLower, "score")
out = recv_all(sLower)
check(f"Name: {expected_lower}" in out, "all-lowercase input is capitalized in score's header")
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
check(f"Name: {expected_upper}" in out, "all-caps input is capitalized, not left all-caps, in score")

# Mixed-case input -> "Mixedguy".
mixed_name = f"MiXeDgUy{_suffix}"
expected_mixed = f"Mixedguy{_suffix}"
sMixed = create_account_and_character("Mixed", mixed_name)

send_line(sMixed, "score")
out = recv_all(sMixed)
check(f"Name: {expected_mixed}" in out, "mixed-case input is normalized to proper case in score")

# `look` shows another player's name in proper case in the room listing --
# Upper and Mixed both land in the same default room on creation.
send_line(sMixed, "look")
out = recv_all(sMixed)
check(f"{expected_upper} is here" in out,
      "look shows another player's name in proper case in the room listing")

# --- Name validation (Session 20): 3-15 letters only, same rule as the
# original's _parse_name_safe(). Each bad name is rejected and re-prompted;
# a good name afterwards still works on the same connection. ---
sVal = socket.create_connection((host, port), timeout=5)
recv_all(sVal)
send_line(sVal, f"CaseTesterVal{_suffix}")
recv_all(sVal)
send_line(sVal, "y")
recv_all(sVal)
send_line(sVal, "casetestpw123")
recv_all(sVal)
send_line(sVal, "casetestpw123")  # confirm password (Session 21)
recv_all(sVal)
send_line(sVal, "new")
recv_all(sVal)

for bad, expected, why in [
    (f"Bad{_suffix}123", "only contain letters", "a name containing digits is rejected"),
    ("Bad guy", "only contain letters", "a name containing a space is rejected"),
    ("Bad-guy", "only contain letters", "a name containing punctuation is rejected"),
    ("Ab", "too short", "a 2-letter name is rejected (minimum is 3)"),
    ("Toolongofanamexx", "too long", "a 16-letter name is rejected (maximum is 15)"),
]:
    send_line(sVal, bad)
    out = recv_all(sVal)
    check(expected in out, why)

valid_name = f"Goodguy{_suffix}"
send_line(sVal, valid_name)
recv_all(sVal)
send_line(sVal, "1"); recv_all(sVal)  # race: human (zero stat modifier)
send_line(sVal, "1"); recv_all(sVal)  # territory: urban
send_line(sVal, "1"); recv_all(sVal)  # class: mage
send_line(sVal, "done")
recv_all(sVal)
send_line(sVal, "done"); out = recv_all(sVal)  # alignment: neutral
check(f"Welcome, {valid_name.capitalize()}" in out,
      "a valid letters-only name still creates fine after rejections")
sVal.close()

# --- Duplicate names are rejected across ALL accounts (Session 21) ---
sDup = socket.create_connection((host, port), timeout=5)
greeting = recv_all(sDup)
check("TobinMUD" in greeting, "the connect banner announces TobinMUD")
send_line(sDup, f"CaseTesterDup{_suffix}")
recv_all(sDup)
send_line(sDup, "y")
recv_all(sDup)
send_line(sDup, "casetestpw123")
recv_all(sDup)
send_line(sDup, "casetestpw123")  # confirm password (Session 21)
recv_all(sDup)
send_line(sDup, "new")
recv_all(sDup)
send_line(sDup, valid_name)  # taken by the OTHER account just above
out = recv_all(sDup)
check("already taken" in out, "a duplicate character name is rejected (cross-account)")
send_line(sDup, f"Freshguy{_suffix}")
recv_all(sDup)
send_line(sDup, "1"); recv_all(sDup)  # race: human (zero stat modifier)
send_line(sDup, "1"); recv_all(sDup)  # territory: urban
send_line(sDup, "1"); recv_all(sDup)  # class: mage
send_line(sDup, "done")
recv_all(sDup)
send_line(sDup, "done"); out = recv_all(sDup)  # alignment: neutral
check("Welcome, Freshguy" in out, "a unique name still creates after the rejection")
sDup.close()

sLower.close()
sUpper.close()
sMixed.close()
announce_done("smoke_test_name_case", host, port)
print("=== ALL CHECKS PASSED ===")
