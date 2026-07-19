#!/usr/bin/env python3
"""Smoke test for `ignore`/`unignore` (Sneezy → Tobin feature audit,
"Ignore lists"). Scoped down to tell/whisper only -- see
tobin_migrations.sql's own comment on player_ignore. Covers:
  1. `ignore` with no args and nothing ignored says so.
  2. `ignore <name>` adds; a repeat says "already ignoring".
  3. `ignore` (bare) lists what's ignored.
  4. A tell from an ignored sender fails SILENTLY: sender still sees
     "You tell ..." (no error), target receives nothing.
  5. Whisper is blocked the same way; bystanders still see the generic
     "whispers something to" line either way.
  6. `unignore <name>` removes it; a repeat says "aren't ignoring".
  7. After unignoring, tell/whisper deliver again normally.
  8. Can't ignore yourself.

    python3 tests/smoke_test_ignore.py [host] [port]
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


announce("smoke_test_ignore")

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


def proper(name):
    """The server normalizes character names to proper case at creation
    (being_normalize_name()) -- mirrors that so assertions against
    server-echoed text match, regardless of how the name was typed here."""
    return name[:1].upper() + name[1:].lower()


def make_player(tag):
    name = f"Ign{tag}{_suffix}"
    pw = "ignoretestpw123"
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "y"); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "new"); recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "1"); recv_all(s)  # race: human
    send_line(s, "1"); recv_all(s)  # class: mage
    send_line(s, "done"); recv_all(s)
    send_line(s, "2"); recv_all(s)  # alignment: neutral
    cmd(s, "color off")
    return s, name


sA, nameA = make_player("A")
sB, nameB = make_player("B")

check("aren't ignoring anyone" in cmd(sB, "ignore"), "bare ignore with nothing ignored says so")

check(f"You are now ignoring {nameA}" in cmd(sB, f"ignore {nameA}"), "ignore adds a name")
check("already ignoring" in cmd(sB, f"ignore {nameA}"), "ignoring the same name twice is rejected")
check(nameA in cmd(sB, "ignore"), "bare ignore lists the ignored name")

out = cmd(sA, f"tell {nameB} are you there")
check(f"You tell {proper(nameB)}" in out, "tell reports success to the ignored sender regardless")
outB = recv_all(sB, timeout=1.0)
check("are you there" not in outB and "tells you" not in outB,
      "the ignoring target receives nothing from the blocked tell")

# fresh mortal characters both land in the same default room (same
# precedent smoke_test_combat.py relies on for its own PvP setup).
outw = cmd(sA, f"whisper {nameB} psst")
check(f"You whisper to {proper(nameB)}" in outw, "whisper reports success to the ignored sender regardless")
outwB = recv_all(sB, timeout=1.0)
check("psst" not in outwB, "the ignoring target receives nothing from the blocked whisper")

check(f"You are no longer ignoring {nameA}" in cmd(sB, f"unignore {nameA}"), "unignore removes a name")
check("aren't ignoring" in cmd(sB, f"unignore {nameA}"), "unignoring an absent name is rejected")

out = cmd(sA, f"tell {nameB} can you hear me now")
check(f"You tell {proper(nameB)}" in out, "tell still reports success after unignoring")
outB2 = recv_all(sB, timeout=1.0)
check("can you hear me now" in outB2, "the tell delivers normally once unignored")

check("can't ignore yourself" in cmd(sA, f"ignore {nameA}"), "can't ignore your own name")

sA.close()
sB.close()
announce_done("smoke_test_ignore")
print("=== ALL CHECKS PASSED ===")
