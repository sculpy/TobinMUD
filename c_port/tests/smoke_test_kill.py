#!/usr/bin/env python3
"""Smoke test for the `kill` command:
  1. For a mortal, `kill <target>` behaves exactly like `attack <target>`
     (initiates the normal multi-round fight, applies the wait-state).
  2. For an immortal (hand-promoted to level 51+ via the DB, since there's
     no in-game promotion path yet), `kill <target>` instead kills the
     target INSTANTLY -- no multi-round fight, no wait-state cost, and a
     distinct "slain" message pair instead of the normal "defeated" pair.

    python3 tests/smoke_test_kill.py [host] [port]
"""
import socket
import subprocess
import sys
import time

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000
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
    return name[:1].upper() + name[1:].lower()


def make_player(tag):
    name = f"Kill{tag}{_suffix}"
    pw = "killtestpw123"
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


# --- Part 1: mortal `kill` behaves exactly like `attack` ---
sA, nameA = make_player("A")
sB, nameB = make_player("B")

send_line(sA, f"kill {nameB}")
send_line(sA, "score")  # sent immediately after, same back-to-back pattern as smoke_test_combat.py
out = recv_all(sA, timeout=0.5)
check(f"You attack {proper(nameB)}" in out, "mortal 'kill' produces the same 'You attack' message as attack")
check("still recovering" in out, "mortal is blocked by the wait-state right after 'kill', same as attack")

sA.close()
sB.close()

# --- Part 2: immortal `kill` is an instant, one-shot slay ---
sImm, nameImm = make_player("Imm")
sTarget, nameTarget = make_player("Tgt")
sObs, nameObs = make_player("Obs")  # a bystander for the global death taunt

subprocess.run(
    ["mariadb", "sneezy", "-e",
     f"UPDATE player_progress SET level=51 WHERE player_id=(SELECT id FROM player WHERE name='{nameImm}');"],
    check=True,
)

sImm.close()
sImm = socket.create_connection((host, port), timeout=5)
recv_all(sImm)
send_line(sImm, nameImm)
recv_all(sImm)
send_line(sImm, "killtestpw123")
out = step(sImm, "log back in as the promoted immortal", "1")
check("Welcome" in out, "reconnected and playing as the promoted character")
step(sImm, "rejoin the target in Center Square", "goto 100")

out = step(sImm, "immortal kills the target", f"kill {nameTarget}")
check(f"You have slain {proper(nameTarget)}" in out, "immortal 'kill' produces the instant-slay message, not 'attack'")
check("You attack" not in out, "immortal 'kill' does NOT go through the normal attack/fight path")

# The target should have been notified, told they're DEAD, and dropped at
# the account menu (not permadeleted -- descriptor_leave_to_menu(), same
# path `quit!`-while-playing uses -- their character still exists and is
# still selectable from that menu).
outTarget = recv_all(sTarget, timeout=1.0)
check(f"You have been slain by {proper(nameImm)}" in outTarget, "the slain target is notified")
check("You are DEAD!" in outTarget, "the slain target sees the DEAD message")
check("Your characters" in outTarget, "the slain target is dropped at the account menu, not respawned in-world")
check(proper(nameTarget) in outTarget, "the slain character still exists and is listed in the account menu")

# The whole world (here: the bystander) gets a teasing death announcement
# naming both parties -- neither winner nor loser receives it themselves.
outObs = recv_all(sObs, timeout=1.0)
check(proper(nameTarget) in outObs and proper(nameImm) in outObs,
      "a bystander receives the global death taunt naming victim and killer")
check("[INFO]" in outObs, "the death taunt arrives on the [INFO] channel")
check(outObs.rstrip().endswith(">"),
      "the unsolicited broadcast still leaves the bystander at a prompt")

out = step(sImm, "immortal acts again IMMEDIATELY -- should NOT be blocked (no wait-state was ever applied)", "score")
check("still recovering" not in out, "instakill never applied a wait-state to the immortal")

sImm.close()
sTarget.close()
sObs.close()
print("=== ALL CHECKS PASSED ===")
