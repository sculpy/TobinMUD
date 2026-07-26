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


announce("smoke_test_accounts")

# Unique per run so re-running the script doesn't collide with a leftover
# account/character from a previous (possibly failed) run -- character
# names are globally unique in the DB, not just per-account, so these need
# the same run-unique suffix as the account name, not hardcoded literals.
_suffix = sys.argv[3] if len(sys.argv) > 3 else "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
account_name = f"AttrTester{_suffix}"
char1_name = f"Attrius{_suffix}"
char2_name = f"Secondus{_suffix}"
password = "attrtestpw123"


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
    """Sends `line`, reads the full reply, prints it, and returns it."""
    send_line(sock, line)
    out = recv_all(sock)
    print(f"=== {label} ===")
    print(out)
    return out


def check(condition, message):
    if not condition:
        raise AssertionError(message)
    print(f">>> OK: {message}")


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
out = step(s, "class: mage -> attr screen (defaults)", "1")
check("Strength:      120" in out, "attributes start at ATTR_BASE (120)")
check("Points remaining: 30" in out, "full pool (30) available before spending")

# Each attribute is capped at +/-30 from base (120), and the net pool is also
# 30 -- so a single attribute at its cap exactly exhausts the pool. See
# smoke_test_trade_attrs.py for exhaustive cap/trade coverage; this just
# exercises a normal allocation plus one pool-overspend rejection as part
# of the broader creation flow.
out = step(s, "allocate strength (exhausts the pool)", "str 30")
check("Points remaining: 0" in out, "strength at +30 exactly spends the 30-point pool")

# Handedness (Session 21): optional choice on this screen, default right.
send_line(s, "hand left")
out = recv_all(s)
check("Handedness:    left" in out, "hand left flips the attr screen's handedness line")

out = step(s, "overspend rejected", "dex 5")
check("Not enough points remaining" in out, "spending more than what's left is rejected")

step(s, "finish creation", "done")
out = step(s, "alignment: neutral", "2")
check(f"Welcome, {char1_name}" in out, "'done' creates the character and enters the world")

out = step(s, "score", "score")
check("Pri. Hand: Left" in out, "score shows the chosen left-handedness")
check("Str: 146" in out and "Dex: 120" in out,
      "score shows the persisted point-buy allocation (150 str -4 for the Mage class chosen at creation)")

s.close()

# --- Session 2: reconnect, existing account, menu shows the character ---
s2 = socket.create_connection((host, port), timeout=5)
recv_all(s2)
step(s2, "account name (existing)", account_name)
out = step(s2, "password (existing account) -> menu", password)
check(char1_name in out, f"{char1_name} shows up in the account menu on a fresh login")
check(f"{char1_name} (Level 1)" in out,
      "the menu lists each character's level next to the name")

step(s2, "create second character", "new")
step(s2, "second character name -> race screen", char2_name)
step(s2, "race: human", "1")
step(s2, "class: mage -> attr screen", "1")
step(s2, "accept defaults, finish", "done")
out = step(s2, "alignment: neutral", "2")
check(f"Welcome, {char2_name}" in out, "second character created with default (unallocated) attrs")

out = step(s2, "score for second character", "score")
check("Str: 116" in out, "second character kept the ATTR_BASE defaults (no allocation made; "
      "120 -4 for the Mage class chosen at creation)")

s2.close()

# --- Session 3: reconnect again, delete char1, confirm menu updates ---
s3 = socket.create_connection((host, port), timeout=5)
recv_all(s3)
step(s3, "account name", account_name)
out = step(s3, "password -> menu (both characters)", password)
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
out = step(s4, "password -> menu (post-delete)", password)
check(char1_name not in out, f"{char1_name} is permanently gone after reconnecting")
check(char2_name in out, f"{char2_name} survived the deletion of {char1_name}")

s4.close()

announce_done("smoke_test_accounts")
print("=== ALL CHECKS PASSED ===")
