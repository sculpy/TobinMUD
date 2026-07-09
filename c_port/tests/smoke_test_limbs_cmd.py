#!/usr/bin/env python3
"""Smoke test for the `limbs` command: unlike `score`'s Limbs section
(which only lists an injured limb), `limbs` always shows all 13 limbs
(head, neck, left/right arm, left/right finger, body, waist, genitalia,
right/left leg, left/right foot) with their current health percentage,
whether they're hurt or not.

    python3 tests/smoke_test_limbs_cmd.py [host] [port]
"""
import re
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


announce("smoke_test_limbs_cmd")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))

LIMB_NAMES = [
    "head", "neck", "left arm", "right arm", "left finger", "right finger",
    "body", "waist", "genitalia", "right leg", "left leg", "left foot", "right foot",
]


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


def make_player(tag):
    name = f"LimbCmd{tag}{_suffix}"
    pw = "limbcmdtestpw123"
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name)
    recv_all(s)
    send_line(s, pw)
    recv_all(s)
    send_line(s, pw)  # confirm password (Session 21)
    recv_all(s)
    send_line(s, "new")
    recv_all(s)
    send_line(s, name)
    recv_all(s)
    send_line(s, "done")
    recv_all(s)
    return s, name


# --- Part 1: a fresh, undamaged character shows all 12 limbs at 100% ---
sA, nameA = make_player("A")
send_line(sA, "limbs")
out = recv_all(sA)
check("Limbs" in out, "limbs shows a Limbs header")
for limb in LIMB_NAMES:
    check(re.search(rf"{re.escape(limb)}\s+100%", out) is not None,
          f"limbs lists '{limb}' at 100% on a fresh character")
check("hurt" not in out and "medical attention" not in out and "destroyed" not in out,
      "no injury phrases appear for a fully healthy character")

# --- Part 2: after taking damage, limbs still shows all 12, with the
# injured one(s) flagged, alongside untouched limbs still at 100% ---
sB, nameB = make_player("B")
send_line(sA, f"attack {nameB}")
recv_all(sA, timeout=1.0)

found_injury_in_limbs_cmd = False
for _ in range(8):
    time.sleep(1.5)  # comfortably past one COMBAT_ROUND_PULSES (~1.2s)
    recv_all(sA, timeout=0.5)  # drain combat messages
    send_line(sA, "limbs")
    out = recv_all(sA, timeout=0.5)
    if "Limbs" not in out:
        # A may have been defeated and ejected to the account menu in the
        # same window -- that's fine, it doesn't invalidate the feature
        # (already proven if found_injury_in_limbs_cmd is already True, or
        # we just try B below instead).
        break
    # Only count actual limb-listing lines (a limb name AND a percentage) --
    # interleaved combat strike messages also mention limb names but never
    # carry a "%", so this ignores them and keeps the count reliable.
    limb_lines = [l for l in out.splitlines()
                  if "%" in l and any(limb in l for limb in LIMB_NAMES)]
    check(len(limb_lines) == 13 or len(limb_lines) == 0,
          "limbs always lists all 13 limbs, never a partial set")
    if any("%  --" in l for l in limb_lines):
        found_injury_in_limbs_cmd = True
        print("=== limbs command shows an injured limb alongside healthy ones ===")
        print(out)
        break

check(found_injury_in_limbs_cmd, "limbs eventually shows an injury flag on a damaged limb")

sA.close()
sB.close()
announce_done("smoke_test_limbs_cmd")
print("=== ALL CHECKS PASSED ===")
