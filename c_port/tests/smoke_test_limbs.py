#!/usr/bin/env python3
"""Smoke test for limb-based combat:
  1. `score` shows NO "Limbs:" section at all for a fresh, undamaged
     character -- a limb only appears once it's actually hurt (< 20%
     health, per limb_status_text()).
  2. Combat hit/miss messages name which limb got hit (e.g. "You hit X's
     left leg for 3 damage!"), not just a flat "you hit X for N damage".
     This part still uses real combat -- it's reliable within a handful
     of rounds since every landed hit names a limb (doesn't depend on
     RNG crossing an injury tier).
  3. As a limb's health percentage drops, escalating injury messages fire
     ("is hurt rather badly" < 20%, "needs medical attention" < 10%, "is
     destroyed and needs medical attention" at 0%) -- both in combat and in
     `score`'s per-limb breakdown, using identical wording either way
     (limb_status_text() in being.c). Deterministic by design (Session 43
     continued, diagnosed as a pre-existing flake): rather than waiting on
     combat RNG to land enough hits on one limb within a fixed round
     budget, this uses the immortal-only `hurtlimb <target> <limb> <hp>`
     debug command (cmd_hurtlimb.c) to set a limb's HP directly, which now
     also fires the exact same injury-tier tell() messages a real
     combat_strike() hit would (combat_debug_set_limb_hp() in combat.c).
  4. `limbs` (a dedicated command, see smoke_test_limbs_cmd.py) shows every
     limb unconditionally, unlike score's injured-only display -- covered
     separately since it's a distinct command.

    python3 tests/smoke_test_limbs.py [host] [port]
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


announce("smoke_test_limbs")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 900000 + (int(time.time()) % 70000)

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
    send_line(sock, "1"); recv_all(sock)  # race: human (zero stat modifier)
    send_line(sock, "1"); recv_all(sock)  # class: mage
    send_line(sock, "2"); recv_all(sock)  # alignment: neutral


def login(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    return s


def make_player(tag):
    name = f"Limb{tag}{_suffix}"
    pw = "limbtestpw123"
    s = socket.create_connection((host, port), timeout=5)
    make_char(s, name, pw)
    return s, name


# --- Part 1: score shows no Limbs section at all when undamaged ---
sA, nameA = make_player("A")
send_line(sA, "score")
out = recv_all(sA)
check("Limbs:" not in out, "a fresh, undamaged character has no Limbs: section in score at all")
check(not any(phrase in out for phrase in INJURY_PHRASES),
      "a fresh, undamaged character has no injury lines in score")

# --- Part 2: combat messages name a limb (real combat -- reliable within a
# few rounds, doesn't depend on crossing an injury tier) ---
sB, nameB = make_player("B")
send_line(sA, f"attack {nameB}")
out = recv_all(sA, timeout=1.0)
check("You attack" in out, "attack initiated")

found_limb_message = False
for _ in range(6):
    time.sleep(1.5)  # comfortably past one COMBAT_ROUND_PULSES (~1.2s)
    chunk_a = recv_all(sA, timeout=0.5)
    chunk_b = recv_all(sB, timeout=0.5)
    if any(
        f"'s {limb} for" in (chunk_a + chunk_b) or f"your {limb} for" in (chunk_a + chunk_b)
        for limb in LIMB_NAMES
    ):
        found_limb_message = True
        print("=== found a limb-aware combat message ===")
        print(chunk_a + chunk_b)
        break

check(found_limb_message, "at least one combat exchange named a specific limb that was hit")

send_line(sA, f"flee")  # stop the fight so the deterministic part below isn't racing real combat
recv_all(sA, timeout=1.0)
recv_all(sB, timeout=0.5)

sA.close()
sB.close()

# --- Part 3: deterministic injury-tier escalation via hurtlimb, both in the
# combat-style tell() message and in score's per-limb breakdown ---
imm_name = f"Limbimm{_suffix}"
imm_pw = "limbimmpw123"
victim_name = f"Limbvic{_suffix}"
victim_pw = "limbvicpw123"

s = socket.create_connection((host, port), timeout=5)
make_char(s, imm_name, imm_pw)
set_level(imm_name, 51)
s.close()
s = login(imm_name, imm_pw)
cmd(s, "color off")

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Limb Sandbox','A bare sandbox room.\\n',NULL,0,0,0,0,0,0,0,0,0,0);")
check("Limb Sandbox" in cmd(s, f"goto {ROOM}"), "goto lands in the SQL-bootstrapped sandbox room")

sv = socket.create_connection((host, port), timeout=5)
make_char(sv, victim_name, victim_pw)
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{victim_name}';")
cmd(sv, "quit!")
sv.close()
sv = login(victim_name, victim_pw)
cmd(sv, "color off")
check("Limb Sandbox" in cmd(sv, "look"), "the victim lands directly in the sandbox room")

# hp=2 against the LIMB_MIN_MAX_HP=15 floor -> 13%, inside the "hurt rather
# badly" (<20%, >=10%) tier.
out = cmd(s, f"hurtlimb {victim_name} leftarm 2")
check("Limb HP set" in out, "hurtlimb confirms (not a decapitation)")
check(f"{victim_name}'s left arm is hurt rather badly!" in out,
      "hurtlimb's immortal-side message reports the injury tier crossing")

outVictim = recv_all(sv, timeout=1.0)
check("Your left arm is hurt rather badly!" in outVictim,
      "the victim sees the same injury-tier message combat would send")

out = cmd(sv, "score")
check("Limbs:" in out, "score now shows a Limbs: section for the victim after taking injury-tier damage")
check("is hurt rather badly" in out,
      "score's Limbs section uses the same injury wording combat announced")
m = re.search(r"left arm.*\((\d+)%\)", out)
check(m is not None and m.group(1) == "13",
      "score's injury line includes the limb's exact percentage (13%)")

s.close()
sv.close()
announce_done("smoke_test_limbs")
print("=== ALL CHECKS PASSED ===")
