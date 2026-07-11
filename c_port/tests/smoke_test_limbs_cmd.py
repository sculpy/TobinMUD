#!/usr/bin/env python3
"""Smoke test for the `limbs` command: unlike `score`'s Limbs section
(which only lists an injured limb), `limbs` always shows all 13 limbs
(head, neck, left/right arm, left/right finger, body, waist, genitalia,
right/left leg, left/right foot) with their current health percentage,
whether they're hurt or not.

Deterministic by design (Session 43 continued, diagnosed as a pre-existing
flake): rather than waiting on combat RNG to land enough hits on one limb
within a fixed round budget, this uses the immortal-only
`hurtlimb <target> <limb> <hp>` debug command (cmd_hurtlimb.c) to set a
limb's HP directly, then checks the `limbs` command's output.

    python3 tests/smoke_test_limbs_cmd.py [host] [port]
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


announce("smoke_test_limbs_cmd")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 900000 + (int(time.time()) % 70000)

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


def cmd(sock, line, timeout=1.0):
    send_line(sock, line)
    return recv_all(sock, timeout)


def check(condition, message):
    if not condition:
        raise AssertionError(message)
    print(f">>> OK: {message}")


def sql(stmt):
    subprocess.run(["mariadb", "sneezy", "-e", stmt], check=True)


def set_level(name, level):
    sql(f"UPDATE player_progress SET level={level} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def make_char(sock, name, pw):
    recv_all(sock)
    send_line(sock, name); recv_all(sock)
    send_line(sock, "y"); recv_all(sock)
    send_line(sock, pw); recv_all(sock)
    send_line(sock, pw); recv_all(sock)
    send_line(sock, "new"); recv_all(sock)
    send_line(sock, name); recv_all(sock)
    send_line(sock, "done"); recv_all(sock)


def login(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    return s


# --- Part 1: a fresh, undamaged character shows all 13 limbs at 100% ---
name = f"LimbCmd{_suffix}"
pw = "limbcmdtestpw123"
sA = socket.create_connection((host, port), timeout=5)
make_char(sA, name, pw)
cmd(sA, "color off")

send_line(sA, "limbs")
out = recv_all(sA)
check("Limbs" in out, "limbs shows a Limbs header")
for limb in LIMB_NAMES:
    check(re.search(rf"{re.escape(limb)}\s+100%", out) is not None,
          f"limbs lists '{limb}' at 100% on a fresh character")
check("hurt" not in out and "medical attention" not in out and "destroyed" not in out,
      "no injury phrases appear for a fully healthy character")
sA.close()

# --- Part 2: after a deterministic injury via hurtlimb, limbs still shows
# all 13, with the injured one flagged, alongside untouched limbs at 100% ---
imm_name = f"Limbcmdimm{_suffix}"
imm_pw = "limbcmdimmpw123"
victim_name = f"Limbcmdvic{_suffix}"
victim_pw = "limbcmdvicpw123"

s = socket.create_connection((host, port), timeout=5)
make_char(s, imm_name, imm_pw)
set_level(imm_name, 51)
s.close()
s = login(imm_name, imm_pw)
cmd(s, "color off")

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'LimbCmd Sandbox','A bare sandbox room.\\n',NULL,0,0,0,0,0,0,0,0,0,0);")
check("LimbCmd Sandbox" in cmd(s, f"goto {ROOM}"), "goto lands in the SQL-bootstrapped sandbox room")

sv = socket.create_connection((host, port), timeout=5)
make_char(sv, victim_name, victim_pw)
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{victim_name}';")
cmd(sv, "quit!")
sv.close()
sv = login(victim_name, victim_pw)
cmd(sv, "color off")
check("LimbCmd Sandbox" in cmd(sv, "look"), "the victim lands directly in the sandbox room")

# hp=2 against the LIMB_MIN_MAX_HP=15 floor -> 13%, inside the "hurt rather
# badly" (<20%, >=10%) tier.
out = cmd(s, f"hurtlimb {victim_name} rightleg 2")
check("Limb HP set" in out, "hurtlimb confirms (not a decapitation)")

out = cmd(sv, "limbs")
check("Limbs" in out, "limbs still shows a Limbs header after injury")
limb_lines = [l for l in out.splitlines()
              if "%" in l and any(limb in l for limb in LIMB_NAMES)]
check(len(limb_lines) == 13, "limbs still lists all 13 limbs, not just the injured one")
check(any("right leg" in l and "13%" in l for l in limb_lines),
      "the injured limb (right leg) shows its exact percentage (13%)")
check(sum(1 for l in limb_lines if "100%" in l) == 12,
      "the other 12 untouched limbs still show 100%")

s.close()
sv.close()
announce_done("smoke_test_limbs_cmd")
print("=== ALL CHECKS PASSED ===")
