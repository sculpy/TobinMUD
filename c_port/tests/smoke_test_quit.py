#!/usr/bin/env python3
"""Smoke test for the two-tier `quit!`, from the `who`-visibility angle:
quitting a character (first `quit!`) returns to the account menu without
disconnecting -- and since you're no longer "in the world", you disappear
from `who` immediately, even though the connection is still open. Quitting
again from the account menu (second `quit!`) is what actually disconnects.
Also confirms the bare word `quit` (no bang) does NOT work anywhere --
quit is deliberately excluded from abbreviation matching, so only the
exact literal "quit!" triggers it.
See also smoke_test_quit_menu.py, which covers the same two-tier behavior
from the connection-state angle (re-entering the character, EOF checks).

    python3 tests/smoke_test_quit.py [host] [port]
"""
import socket
import sys
import time

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000
_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
account_name = f"QuitTester{_suffix}"
char_name = f"Quitter{_suffix}"
password = "quittestpw123"


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


# Log in and create a character.
s = socket.create_connection((host, port), timeout=5)
recv_all(s)
send_line(s, account_name)
recv_all(s)
send_line(s, password)
recv_all(s)
send_line(s, "new")
recv_all(s)
send_line(s, char_name)
recv_all(s)
send_line(s, "done")
out = recv_all(s)
check(f"Welcome, {char_name}" in out, "character created and playing")

# A second connection to observe `who` before/after quitting.
s2 = socket.create_connection((host, port), timeout=5)
recv_all(s2)
send_line(s2, f"{account_name}Observer")
recv_all(s2)
send_line(s2, "observerpw123")
recv_all(s2)
send_line(s2, "new")
recv_all(s2)
send_line(s2, f"Observer{_suffix}")
recv_all(s2)
send_line(s2, "done")
recv_all(s2)

send_line(s2, "who")
out = recv_all(s2)
check(char_name in out, "quitter shows up in who before quitting")

# The bare word "quit" (no bang) must NOT work while playing.
send_line(s, "quit")
out = recv_all(s)
check("Huh?!" in out, "bare 'quit' (no bang) is not recognized while playing")
check("return to the character menu" not in out, "bare 'quit' did not trigger the quit behavior")

# First real quit: returns to the account menu, does NOT disconnect.
send_line(s, "quit!")
out = recv_all(s)
check("return to the character menu" in out, "quit! sends the return-to-menu message")

# The connection should still be open -- confirm by getting a further reply
# (the account menu re-listing) rather than a timeout/EOF.
check("Your characters" in out, "account menu shown, connection still alive")

# Even though still connected, the quitter should vanish from `who` --
# they're no longer "in the world", just parked at the account menu.
time.sleep(0.3)
send_line(s2, "who")
out = recv_all(s2)
check(char_name not in out, "quitter disappears from who after quit! (menu), even though still connected")
check(f"Observer{_suffix}" in out, "the observer itself still shows up")

# Bare "quit" at the account menu must also not work.
send_line(s, "quit")
out = recv_all(s)
check("Goodbye" not in out, "bare 'quit' at the account menu does not disconnect")

# Second quit!, from the account menu: THIS actually disconnects.
send_line(s, "quit!")
out = recv_all(s)
check("Goodbye" in out, "quit! (from the menu) sends a goodbye message")

s.settimeout(2.0)
trailing = s.recv(4096)
check(trailing == b"", "server closed the connection after quit! (EOF, not just silence)")
s.close()

s2.close()
print("=== ALL CHECKS PASSED ===")
