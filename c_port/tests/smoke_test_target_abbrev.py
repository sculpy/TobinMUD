#!/usr/bin/env python3
"""Smoke test for combat target-name abbreviation (Session 20):
  1. An exact name always wins over a prefix match: with players Tgt123
     and Tgt123x both in the room, `attack tgt123` must hit Tgt123,
     never Tgt123x.
  2. `attack <prefix>` finds a room occupant whose name starts with the
     prefix ("attack abbrh" -> Abbrhunter...), case-insensitively.
  3. A prefix matching nobody is rejected with the usual message.

    python3 tests/smoke_test_target_abbrev.py [host] [port]
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


announce("smoke_test_target_abbrev")

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


def check(condition, message):
    if not condition:
        raise AssertionError(message)
    print(f">>> OK: {message}")


def make_player(name):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name)
    recv_all(s)
    send_line(s, "abbrevpw123")
    recv_all(s)
    send_line(s, "abbrevpw123")  # confirm password (Session 21)
    recv_all(s)
    send_line(s, "new")
    recv_all(s)
    send_line(s, name)
    recv_all(s)
    send_line(s, "done")
    recv_all(s)
    return s


# Names chosen so one is an exact prefix of the other (the worst case
# for the exact-beats-prefix rule). being_normalize_name() proper-cases
# whatever we type, so assertions use the capitalized forms.
short_name = f"Tgt{_suffix}"
long_name = f"Tgt{_suffix}x"
hunter_name = f"Abbrhunter{_suffix}"
proper_short = short_name.capitalize()
proper_long = long_name.capitalize()
proper_hunter = hunter_name.capitalize()

sHunter = make_player(hunter_name)
sShort = make_player(short_name)
sLong = make_player(long_name)

# --- Part 1: exact name beats prefix ---
send_line(sHunter, f"attack tgt{_suffix}")
out = recv_all(sHunter)
check(f"You attack {proper_short}!" in out,
      f"exact name 'tgt{_suffix}' hits {proper_short}, not {proper_long}")
check("attacks you" in recv_all(sShort),
      "the exact-name target sees themselves attacked")
check("attacks you" not in recv_all(sLong),
      "the longer-named bystander is untouched")

# --- Part 2: a partial prefix reaches its only match ---
send_line(sLong, "attack abbrh")
out = recv_all(sLong)
check(f"You attack {proper_hunter}!" in out,
      "a partial prefix ('abbrh') reaches the only matching player")

# --- Part 3: prefix matching nobody ---
send_line(sHunter, "attack zzznobody")
out = recv_all(sHunter)
check("They aren't here." in out,
      "a prefix matching nobody is rejected with the usual message")

for s in (sHunter, sShort, sLong):
    s.close()
announce_done("smoke_test_target_abbrev")
print("=== ALL CHECKS PASSED ===")
