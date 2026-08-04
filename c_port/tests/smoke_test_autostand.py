#!/usr/bin/env python3
"""Smoke test for auto-stand-on-full-rest (user, 2026-08-03: "when
completely rested or when yoginsa or meditate gets you to full you
should automatically stand"). Two independent paths, both wired this
session: regen.c's plain rest/sleep/sit regen tick (~5s cadence,
REGEN_PULSES), and meditate.c's yoginsa/meditation tick (same cadence)
-- either one standing a resting character back up the moment BOTH HP
and Vitality reach their max, with a "You feel fully rested and stand
up." message. The meditation path also ends the meditation itself.

Covers:
  1. A resting character sitting 1 HP/1 Vit below max stands up
     automatically on the very next regen tick that tops them off.
  2. A meditating (yoginsa) character reaches full HP/Vit mid-session
     and is both stood up AND has their meditation ended automatically.
  3. A resting character who is NOT yet full does not get stood up.

    python3 tests/smoke_test_autostand.py [host] [port]
"""
import re
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))


announce("smoke_test_autostand", host, port)


# Territory (2026-08-03) is a forced step right after race: race, then
# homeland (1-3), then class.
def make_char(name, pw, class_num):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw, "new", name, "1", "1", str(class_num), "done", "done"):
        send_line(s, step)
        recv_all(s)
    return s


def relog(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, pw, "1"):
        send_line(s, step)
        recv_all(s)
    cmd(s, "color off")
    return s


# --- 1: plain rest tops off and stands up automatically ---
name1, pw1 = f"Astand{_suffix}", "astandpw12345"
s1 = make_char(name1, pw1, 3)  # Warrior
cmd(s1, "quit!"); s1.close()
sql(f"UPDATE player_progress SET hp=max_hp-1, vit=max_vit-1 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{name1}');")
s1 = relog(name1, pw1)
cmd(s1, "rest")
out = ""
for _ in range(4):
    time.sleep(1.5)
    out += recv_all(s1, 0.5)
check("fully rested and stand up" in out.lower(), "a resting character auto-stands the moment HP/Vit both top off")
out2 = cmd(s1, "score")
check("Position: Standing" in out2 or "standing" in out2.lower(),
      "the character's position is actually Standing afterward, not just the message")
s1.close()

# --- 2: yoginsa/meditation path also auto-stands AND ends meditation ---
name2, pw2 = f"Astandm{_suffix}", "astandmpw1234"
s2 = make_char(name2, pw2, 6)  # Monk (has yoginsa)
cmd(s2, "quit!"); s2.close()
sql(f"UPDATE player_progress SET hp=max_hp-1, vit=max_vit-1, basic_disc_pct=100 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{name2}');")
sql(f"INSERT INTO player_skill (player_id, skill_name, pct, last_gain_at) "
    f"VALUES ((SELECT id FROM player WHERE name='{name2}'), 'yoginsa', 100, {int(time.time())}) "
    f"ON DUPLICATE KEY UPDATE pct=100;")
s2 = relog(name2, pw2)
cmd(s2, "sit")
out = cmd(s2, "yoginsa")
check("begin meditating" in out.lower(), "yoginsa actually starts meditating (100% basic discipline seeded)")
out2 = ""
for _ in range(4):
    time.sleep(1.5)
    out2 += recv_all(s2, 0.5)
check("fully rested and stand up" in out2.lower(), "a meditating character auto-stands once HP/Vit both reach max")
# `yoginsa` TOGGLES meditating (cmd_yoginsa.c): if it were still on, this
# call would print "You stop meditating." instead -- printing "begin
# meditating" here proves auto-stand actually turned it off, not just
# stood the character up while still meditating underneath.
out3 = cmd(s2, "yoginsa")
check("begin meditating" in out3.lower(),
      "meditation was actually ENDED by reaching full, not left silently running")
s2.close()

# --- 3: not-yet-full resting character stays put ---
name3, pw3 = f"Astandnf{_suffix}", "astandnfpw123"
s3 = make_char(name3, pw3, 3)  # Warrior
cmd(s3, "quit!"); s3.close()
sql(f"UPDATE player_progress SET hp=max_hp/2, vit=max_vit/2 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{name3}');")
s3 = relog(name3, pw3)
cmd(s3, "rest")
out = ""
for _ in range(3):
    time.sleep(1.5)
    out += recv_all(s3, 0.5)
check("fully rested and stand up" not in out.lower(),
      "a resting character who is NOT yet full is not auto-stood up")
s3.close()

announce_done("smoke_test_autostand", host, port)
print("=== ALL CHECKS PASSED ===")
