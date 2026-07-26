#!/usr/bin/env python3
"""Smoke test for the connection/log-announce plumbing:
  1. `@test <name>` (loopback-only server hook) emits a [TEST] log line that
     online immortals see -- this is the mechanism EVERY smoke test uses to
     announce itself at start (announce()) and at finish (announce_done(),
     which logs a distinct "finished" line via "@test done <name>"). Every
     smoke test calls both -- copy these two functions into new tests.
  2. Player connect is a typed [PIO] log: an online immortal sees
     "<name> has connected. [<ip>]", symmetric to the (gender-specific)
     "has lost his/her/its link." line, and it carries the IP.

    python3 tests/smoke_test_logging.py [host] [port]
"""
import re
import socket
import subprocess
import sys
import time

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

def announce_done(test_name, host=host, port=port):
    """Companion to announce() -- emits a [TEST] "finished" log line. Call
    once at the very end of a smoke test, just before it reports success."""
    announce(f"done {test_name}", host, port)


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


def strip(s):
    return re.sub(r"\x1b\[[0-9;]*m", "", s)


def announce(test_name, host=host, port=port):
    """Tell the running MUD which smoke test is executing: emits a [TEST] log
    line (visible to online immortals and in the day's log file) via the
    loopback-only `@test` server hook. Best-effort -- never fails the test.
    Copy this into every new smoke test and call it once at startup."""
    try:
        s = socket.create_connection((host, port), timeout=3)
        recv_all(s, 0.5)
        send_line(s, f"@test {test_name}")
        recv_all(s, 0.5)
        s.close()
    except OSError:
        pass


announce("smoke_test_logging")


def set_level(name, level):
    subprocess.run(
        ["mariadb", "sneezy", "-e",
         f"UPDATE player_progress SET level={level} WHERE player_id="
         f"(SELECT id FROM player WHERE name='{name}');"],
        check=True,
    )


def make_player(tag):
    name = f"Log{tag}{_suffix}"
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "y"); recv_all(s)
    send_line(s, "logpw123"); recv_all(s)
    send_line(s, "logpw123"); recv_all(s)
    send_line(s, "new"); recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "1"); recv_all(s)  # race: human (zero stat modifier)
    send_line(s, "1"); recv_all(s)  # class: mage
    send_line(s, "done"); recv_all(s)
    send_line(s, "done"); recv_all(s)  # alignment: neutral
    return s, name


def relog(name):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "logpw123"); recv_all(s)
    send_line(s, "1"); recv_all(s)
    return s


# --- an immortal watcher who will see the [PIO]/[TEST] echoes ---
sImm, nameImm = make_player("Imm")
set_level(nameImm, 51)
sImm.close()
sImm = relog(nameImm)

# --- connect is a typed [PIO] log carrying the IP ---
recv_all(sImm)  # drain the watcher's own arrival noise
sVictim, nameVictim = make_player("Vic")
outImm = strip(recv_all(sImm, timeout=1.0))
check(f"[PIO] {nameVictim.capitalize()} has connected." in outImm,
      "an online immortal sees the [PIO] connect line for a new player")
check(re.search(rf"{nameVictim.capitalize()} has connected\. \[[0-9.:]+\]", outImm),
      "the connect line carries the connecting player's IP address")
sVictim.close()

# --- the [TEST] announce hook reaches immortals ---
recv_all(sImm)  # drain the victim's link-drop line
marker = f"marker{_suffix}"
announce(marker)
outImm = strip(recv_all(sImm, timeout=1.5))
check(f"[TEST] running {marker}" in outImm,
      "an online immortal sees the [TEST] announce line from the @test hook")

# --- an immortal who doesn't need test announcements can silence just those
#     via `setsev test` (the rest of their log echoes are unaffected) ---
send_line(sImm, "setsev test"); recv_all(sImm)
announce(marker + "off")
outImm = strip(recv_all(sImm, timeout=1.5))
check(f"[TEST] running {marker}off" not in outImm,
      "setsev test off silences [TEST] announcements for that immortal")
send_line(sImm, "setsev test"); recv_all(sImm)  # restore
announce(marker + "on")
outImm = strip(recv_all(sImm, timeout=1.5))
check(f"[TEST] running {marker}on" in outImm,
      "setsev test on restores [TEST] announcements")

# --- announce_done() logs a distinct "finished" line, not another "running" ---
announce_done(marker)
outImm = strip(recv_all(sImm, timeout=1.5))
check(f"[TEST] finished {marker}" in outImm,
      "announce_done() logs a [TEST] finished line, distinct from running")

# hygiene
set_level(nameImm, 1)
sImm.close()
announce_done("smoke_test_logging")
print("=== ALL CHECKS PASSED ===")
