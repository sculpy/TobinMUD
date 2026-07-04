#!/usr/bin/env python3
"""Smoke test for Phase 2A immortal command infrastructure:
  1. min_level enforcement: a mortal typing `goto`/`promote` (or any
     prefix of them, like `g`) gets "Huh?!" -- immortal commands are
     invisible to mortals, not merely refused.
  2. `goto <vnum>`: an immortal teleports to room 0 ("The Void") and
     back to 1 ("Imperia"); a nonexistent vnum and a missing/garbage
     argument are both rejected cleanly.
  3. `promote <name> [level]`: promotes an online player (applies live,
     no relog needed -- their score changes immediately, and they get
     told); rejects levels above the promoter's own, out-of-range
     levels, self-promotion, and unknown players. Also works as a
     demotion (setting a lower level).

    python3 tests/smoke_test_immortal_cmds.py [host] [port]
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


def check(condition, message):
    if not condition:
        raise AssertionError(message)
    print(f">>> OK: {message}")


def make_player(tag):
    name = f"Wiz{tag}{_suffix}"
    pw = "wiztestpw123"
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


sImm, nameImm = make_player("Imm")
sMort, nameMort = make_player("Mort")

# --- Part 1: immortal commands are invisible to mortals ---
for attempt in ["goto 0", "promote somebody", "g 0"]:
    send_line(sMort, attempt)
    out = recv_all(sMort)
    check("Huh?!" in out, f"a mortal typing '{attempt}' gets Huh?! (command hidden)")

# --- Bootstrap the immortal (level 58 so the above-own-level cap is testable) ---
subprocess.run(
    ["mariadb", "sneezy", "-e",
     f"UPDATE player_progress SET level=58 WHERE player_id=(SELECT id FROM player WHERE name='{nameImm}');"],
    check=True,
)
sImm.close()
sImm = socket.create_connection((host, port), timeout=5)
recv_all(sImm)
send_line(sImm, nameImm)
recv_all(sImm)
send_line(sImm, "wiztestpw123")
recv_all(sImm)
send_line(sImm, "1")
recv_all(sImm)

# --- Part 2: goto ---
send_line(sImm, "goto 0")
out = recv_all(sImm)
check("The Void" in out, "goto 0 lands in The Void (auto-look shows it)")

send_line(sImm, "goto 1")
out = recv_all(sImm)
check("Imperia" in out, "goto 1 returns to Imperia")

send_line(sImm, "goto 99999999")
out = recv_all(sImm)
check("No room with vnum" in out, "goto to a nonexistent vnum is rejected")

send_line(sImm, "goto nowhere")
out = recv_all(sImm)
check("Usage: goto" in out, "goto with a non-numeric argument shows usage")

# --- Part 3: promote ---
send_line(sImm, f"promote {nameMort} 60")
out = recv_all(sImm)
check("above your own level" in out, "promoting above the promoter's own level is rejected")

send_line(sImm, f"promote {nameMort} 0")
out = recv_all(sImm)
check("Level must be between" in out, "an out-of-range level is rejected")

send_line(sImm, f"promote {nameImm.capitalize()}")
out = recv_all(sImm)
check("promote yourself" in out, "self-promotion is rejected")

send_line(sImm, "promote Nosuchplayerxyz")
out = recv_all(sImm)
check("No player named" in out, "promoting an unknown player is rejected")

send_line(sImm, f"promote {nameMort}")
out = recv_all(sImm)
check("is now level 51" in out, "promote defaults to level 51 and confirms")
out_mort = recv_all(sMort)
check("promoted you to level 51" in out_mort, "the target is told they were promoted")

# Applies live -- no relog: the target's own score shows the immortal title.
send_line(sMort, "score")
out = recv_all(sMort)
check("Immortal" in out, "the promotion applies to the live character immediately")

# And the target can now see immortal commands (wizhelp gate uses live level).
send_line(sMort, "wizhelp")
out = recv_all(sMort)
check("Immortal-only commands" in out, "the freshly promoted player passes the wizhelp gate")

# Demotion works the same way.
send_line(sImm, f"promote {nameMort} 1")
out = recv_all(sImm)
check("is now level 1" in out, "demotion (promote to a lower level) confirms")
out_mort = recv_all(sMort)
check("demoted you to level 1" in out_mort, "the target is told they were demoted")
send_line(sMort, "goto 0")
out = recv_all(sMort)
check("Huh?!" in out, "a demoted player immediately loses immortal commands")

# --- Part 4: loadroom (51+) sets where the character logs in ---
send_line(sMort, "loadroom 0")
out = recv_all(sMort)
check("Huh?!" in out, "a mortal typing loadroom gets Huh?! (hidden)")

send_line(sImm, "loadroom")
out = recv_all(sImm)
check("You currently load into room" in out, "bare loadroom shows the current setting")

send_line(sImm, "loadroom 99999999")
out = recv_all(sImm)
check("No room with vnum" in out, "loadroom to a nonexistent room is rejected")

send_line(sImm, "loadroom 0")
out = recv_all(sImm)
check("enter the game in room 0" in out, "loadroom 0 confirms with the room name")

send_line(sImm, "quit!")
recv_all(sImm)
send_line(sImm, "1")
out = recv_all(sImm)
check("The Void" in out, "re-entering the game lands in the new load room (The Void)")

send_line(sImm, "loadroom 1")  # restore
recv_all(sImm)

# --- Part 5: users (58+) -- the connection roster ---
send_line(sMort, "users")
check("Huh?!" in recv_all(sMort), "a mortal typing users gets Huh?! (hidden)")

send_line(sImm, "users")
out = recv_all(sImm)
check(nameImm.capitalize() in out and "127.0.0.1" in out and "playing" in out,
      "users lists this connection with IP and state")
check("connection" in out, "users reports the connection count")

sImm.close()
sMort.close()
print("=== ALL CHECKS PASSED ===")
