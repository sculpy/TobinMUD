#!/usr/bin/env python3
"""Smoke test for limb-based combat:
  1. `score` shows NO "Limbs:" section at all for a fresh, undamaged
     character -- a limb only appears once it's actually hurt (< 20%
     health, per limb_status_text()).
  2. Combat hit/miss messages name which limb got hit (e.g. "You hit X's
     left leg for 3 damage!"), not just a flat "you hit X for N damage".
  3. As a limb's health percentage drops, escalating injury messages fire
     ("is hurt rather badly" < 20%, "needs medical attention" < 10%, "is
     destroyed and needs medical attention" at 0%) -- both in combat and in
     `score`'s per-limb breakdown (which then shows that limb's line, with
     its percentage), using identical wording either way (limb_status_text()
     in being.c). Limb max HP is small by design (progress.max_hp / 13,
     roughly 1-2 on a fresh mortal), so ordinary combat damage (1-6 per hit)
     reliably drives at least one limb through these tiers within a
     handful of rounds.
  4. `limbs` (a dedicated command, see smoke_test_limbs_cmd.py) shows every
     limb unconditionally, unlike score's injured-only display -- covered
     separately since it's a distinct command.

    python3 tests/smoke_test_limbs.py [host] [port]
"""
import re
import socket
import sys
import time

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000
_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))

LIMB_NAMES = [
    "head", "neck", "left arm", "right arm", "left finger", "right finger",
    "body", "waist", "genitalia", "right leg", "left leg", "left foot", "right foot",
]
INJURY_PHRASES = [
    "is hurt rather badly",
    "needs medical attention",
    "is destroyed and needs medical attention",
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
    name = f"Limb{tag}{_suffix}"
    pw = "limbtestpw123"
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


# --- Part 1: score shows no Limbs section at all when undamaged ---
sA, nameA = make_player("A")
send_line(sA, "score")
out = recv_all(sA)
check("Limbs:" not in out, "a fresh, undamaged character has no Limbs: section in score at all")
check(not any(phrase in out for phrase in INJURY_PHRASES),
      "a fresh, undamaged character has no injury lines in score")

# --- Part 2: combat messages name a limb, and injuries escalate ---
sB, nameB = make_player("B")
send_line(sA, f"attack {nameB}")
out = recv_all(sA, timeout=1.0)
check("You attack" in out, "attack initiated")

# Collect combat messages over several rounds, per-socket (not merged), so
# we know exactly which character actually took the injury and can later
# check *their* score reflects it. Limb max HP is tiny (roughly max_hp/13,
# ~2 on a fresh mortal) relative to per-hit damage (1-6), so a specific-limb
# message and at least one injury-tier escalation should both show up well
# within this window.
found_limb_message = False
injured_sock = None
for _ in range(8):
    time.sleep(1.5)  # comfortably past one COMBAT_ROUND_PULSES (~1.2s)
    chunk_a = recv_all(sA, timeout=0.5)
    chunk_b = recv_all(sB, timeout=0.5)
    if not found_limb_message and any(
        f"'s {limb} for" in (chunk_a + chunk_b) or f"your {limb} for" in (chunk_a + chunk_b)
        for limb in LIMB_NAMES
    ):
        found_limb_message = True
        print("=== found a limb-aware combat message ===")
        print(chunk_a + chunk_b)
    if injured_sock is None:
        if any(f"Your {limb} {phrase}" in chunk_a for limb in LIMB_NAMES for phrase in INJURY_PHRASES):
            injured_sock, injured_name = sA, nameA
            print("=== A took an injury-tier hit ===")
            print(chunk_a)
        elif any(f"Your {limb} {phrase}" in chunk_b for limb in LIMB_NAMES for phrase in INJURY_PHRASES):
            injured_sock, injured_name = sB, nameB
            print("=== B took an injury-tier hit ===")
            print(chunk_b)
    if found_limb_message and injured_sock is not None:
        break

check(found_limb_message, "at least one combat exchange named a specific limb that was hit")
check(injured_sock is not None, "at least one limb's damage crossed an injury tier and was announced")

# --- Part 3: score now shows a Limbs section for the injured character ---
# (Skipped if that character was fully defeated -- overall HP <= 0 -- in
# the same window and ejected to the account menu; combat already proved
# the injury-tier message fires either way, this just double-checks score
# reflects it too, when there's still a live character to check.)
send_line(injured_sock, "score")
out = recv_all(injured_sock)
if "Your characters" in out and "You are DEAD!" in out:
    print(f"=== {injured_name} was fully defeated before score could be checked -- skipping Part 3 ===")
else:
    check("Limbs:" in out, f"score now shows a Limbs: section for {injured_name} after taking injury-tier damage")
    check(any(phrase in out for phrase in INJURY_PHRASES),
          "score's Limbs section uses the same injury wording combat announced")
    check(re.search(r"\(\d+%\)", out) is not None, "score's injury line includes the limb's percentage")

sA.close()
sB.close()
print("=== ALL CHECKS PASSED ===")
