#!/usr/bin/env python3
"""Smoke test for cancelling character creation via `quit!`, from both the
name-entry step and the attribute point-buy step. In both cases: no
character should actually get created, the connection stays open (you land
back at the account menu), and creating a real character afterward still
works fine. Also confirms bare "quit" (no bang) does nothing at either step.

    python3 tests/smoke_test_quit_creation.py [host] [port]
"""
import socket
import sys
import time

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000
_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
account_name = f"QCTester{_suffix}"
abandoned_name = f"Abandoned{_suffix}"
abandoned_name2 = f"Abandonedb{_suffix}"
real_name = f"RealChar{_suffix}"
# The server normalizes character names to proper case at creation (first
# letter uppercase, rest lowercase) -- see being_normalize_name() -- so the
# displayed form differs from the as-typed real_name (mid-word capital C).
real_name_display = real_name[:1].upper() + real_name[1:].lower()
password = "qctestpw123"


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
    send_line(sock, line)
    out = recv_all(sock)
    print(f"=== {label} ===")
    print(out)
    return out


def check(condition, message):
    if not condition:
        raise AssertionError(message)
    print(f">>> OK: {message}")


s = socket.create_connection((host, port), timeout=5)
recv_all(s)
step(s, "account name", account_name)
step(s, "password -> menu", password)

# --- Cancel right at the name-entry step ---
out = step(s, "choose 'new'", "new")
check("or 'quit!' to cancel" in out, "name prompt mentions the quit! option")

out = step(s, "bare 'quit' at the name prompt should NOT cancel", "quit")
check("Character creation cancelled" not in out, "bare 'quit' did not cancel creation")

out = step(s, "type a name, then move to the attribute screen", abandoned_name)
check("Allocate attributes" in out, "moved on to the attribute screen")

out = step(s, "bare 'quit' at the attribute screen should NOT cancel", "quit")
check("Character creation cancelled" not in out, "bare 'quit' did not cancel creation here either")

out = step(s, "quit! from the attribute screen", "quit!")
check("Character creation cancelled" in out, "cancellation message shown")
check("Your characters" in out, "back at the account menu")
check("(none yet)" in out, "no character was created -- menu is still empty")

# --- Cancel again, this time right at the name prompt itself (before any attrs step) ---
step(s, "choose 'new' again", "new")
out = step(s, "quit! immediately at the name prompt", "quit!")
check("Character creation cancelled" in out, "cancellation message shown at the name step")
check("(none yet)" in out, "still no character created")

# --- Cancel a THIRD time, but after actually allocating some points ---
step(s, "choose 'new' a third time", "new")
step(s, "name it", abandoned_name2)
step(s, "allocate some points", "str 20")
out = step(s, "quit! after allocating -- should still discard everything", "quit!")
check("Character creation cancelled" in out, "cancellation works even mid-allocation")
check("(none yet)" in out, "allocated-but-uncommitted character was never persisted")

# --- Now actually finish creating a real character, to confirm the flow still works ---
step(s, "choose 'new' for real this time", "new")
step(s, "real name", real_name)
step(s, "allocate", "str 15")
out = step(s, "finish for real", "done")
check(f"Welcome, {real_name_display}" in out, "a real character can still be created after two cancellations")

s.close()
print("=== ALL CHECKS PASSED ===")
