#!/usr/bin/env python3
"""Smoke test: deleting a character that has a linkdead body still
resident in the world cleans up that in-memory being_t too, not just the
DB row.

Reproduces the scenario found while testing smoke_test_accounts.py: a
character disconnects via a raw socket close (never `quit!`), leaving a
linkdead body standing in its room (descriptor_destroy()'s own documented
behavior). Deleting that character from another connection used to only
run a DB DELETE (player_delete(), player_repo.c) -- the orphaned being_t
kept appearing in room listings forever, now pointing at a player_id that
no longer exists. Fixed in descriptor.c's CONN_CHAR_DELETE_PASSWORD
handler: looks up world_find_linkdead_pc() before the DB row goes away and
destroys it too.

    python3 tests/smoke_test_delete_linkdead.py [host] [port]
"""
import socket
import sys
import time

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


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


announce("smoke_test_delete_linkdead")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
account_name = f"Dlacct{_suffix}"
char_name = f"Dlchar{_suffix}"
password = "dlinkdeadpw123"


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


# --- Create the character, then disconnect via raw socket close (NOT
# quit!) -- leaves a linkdead body standing in its room. ---
s = socket.create_connection((host, port), timeout=5)
recv_all(s)
send_line(s, account_name); recv_all(s)
send_line(s, "y"); recv_all(s)
send_line(s, password); recv_all(s)
send_line(s, password); recv_all(s)
send_line(s, "y"); recv_all(s)   # color prompt
send_line(s, ""); recv_all(s)    # time zone prompt -> account menu
send_line(s, "new"); recv_all(s)
send_line(s, char_name); recv_all(s)
send_line(s, "1"); recv_all(s)   # race: human
send_line(s, "1"); recv_all(s)   # class: mage
send_line(s, "done"); recv_all(s)  # attrs done -> options menu
out = cmd(s, "done")             # options done -> playing
check("Welcome" in out, "character created and playing")
s.close()  # raw close, no quit! -- leaves a linkdead body in the room


# --- From a second connection, delete char_name directly by name --
# deliberately never reconnecting to it first (that would clean up the
# linkdead body via enter_world()'s own recovery path, defeating the
# reproduction). ---
s2 = socket.create_connection((host, port), timeout=5)
recv_all(s2)
send_line(s2, account_name); recv_all(s2)
cmd(s2, password)
out = cmd(s2, f"delete {char_name}")
check(f"Really delete '{char_name}'" in out, "delete prompts for confirmation")
out = cmd(s2, "YES")
check("Enter your account password" in out, "YES prompts for account password reconfirmation")
out = cmd(s2, password)
check("Character deleted" in out, "deletion succeeds")


# --- Confirm the linkdead body is actually gone from the world, not just
# the DB row: recreate a DIFFERENT character in the same starting room and
# look around -- the deleted character's linkdead body must not appear. ---
other_name = f"Dlwitness{_suffix}"
s3 = socket.create_connection((host, port), timeout=5)
recv_all(s3)
send_line(s3, f"{account_name}w"); recv_all(s3)
send_line(s3, "y"); recv_all(s3)
send_line(s3, password); recv_all(s3)
send_line(s3, password); recv_all(s3)
send_line(s3, "y"); recv_all(s3)
send_line(s3, ""); recv_all(s3)
send_line(s3, "new"); recv_all(s3)
send_line(s3, other_name); recv_all(s3)
send_line(s3, "1"); recv_all(s3)
send_line(s3, "1"); recv_all(s3)
send_line(s3, "done"); recv_all(s3)
cmd(s3, "done")  # playing, auto-look already ran

out = cmd(s3, "look")
check(char_name not in out, f"{char_name}'s linkdead body is gone from the room after deletion")

s2.close()
s3.close()
announce_done("smoke_test_delete_linkdead")
print("=== ALL CHECKS PASSED ===")
