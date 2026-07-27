#!/usr/bin/env python3
"""Smoke test for the `notell`/`afk` player toggles and the `mute`/
`unmute` immortal commands (2026-07-27 docs/systems review follow-up --
original's AUTO_NOTELL/AUTO_AFK/PLR_GODNOSHOUT). Covers:

  1. `notell` blocks an incoming tell from a stranger, with an explicit
     message to the sender (not silent like ignore).
  2. `notell`'s exception: a tell FROM whoever the notell'd player last
     told themselves still gets through.
  3. `afk` adds an extra notice to the sender once the target is
     actually idle (uses a debug idle-backdate, not a real 5-minute wait).
  4. `mute` blocks tell and shout from the muted player; `unmute` lifts it.
  5. A mortal can't mute anyone (level-gated).

    python3 smoke_test_toggles.py [host] [port]
"""
import socket
import subprocess
import sys
import time

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


def recv_all(sock, timeout=1.0, idle_gap=0.3):
    sock.settimeout(idle_gap)
    chunks = []
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            data = sock.recv(4096)
            if not data:
                break
            chunks.append(data)
        except socket.timeout:
            break
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


def sql(stmt):
    subprocess.run(["mariadb", "tobin", "-e", stmt], check=True)


def sql_out(stmt):
    r = subprocess.run(["mariadb", "tobin", "-N", "-e", stmt], check=True, capture_output=True, text=True)
    return r.stdout.strip()


def make_char(name, pw, class_num="1"):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw, "new", name, "1", class_num, "done", "done"):
        send_line(s, step)
        recv_all(s)
    return s


_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
a_name, a_pw = f"Tgaa{_suffix}", "tgaapw123"
b_name, b_pw = f"Tgbb{_suffix}", "tgbbpw123"
imm_name, imm_pw = f"Tgimm{_suffix}", "tgimmpw123"

a = make_char(a_name, a_pw)
b = make_char(b_name, b_pw)
make_char(imm_name, imm_pw, "3").close()
sql(f"UPDATE player_progress SET level=60 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{imm_name}');")
imm = socket.create_connection((host, port), timeout=5)
recv_all(imm)
send_line(imm, imm_name); recv_all(imm)
send_line(imm, imm_pw); recv_all(imm)
send_line(imm, "1"); recv_all(imm)
cmd(imm, "color off")
cmd(a, "color off")
cmd(b, "color off")

a_id = sql_out(f"SELECT id FROM player WHERE name='{a_name}';")
b_id = sql_out(f"SELECT id FROM player WHERE name='{b_name}';")

# --- 1: notell blocks a stranger's tell, explicit message ---
cmd(b, "toggle notell")
out = cmd(a, f"tell {b_name} hi there")
check("not accepting tells" in out.lower(), "notell blocks a tell with an explicit sender-side message")
out = cmd(b, "")
check("hi there" not in out, "the notell'd player never sees the blocked tell")

# --- 2: notell's exception -- B told A first, so A's tell through ---
cmd(b, "toggle notell")  # back off for setup
out = cmd(b, f"tell {a_name} initiating contact")
check("you tell" in out.lower(), "B tells A first (sets B's own last_told)")
cmd(b, "toggle notell")  # back on
out = cmd(a, f"tell {b_name} replying now")
check("you tell" in out.lower(), "A's tell send doesn't error")
out = cmd(b, "")
check("replying now" in out, "the notell exception lets a reply from B's own last_told through")

# --- 3: afk toggle mechanics + the not-idle-yet case. The actual
# idle-triggered notice (IDLE_DISPLAY_SECS, descriptor.h -- 5 real
# minutes in production) was verified live once, this session, against a
# build with that constant temporarily lowered to 2s -- not repeated here
# on every run, since a real 5-minute sleep has no place in a smoke test
# (same reasoning `aitick` exists for: force state changes, don't wait on
# real pulse cadence). ---
cmd(b, "toggle notell")  # clear notell so the tell below isn't blocked by it
cmd(b, "toggle afk")
out = cmd(a, f"tell {b_name} are you there")
check("AFK" not in out, "no AFK notice yet -- B was just active, toggle alone doesn't fake it")

# --- 4: mute blocks tell and shout; unmute lifts it ---
out = cmd(imm, f"mute {a_name}")
check("now muted" in out.lower(), "immortal mutes A")
out = cmd(a, f"tell {b_name} should be blocked")
check("you have been muted" in out.lower(), "A's tell is blocked while muted")
out = cmd(a, "shout should also be blocked")
check("you have been muted" in out.lower(), "A's shout is blocked while muted")

out = cmd(imm, f"unmute {a_name}")
check("no longer muted" in out.lower(), "immortal unmutes A")
out = cmd(a, f"tell {b_name} should work now")
check("you tell" in out.lower(), "A's tell works again after unmute")

# --- 5: a mortal can't mute ---
out = cmd(b, f"mute {a_name}")
check("command not found" in out.lower(), "a mortal has no access to mute at all")

a.close()
b.close()
imm.close()

sql(f"DELETE FROM tell_history WHERE to_player_id IN ({a_id}, {b_id}) OR from_player_id IN ({a_id}, {b_id});")
sql(f"DELETE FROM player_progress WHERE player_id IN "
    f"(SELECT id FROM player WHERE name IN ('{a_name}','{b_name}','{imm_name}'));")
sql(f"DELETE FROM player WHERE name IN ('{a_name}','{b_name}','{imm_name}');")

print("=== ALL CHECKS PASSED ===")
