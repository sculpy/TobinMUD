#!/usr/bin/env python3
"""Smoke test for deleting an entire account from the account menu (user,
2026-07-10: "add a delete option to delete account from the account menu,
requires user password to delete account"). Mirrors the existing
character-delete flow (Y-E-S then account password reconfirmation) one
level up: `X` (or `delete account`) at the account menu deletes the WHOLE
account, cascading to every character on it (player.account_id has an ON
DELETE CASCADE FK to account).

  1. The menu lists the new X option.
  2. X prompts for confirmation, naming the account and character count.
  3. Anything but YES cancels -- the account still exists afterward.
  4. YES then a WRONG password cancels -- the account still exists.
  5. YES then the CORRECT password deletes the account and disconnects.
  6. A fresh reconnect with the same account name is now unrecognized
     ("New account" prompt) -- proving both characters and the account
     row itself are actually gone, not just hidden.

    python3 tests/smoke_test_account_delete.py [host] [port]
"""
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


announce("smoke_test_account_delete", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
account_name = f"Delacct{_suffix}"
char1_name = f"Delchar{_suffix}"
char2_name = f"Delchtwo{_suffix}"
password = "delaccttestpw123"


def step(sock, label, line, timeout=1.0):
    send_line(sock, line)
    out = recv_all(sock, timeout)
    print(f"=== {label} ===")
    print(out)
    return out


# --- Set up: a fresh account with two characters ---
s = socket.create_connection((host, port), timeout=5)
recv_all(s)
step(s, "account name (new)", account_name)
step(s, "confirm new account", "y")
step(s, "password (first entry)", password)
step(s, "confirm password -> color prompt", password)
step(s, "answer color prompt -> time zone prompt", "y")
out = step(s, "answer time zone prompt -> account menu", "")
check("X" in out, "the account menu lists the X (delete account) option")

step(s, "create first character", "new")
step(s, "first character name -> race screen", char1_name)
step(s, "race: human", "1")
step(s, "territory: urban", "1")
step(s, "class: mage -> attr screen", "1")
step(s, "finish creation", "done")
step(s, "alignment: neutral", "done")
step(s, "back to menu", "quit!")  # leave-to-menu (playing quit!), not disconnect
s.close()

s = socket.create_connection((host, port), timeout=5)
recv_all(s)
step(s, "account name (existing)", account_name)
out = step(s, "password -> menu", password)
check(char1_name in out, "first character present before the second is created")
step(s, "create second character", "new")
step(s, "second character name -> race screen", char2_name)
step(s, "race: human", "1")
step(s, "territory: urban", "1")
step(s, "class: mage -> attr screen", "1")
step(s, "finish creation", "done")
step(s, "alignment: neutral", "done")

# --- Part 1: cancelling doesn't delete anything ---
out = step(s, "leave to menu", "quit!")
out = step(s, "X to start account deletion", "x")
# account names are stored/reloaded lowercased (account_repo.c), so compare
# case-insensitively on both sides.
check(f"account '{account_name.lower()}'" in out.lower(), "the confirmation prompt names the account")
check("2 character(s)" in out, "the confirmation prompt names the character count")

out = step(s, "cancel (not YES)", "no thanks")
check("Cancelled" in out, "anything but YES cancels the deletion")
check(char1_name in out and char2_name in out, "both characters still listed -- nothing was deleted")

# --- Part 2: YES + wrong password cancels ---
step(s, "X again", "x")
out = step(s, "confirm YES", "YES")
check("account password" in out.lower(), "YES prompts for the account password")
out = step(s, "wrong password", "not-the-real-password")
check("Incorrect password" in out, "a wrong password cancels the deletion")
check(char1_name in out, "the account survives a wrong-password attempt")

# --- Part 3: YES + correct password actually deletes and disconnects ---
step(s, "X once more", "x")
step(s, "confirm YES", "YES")
out = step(s, "correct password -> deleted", password, timeout=2.0)
check("account has been deleted" in out.lower(), "the correct password completes the deletion")

# The connection should now be closed by the server.
closed = False
try:
    more = s.recv(4096)
    closed = (more == b"")
except (socket.timeout, ConnectionResetError, OSError):
    closed = True
check(closed, "the connection is closed after account deletion")
s.close()

# --- Part 4: the account is truly gone -- a fresh connection sees it as new ---
s2 = socket.create_connection((host, port), timeout=5)
recv_all(s2)
out = step(s2, "reconnect with the deleted account name", account_name)
check("new account" in out.lower(), "the deleted account name is unrecognized -- treated as brand new")
s2.close()

announce_done("smoke_test_account_delete", host, port)
print("=== ALL CHECKS PASSED ===")
