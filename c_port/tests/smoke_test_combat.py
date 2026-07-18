#!/usr/bin/env python3
"""Smoke test for the pulse/wait-state engine and PvP combat:
  1. A mortal who just attacked is blocked ("You are still recovering!")
     from issuing another command immediately, but can act again after
     waiting past COMBAT_ROUND_PULSES (~1.2s).
  2. Combat rounds actually resolve over time (HP changes across a few
     rounds), not instantly/synchronously in the attack command itself.
  3. An immortal (hand-promoted to level 51+ via the DB, since there's no
     in-game promotion path yet) is NEVER blocked by the wait-state, even
     immediately after attacking.

    python3 tests/smoke_test_combat.py [host] [port]
"""
import re
import socket
import subprocess
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


announce("smoke_test_combat")

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


def step(sock, label, line, timeout=1.0):
    send_line(sock, line)
    out = recv_all(sock, timeout)
    print(f"=== {label} ===")
    print(out)
    return out


def check(condition, message):
    if not condition:
        raise AssertionError(message)
    print(f">>> OK: {message}")


def proper(name):
    """The server normalizes character names to proper case at creation
    (see being_normalize_name()) -- this mirrors that so assertions against
    server-echoed text match, regardless of how the name was typed here."""
    return name[:1].upper() + name[1:].lower()


def try_read_hp(out):
    """Returns (hp, max_hp) if `out` looks like a live score sheet, else
    None -- a defeated combatant is now ejected to the account menu (see
    combat_defeat() / STATUS.md), so a "score" sent right after a fatal
    round returns menu text, not an HP line."""
    m = re.search(r"HP:\s+(\d+)/(\d+)", out)
    return (int(m.group(1)), int(m.group(2))) if m else None


def check_ejected(out, who):
    check("You are DEAD!" in out and "Connect Player" in out,
          f"{who} didn't survive and was properly ejected to the account menu")


def make_player(suffix_tag):
    name = f"Combat{suffix_tag}{_suffix}"
    pw = "combattestpw123"
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name)
    recv_all(s)
    send_line(s, "y")
    recv_all(s)
    send_line(s, pw)
    recv_all(s)
    send_line(s, pw)  # confirm password (Session 21)
    recv_all(s)
    send_line(s, "new")
    recv_all(s)
    send_line(s, name)
    recv_all(s)
    send_line(s, "1"); recv_all(s)  # race: human (zero stat modifier)
    send_line(s, "1"); recv_all(s)  # class: mage
    send_line(s, "done")
    recv_all(s)
    send_line(s, "2"); recv_all(s)  # alignment: neutral
    return s, name


# --- Part 1: wait-state blocks a mortal, clears after ~1 combat round ---
# NOTE on timing: combat_process_run() fires on a GLOBAL pulse modulus (see
# pulse.h), not a per-fight timer -- so the first round after an attack can
# land anywhere from 1 to COMBAT_ROUND_PULSES pulses later, depending on
# where the global counter happens to be. being_set_wait() itself, though,
# is applied synchronously inside cmd_attack -- so send the follow-up
# command back-to-back with essentially no gap, rather than waiting on a
# generous idle-timeout read first (which risks a round's messages
# accumulating in the same buffer and eating wall-clock time before the
# follow-up is even sent).
sA, nameA = make_player("A")
sB, nameB = make_player("B")

send_line(sA, f"attack {nameB}")
send_line(sA, "score")  # sent immediately after, minimal gap
out = recv_all(sA, timeout=0.5)
print("=== attack + immediate follow-up (combined read) ===")
print(out)
check(f"You attack {proper(nameB)}" in out, "attack initiated")
check("still recovering" in out, "mortal is blocked by the wait-state right after attacking")

time.sleep(1.5)  # comfortably past COMBAT_ROUND_PULSES (~1.2s at 100ms/pulse)
out = step(sA, "A tries again after waiting -- should work now", "score")
check("still recovering" not in out, "wait clears after ~1 combat round, command succeeds")

# --- Part 2: rounds actually resolve HP changes (or a defeat) over time ---
# A dying this early (within ~1 round) is unlikely but not impossible --
# handle it the same way as a later-round defeat rather than assuming A is
# still alive to read a score sheet from.
hp_a = try_read_hp(out)
if hp_a is None:
    check_ejected(out, "A")
else:
    print(f"A's HP after ~1 round: {hp_a[0]}/{hp_a[1]}")

    time.sleep(3.0)  # let a few more rounds resolve
    out = step(sA, "check score again after more rounds", "score")
    hp_a2 = try_read_hp(out)
    if hp_a2 is None:
        check_ejected(out, "A")
    else:
        print(f"A's HP after more rounds: {hp_a2[0]}/{hp_a2[1]}")

    outB = step(sB, "check B's score too", "score")
    hp_b = try_read_hp(outB)
    if hp_b is None:
        check_ejected(outB, "B")
    else:
        print(f"B's HP: {hp_b[0]}/{hp_b[1]}")

    took_damage = (
        hp_a[0] < hp_a[1]
        or (hp_a2 is not None and hp_a2[0] < hp_a2[1])
        or (hp_b is not None and hp_b[0] < hp_b[1])
    )
    someone_died = hp_a2 is None or hp_b is None
    check(took_damage or someone_died,
          "at least one combatant took damage (or was defeated) after multiple rounds -- "
          "rounds actually resolved over time")

sA.close()
sB.close()

# --- Part 3: immortal bypass ---
sImm, nameImm = make_player("Imm")
sTarget, nameTarget = make_player("Tgt")

# Hand-promote the "immortal" test character to level 51 via the DB --
# there's no in-game promotion path yet (see STATUS.md).
subprocess.run(
    ["mariadb", "sneezy", "-e",
     f"UPDATE player_progress SET level=51 WHERE player_id=(SELECT id FROM player WHERE name='{nameImm}');"],
    check=True,
)

# Reconnect so the character reloads with the new level from the DB.
sImm.close()
sImm = socket.create_connection((host, port), timeout=5)
recv_all(sImm)
send_line(sImm, nameImm)
recv_all(sImm)
send_line(sImm, "combattestpw123")
out = step(sImm, "log back in -- should show the promoted character in the menu", "1")
check("Welcome" in out, "reconnected and playing as the promoted character")
step(sImm, "immortals default to room 1 now -- rejoin the target in 100", "goto 100")

out = step(sImm, "score shows immortal status", "score")
level_line = [l for l in out.splitlines() if "Level:" in l]
check(level_line and "Immortal" in level_line[0],
      "score marks the character as immortal via the rank title")

# attack == kill (full aliases): for an immortal both are the instant slay.
out = step(sImm, "immortal attacks (instant slay -- attack aliases kill)", f"attack {nameTarget}")
check(f"You have slain {proper(nameTarget)}" in out,
      "immortal 'attack' is the instant slay, same as kill")

out = step(sImm, "immortal acts again IMMEDIATELY -- should NOT be blocked", "score")
check("still recovering" not in out, "immortal is never blocked by the wait-state, even right after attacking")

# --- Part 4: `hit` always engages real combat, even for an immortal ---
sHit, nameHit = make_player("Hit")  # lands in room 100 by default, same as sImm
out = step(sImm, "immortal uses 'hit' instead of 'kill' -- should NOT instakill", f"hit {nameHit}")
check("You attack" in out, "'hit' engages normal combat (cmd_attack's own message), not an instakill")
check(f"You have slain {proper(nameHit)}" not in out, "'hit' never produces kill's instant-slay message")

sHit.close()
sImm.close()
sTarget.close()
announce_done("smoke_test_combat")
print("=== ALL CHECKS PASSED ===")
