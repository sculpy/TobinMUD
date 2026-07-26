#!/usr/bin/env python3
"""Smoke test for three related `look` bugs found and fixed together
(Session 43 continued), all against the real seeded mob vnum 33271 ("a
dirty refuse hauler", Center Square) rather than synthetic fixtures,
since all three bugs only manifest on real authored content:

  1. Capitalization bug (user: "sometimes in look the proper
     capitalization is ignored, fix this"). Root cause: this mob's
     short_desc is authored with a leading inline color tag
     ("<o>a dirty refuse hauler<1>"), and cap_first() (cmd_look.c) used
     to blindly uppercase byte 0 -- which was '<', not the real letter.
     The room-floor listing must show "A dirty refuse hauler is here."
     (capitalized), not "a dirty refuse hauler is here."
  2. Wrong display name in `look <mob>` (user: "You look at man dirty
     refuse hauler. should read You look at a dirty refuse hauler.").
     Root cause: look_at_target() used thing_t.name (the raw keyword-
     match list, "man dirty refuse hauler") instead of short_descr.
     Must read "You look at a dirty refuse hauler." -- lowercase, since
     it's mid-sentence (not cap_first()'d).
  3. Truncated long description (user: "increase the buffer size so i
     can read the entire string"). Root cause: BEING_APPEARANCE_LEN was
     256 (sized for player.appearance's real varchar(255) column), but
     mob.description is mediumtext and this mob's real description runs
     well past 256 chars -- silently cut off mid-sentence on load
     (mob_repo.c). Bumped to 2048. The full closing sentence ("A more
     pungent ogre could not be found.") must survive.

    python3 tests/smoke_test_look_capitalization.py [host] [port]
"""
import socket
import subprocess
import sys
import time

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000
MOB_VNUM = 33271  # "a dirty refuse hauler" -- real seeded content, Center Square


def announce(test_name, host=host, port=port):
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
    announce(f"done {test_name}", host, port)


announce("smoke_test_look_capitalization")

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


def cmd(sock, line, timeout=1.0):
    send_line(sock, line)
    return recv_all(sock, timeout)


def check(condition, message):
    if not condition:
        raise AssertionError(message)
    print(f">>> OK: {message}")


def sql(stmt):
    subprocess.run(["mariadb", "tobin", "-e", stmt], check=True)


name = f"Capz{_suffix}"
pw = "capzpw123"

s = socket.create_connection((host, port), timeout=5)
recv_all(s)
send_line(s, name); recv_all(s)
send_line(s, "y"); recv_all(s)
send_line(s, pw); recv_all(s)
send_line(s, pw); recv_all(s)
send_line(s, ""); recv_all(s)   # color default
send_line(s, ""); recv_all(s)   # timezone default
send_line(s, "new"); recv_all(s)
send_line(s, name); recv_all(s)
cmd(s, "1")  # race: human (zero stat modifier)
cmd(s, "1")  # class: mage
cmd(s, "done")
cmd(s, "done")  # alignment: neutral
sql(f"UPDATE player_progress SET level=52 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{name}');")  # BUILD_MIN_LEVEL, for `load`
s.close()

s = socket.create_connection((host, port), timeout=5)
recv_all(s)
send_line(s, name); recv_all(s)
send_line(s, pw); recv_all(s)
send_line(s, "1"); recv_all(s)
cmd(s, "color off")

out = cmd(s, f"load mob {MOB_VNUM}")
check("You conjure" in out, f"the test mob (vnum {MOB_VNUM}) loads")

# --- 1: capitalization in the room-floor listing ---
out = cmd(s, "look")
check("a dirty refuse hauler is here" not in out,
      "no lowercase leak in the room listing (tag-prefixed short_desc)")
check("A dirty refuse hauler is here." in out,
      "the mob's room-listing line is properly capitalized despite its leading color tag")

# --- 2: `look <mob>` shows the short_descr, mid-sentence (lowercase) ---
out = cmd(s, "look hauler")
check("You look at man dirty refuse hauler" not in out,
      "the raw keyword-match list no longer leaks into the display name")
check("You look at a dirty refuse hauler." in out,
      "`look <mob>` shows the short_descr, correctly lowercase mid-sentence")

# --- 3: the full (long) description is not truncated ---
check("A more pungent ogre could not be found." in out,
      "the mob's full description survives -- was truncated mid-sentence before "
      "the BEING_APPEARANCE_LEN bump")

s.close()
announce_done("smoke_test_look_capitalization")
print("=== ALL CHECKS PASSED ===")
