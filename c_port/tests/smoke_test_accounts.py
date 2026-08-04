#!/usr/bin/env python3
"""Smoke test for the account/character menu, point-buy attribute dialog,
and character deletion. Drives a raw TCP session through:

  1. new account -> empty character menu -> "new" -> point-buy attrs -> play
  2. `score` shows the allocated attributes
  3. disconnect + reconnect -> existing account, menu shows the character
  4. create a SECOND character on the same account
  5. delete the first character with the YES confirmation
  6. re-check the menu reflects the deletion

Each server reply to a sent line typically arrives in one TCP burst well
within the read timeout, so every step() call both sends a line AND reads
the full reply to it -- there's no separate "read more" step.

    python3 tests/smoke_test_accounts.py [host] [port] [account_name]
"""
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

announce("smoke_test_accounts", host, port)

# Unique per run so re-running the script doesn't collide with a leftover
# account/character from a previous (possibly failed) run -- character
# names are globally unique in the DB, not just per-account, so these need
# the same run-unique suffix as the account name, not hardcoded literals.
_suffix = sys.argv[3] if len(sys.argv) > 3 else "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
account_name = f"AttrTester{_suffix}"
char1_name = f"Attrius{_suffix}"
char2_name = f"Secondus{_suffix}"
password = "attrtestpw123"


def step(sock, label, line):
    """Sends `line`, reads the full reply, prints it, and returns it."""
    send_line(sock, line)
    out = recv_all(sock)
    print(f"=== {label} ===")
    print(out)
    return out


def player_name_exists_in_db(name):
    out = subprocess.run(["mariadb", "tobin", "-N", "-e",
                          f"select 1 from player where name='{name}';"],
                         check=True, capture_output=True, text=True).stdout
    return out.strip() == "1"


print(f"Using account name: {account_name}")

# --- Session 1: new account, new character, point-buy, play, score ---
s = socket.create_connection((host, port), timeout=5)
recv_all(s)  # banner + "Account name: " prompt
step(s, "account name (new)", account_name)
step(s, "confirm new account", "y")
step(s, "password (first entry)", password)
out = step(s, "MISMATCHED confirmation", password + "x")
check("do not match" in out, "a mismatched confirmation re-prompts for the password")
step(s, "password again", password)
out = step(s, "confirm password -> color prompt", password)
check("color" in out.lower(), "a new account is asked about color after the password")
out = step(s, "answer color prompt -> time zone prompt", "y")
check("time zone" in out.lower(), "a new account is asked about a time zone after color")
out = step(s, "answer time zone prompt -> account menu", "")
check("(none yet)" in out, "brand-new account starts with an empty character list")

step(s, "choose 'new'", "new")
step(s, "character name -> race screen", char1_name)
step(s, "race: human", "1")
step(s, "territory: urban", "1")
out = step(s, "class: mage -> attr screen (defaults)", "1")
check("Str: 120" in out, "attributes start at ATTR_BASE (120)")
check("Points remaining: 30" in out, "full pool (30) available before spending")

# Each attribute is capped at +/-30 from base (120), and the net pool is also
# 30 -- so a single attribute at its cap exactly exhausts the pool. See
# smoke_test_trade_attrs.py for exhaustive cap/trade coverage; this just
# exercises a normal allocation plus one pool-overspend rejection as part
# of the broader creation flow.
out = step(s, "allocate strength (exhausts the pool)", "str 30")
check("Points remaining: 0" in out, "strength at +30 exactly spends the 30-point pool")

out = step(s, "overspend rejected", "dex 5")
check("Not enough points remaining" in out, "spending more than what's left is rejected")

step(s, "attrs done -> the options menu", "done")

# Handedness (Session 21, moved into the options menu 2026-07-26): optional
# choice, default right.
step(s, "options menu -> handedness sub-menu", "1")
out = step(s, "pick left", "1")
check("Handedness: Left" in out, "picking left flips the options menu's handedness line")

out = step(s, "options menu done -> playing", "done")
check(f"Welcome, {char1_name}" in out, "'done' creates the character and enters the world")

out = step(s, "score", "score")
check("Pri. Hand: Left" in out, "score shows the chosen left-handedness")
check("Str: 143" in out and "Dex: 120" in out,
      "score shows the persisted point-buy allocation (150 str -4 for the Mage class, -3 for the Urban homeland chosen at creation)")

s.close()

# --- Session 2: reconnect, existing account, menu shows the character ---
s2 = socket.create_connection((host, port), timeout=5)
recv_all(s2)
step(s2, "account name (existing)", account_name)
out = step(s2, "password (existing account) -> menu (hidden list)", password)
# The character list is hidden by default (Session 47) until 'C'; with
# only one character, bare 'c' auto-connects instead of revealing a list
# (see show_account_menu()) -- the multi-character reveal itself is
# already covered below, once char2 exists (Session 3).
check("(none yet)" not in out, "the account menu recognizes the existing character (list just hidden)")

step(s2, "create second character", "new")
step(s2, "second character name -> race screen", char2_name)
step(s2, "race: human", "1")
step(s2, "territory: urban", "1")
step(s2, "class: mage -> attr screen", "1")
step(s2, "accept defaults, finish", "done")
out = step(s2, "alignment: neutral", "done")
check(f"Welcome, {char2_name}" in out, "second character created with default (unallocated) attrs")

out = step(s2, "score for second character", "score")
check("Str: 113" in out, "second character kept the ATTR_BASE defaults (no allocation made; "
      "120 -4 for the Mage class, -3 for the Urban homeland chosen at creation)")

s2.close()

# --- Session 3: reconnect again, delete char1, confirm menu updates ---
s3 = socket.create_connection((host, port), timeout=5)
recv_all(s3)
step(s3, "account name", account_name)
step(s3, "password -> menu (hidden list)", password)
out = step(s3, "reveal the list (2 characters now)", "c")
check(char1_name in out and char2_name in out, "both characters listed before deletion")

out = step(s3, f"delete {char1_name}", f"delete {char1_name}")
check(f"Really delete '{char1_name}'" in out, "delete prompts for confirmation")

out = step(s3, "confirm YES", "YES")
check("Enter your account password" in out, "YES now prompts for account password reconfirmation")

out = step(s3, "wrong account password", "not-the-real-password")
check("Incorrect password" in out, "a wrong password cancels the deletion")
check(char1_name in out, f"{char1_name} still shows up in the menu -- it was NOT deleted")

out = step(s3, f"delete {char1_name} again", f"delete {char1_name}")
step(s3, "confirm YES again", "YES")
out = step(s3, "correct account password", password)
check("Character deleted" in out, "confirmed deletion with the correct password succeeds")
check(char2_name in out, "menu redisplay after delete still lists the other character")

s3.close()

# --- Session 4: final check -- char1 truly gone, char2 remains ---
s4 = socket.create_connection((host, port), timeout=5)
recv_all(s4)
step(s4, "account name", account_name)
step(s4, "password -> menu (hidden list, only char2 left)", password)
# Only one character left now -- bare 'c' auto-connects (see
# show_account_menu()'s char_count==1 case) rather than listing.
out = step(s4, "connect (auto, only one character left)", "c")
check(f"Welcome back, {char2_name}" in out or char2_name in out,
      f"{char2_name} survived the deletion of {char1_name}")
# Not asserting char1_name's absence from the room-floor text here: an
# earlier session in this same script (s.close()) disconnected char1
# WITHOUT quitting, leaving a linkdead body resident in the world --
# deleting the player row (player_delete(), player_repo.c) is DB-only and
# doesn't clean up an already-in-memory linkdead being_t. That's a real,
# separate latent gap (flagged via spawn_task), not something to paper
# over with a weaker assertion here.
check(not player_name_exists_in_db(char1_name), f"{char1_name}'s player row is gone from the DB")

s4.close()

announce_done("smoke_test_accounts", host, port)
print("=== ALL CHECKS PASSED ===")
