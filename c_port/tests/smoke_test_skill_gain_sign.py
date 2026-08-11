#!/usr/bin/env python3
"""Smoke test for the skill learn-by-doing gap (user, 2026-08-11: "sign
(0/79) untrained -- yet i use sign a lot and never gain proficiency").

Several skill-use commands gated on the skill but never called
skill_learn_from_doing(), so their proficiency was frozen at 0 forever no
matter how much the skill was used or how much discipline was practiced.
This covers the `sign` case end-to-end, plus the label fix:

  1. A known, discipline-practiced skill sitting at 0% reads "learned",
     not "untrained" (untrained implies not known).
  2. Actually using `sign` raises its stored proficiency above 0
     (skill_learn_from_doing now fires) -- previously it stayed 0.

    python3 tests/smoke_test_skill_gain_sign.py [host] [port]
"""
import re
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

announce("smoke_test_skill_gain_sign", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))


def make_char(name, pw, class_choice):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw, "new", name, "1", "1", class_choice, "done", "done"):
        send_line(s, step); recv_all(s)
    s.close()


def login(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


def skill_pct(pid_name):
    out = subprocess.run(
        ["mariadb", "tobin", "-N", "-e",
         f"SELECT pct FROM player_skill WHERE skill_name='sign' AND player_id="
         f"(SELECT id FROM player WHERE name='{pid_name}')"],
        capture_output=True, text=True).stdout.strip()
    return int(out) if out else None


name = f"Signr{_suffix}"
pw = "signpw12345"

make_char(name, pw, "3")   # Warrior -- sign is a level-1 CLASS-tier skill for every class
# Practice the Basic discipline (sign's ceiling) so the skill is unlocked
# and CAN gain -- exactly the user's state (discipline practiced, skill
# still at 0%).
sql(f"UPDATE player_progress SET basic_disc_pct=50 "
    f"WHERE player_id=(SELECT id FROM player WHERE name='{name}');")

check(skill_pct(name) is None, "sign starts with no proficiency row (0%)")

s = login(name, pw)

# 1. Label: a known, practiced, still-0% skill reads "learned", not "untrained".
prac = cmd(s, "practice basic")
sign_line = next((ln for ln in prac.splitlines() if re.search(r"\bsign\b", ln)), "")
check("sign" in sign_line.lower(), "sign appears in the Basic practice list")
check("learned" in sign_line.lower(),
      f"a known 0% skill reads 'learned' (line: {sign_line.strip()!r})")
check("untrained" not in sign_line.lower(),
      "a known 0% skill no longer reads 'untrained'")

# 2. Using sign actually gains proficiency (the real bug). Free both hands
# first -- newbie gear starts wielded, and sign refuses with full hands.
cmd(s, "remove all")
cmd(s, "drop all")
out = cmd(s, "sign hello there")
check("you sign" in out.lower(),
      f"the sign actually goes through (out: {out.strip()[:80]!r})")
time.sleep(0.5)
after = skill_pct(name)
check(after is not None and after >= 1,
      f"using `sign` raised its proficiency above 0 (now {after})")

# Cleanup.
send_line(s, "quit!"); recv_all(s)
sql(f"DELETE FROM player_skill WHERE player_id=(SELECT id FROM player WHERE name='{name}');")
sql(f"DELETE FROM player_progress WHERE player_id=(SELECT id FROM player WHERE name='{name}');")
sql(f"DELETE FROM player WHERE name='{name}';")

announce_done("smoke_test_skill_gain_sign", host, port)
print("=== ALL CHECKS PASSED ===")
