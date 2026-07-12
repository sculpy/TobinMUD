#!/usr/bin/env python3
"""Smoke test for player titles (cmd_title.c) and who arguments (cmd_who.c):
  1. `title the Brave` echoes and makes who show "<Name> the Brave".
  2. The title persists across a reconnect (it's stored in player.title).
  3. `title none` clears it; who no longer shows it.
  4. `who <substring>` filters the list by name; a non-matching filter shows
     the "No one matching" line.
  5. `who mortals` includes a mortal; `who immortals` excludes them.

    python3 tests/smoke_test_title.py [host] [port]
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


announce("smoke_test_title")

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


def make_char(nm):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, nm); recv_all(s)
    send_line(s, "y"); recv_all(s)
    send_line(s, "titpw"); recv_all(s)
    send_line(s, "titpw"); recv_all(s)
    send_line(s, "new"); recv_all(s)
    send_line(s, nm); recv_all(s)
    send_line(s, "done"); recv_all(s)
    send_line(s, "1"); recv_all(s)  # race: human (zero stat modifier)
    send_line(s, "1"); recv_all(s)  # class: mage
    send_line(s, "2"); recv_all(s)  # alignment: neutral
    return s


def relogin(nm):
    r = socket.create_connection((host, port), timeout=5)
    recv_all(r)
    send_line(r, nm); recv_all(r)
    send_line(r, "titpw"); recv_all(r)
    send_line(r, "1"); recv_all(r)
    return r


nameA = f"Tita{_suffix}"
s = make_char(nameA)

out = cmd(s, "title the Brave")
check("Title set to: the Brave" in out, "title command echoes the new title")

out = cmd(s, "who")
check(f"{nameA} the Brave" in out, "who shows the name followed by the title")

# Persistence: reconnect and confirm the title survived.
s.close()
s = relogin(nameA)
check(f"{nameA} the Brave" in cmd(s, "who"), "title persists across a reconnect")

out = cmd(s, "title none")
check("cleared" in out.lower(), "title none clears the title")
out = cmd(s, "who")
check(f"{nameA} the Brave" not in out and nameA in out,
      "who no longer shows the title but still lists the character")

# who arguments -----------------------------------------------------------
check(nameA in cmd(s, f"who {nameA[:4]}"), "who <substring> matches the name")
check("No one matching" in cmd(s, "who zzzznope"),
      "who with a non-matching filter reports no matches")
check(nameA in cmd(s, "who mortals"), "who mortals lists a mortal character")
check(nameA not in cmd(s, "who immortals"),
      "who immortals excludes a mortal character")

# <N> substitution: the name is embedded anywhere in the title, and the
# separate name is NOT shown (the title stands alone).
cmd(s, "title <N> really is out to get you!")
out = cmd(s, "who")
check(f"{nameA} really is out to get you!" in out,
      "who substitutes <N> with the character's name inside the title")
# The name should appear exactly once on that row (embedded), not twice.
row = [ln for ln in out.splitlines() if "really is out to get you" in ln]
check(row and row[0].count(nameA) == 1,
      "with <N>, the name is shown once (embedded), not duplicated")

s.close()
announce_done("smoke_test_title")
print("=== ALL CHECKS PASSED ===")
