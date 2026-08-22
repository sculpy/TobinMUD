#!/usr/bin/env python3
"""Smoke test for the `limbs` command: unlike `score`'s Limbs section
(which only lists an injured limb), `limbs` always shows all 18 real,
always-active limbs (head, neck, back, left/right arm, left/right wrist,
left/right hand, left/right finger, body, waist, genitalia, right/left
leg, left/right foot -- Limbs -> wearSlotT, 2026-07-26) with their
current health percentage, whether they're hurt or not. The 4 mob-only
EX_* extra-leg/-foot slots stay hidden (being_has_limb()) since nothing
assigns them a real max_hp until the separate Body types system exists.

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
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

announce("smoke_test_limbs_cmd", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 900000 + (int(time.time()) % 70000)

LIMB_NAMES = [
    "head", "neck", "back", "left arm", "right arm", "left wrist", "right wrist",
    "left hand", "right hand", "left finger", "right finger",
    "body", "waist", "genitalia", "right leg", "left leg", "left foot", "right foot",
]


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
    send_line(sock, "1"); recv_all(sock)  # race: human (zero stat modifier)
    send_line(sock, "1"); recv_all(sock)  # homeland: urban (territory, forced step since 2026-08-03)
    send_line(sock, "1"); recv_all(sock)  # class: mage
    send_line(sock, "done"); recv_all(sock)
    send_line(sock, "done"); recv_all(sock)  # alignment: neutral


def login(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    return s


# --- Part 1: a fresh, undamaged character shows all 18 limbs at 100% ---
name = f"LimbCmd{_suffix}"
pw = "limbcmdtestpw123"
sA = socket.create_connection((host, port), timeout=5)
make_char(sA, name, pw)
cmd(sA, "color off")

send_line(sA, "limbs")
out = recv_all(sA)
check("Limbs" in out, "limbs shows a Limbs header")
for limb in LIMB_NAMES:
    check(re.search(rf"{re.escape(limb)}\s+perfect\s+\(100%\)", out) is not None,
          f"limbs lists '{limb}' as 'perfect (100%)' on a fresh character")
check("medical attention" not in out and "destroyed" not in out,
      "no injury phrases appear for a fully healthy character")
sA.close()

# --- Part 2: after a deterministic injury via hurtlimb, limbs still shows
# all 18, with the injured one flagged, alongside untouched limbs at 100% ---
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
    f"VALUES ({ROOM},0,0,0,'LimbCmd Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
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
#
# Root-caused 2026-07-26 (was previously an unexplained ~1-in-3-4 flake,
# TODO.md): regen_tick_run() (regen.c) heals EVERY limb by a small amount
# every REGEN_PULSES (~5s real time) for any connected, non-fighting
# character -- being_heal()'s per-limb spillover, not a bug. If that tick
# happens to land in the (usually sub-second, but not guaranteed) gap
# between the `hurtlimb` call and this `limbs` read, the right leg picks
# up +1 HP (2->3, i.e. 15% not 13%) per tick that fires -- a real,
# expected race against wall-clock time, not infra flakiness. Accepting
# either outcome (0 or 1 regen ticks) keeps this deterministic without
# either disabling real regen behavior just for a test or requiring
# sub-tick-precision timing; 2+ ticks (would need several seconds of
# delay between the two commands) still fails, since that would indicate
# something actually wrong rather than ordinary scheduling jitter.
out = cmd(s, f"crit {victim_name} rightleg 2")
check("Limb HP set" in out, "hurtlimb confirms (not a decapitation)")

out = cmd(sv, "limbs")
check("Limbs" in out, "limbs still shows a Limbs header after injury")
# Anchored on the real `limbs` line format ("  <limb name>  <word>  (NN%)")
# rather than a bare substring match -- hurtlimb's own broadcast ("Your
# right leg is hurt rather badly!") can still be sitting unread in the
# socket buffer ahead of this response and would otherwise double-count
# whichever limb it just injured.
limb_lines = [l for l in out.splitlines()
              if re.match(r"^  (" + "|".join(re.escape(n) for n in LIMB_NAMES) + r")\s+\S+\s+\(", l)]
check(len(limb_lines) == 18, "limbs still lists all 18 real limbs, not just the injured one")
# hp=2/15 = 13% -> "horrid" (10-19% tier); hp=3/15 = 20% -> "awful" (20-29%
# tier) if one ~5s regen tick landed between hurtlimb and this check --
# see the comment above for why both outcomes are accepted.
check(any("right leg" in l and ("horrid" in l or "awful" in l) for l in limb_lines),
      "the injured limb (right leg) shows its expected health word (horrid, or awful if one "
      "~5s regen tick landed between hurtlimb and this check)")
check(sum(1 for l in limb_lines if "perfect" in l) == 17,
      "the other 17 untouched limbs still show 'perfect'")

s.close()
sv.close()
announce_done("smoke_test_limbs_cmd", host, port)
print("=== ALL CHECKS PASSED ===")
