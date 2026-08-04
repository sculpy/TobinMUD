#!/usr/bin/env python3
"""Smoke test for showing fighting status in a room's `look` listing (user,
2026-08-03): "when fighting and you look in the room you should see the mob
fighting a tank. A deputy of the Brotherhood is here, fighting you."

Previously render_room_item() (cmd_look.c) only ever rendered "<label> is
here." for a PC/mob room occupant, with no indication of who (if anyone)
they were currently fighting. Now a fighting occupant's line reads
"<label> is here, fighting you." from the perspective of the one they're
actually fighting, or "<label> is here, fighting <opponent>." for anyone
else looking on. Grouping (stacking identical lines with "(xN)") is by the
rendered string itself, so two mobs fighting different targets naturally
render distinct lines and are never incorrectly stacked together.

Covers:
  1. A third-party viewer sees both fighters' lines naming each other by
     name ("A is here, fighting B." / "B is here, fighting A.").
  2. The attacker's own `look` shows their opponent as "fighting you".
  3. The defender's own `look` shows their opponent as "fighting you" too
     (symmetric -- either side's perspective reads "fighting you").
  4. Once combat ends (flee), the "fighting" clause disappears from the
     room listing entirely.

    python3 tests/smoke_test_look_fighting.py [host] [port]
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


announce("smoke_test_look_fighting", host, port)


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


name1, pw1 = f"Lfa{_suffix}", "lfapw123456"
name2, pw2 = f"Lfb{_suffix}", "lfbpw123456"
name3, pw3 = f"Lfc{_suffix}", "lfcpw123456"
s1 = make_char(name1, pw1, 3)  # Warrior (attacker)
s2 = make_char(name2, pw2, 3)  # Warrior (target)
s3 = make_char(name3, pw3, 3)  # Warrior (third-party viewer)
cmd(s1, "quit!"); s1.close()
cmd(s2, "quit!"); s2.close()
cmd(s3, "quit!"); s3.close()

for nm in (name1, name2, name3):
    sql(f"UPDATE player_progress SET hp=5000, max_hp=5000 WHERE player_id="
        f"(SELECT id FROM player WHERE name='{nm}');")
sql(f"UPDATE player SET load_room=(SELECT load_room FROM player WHERE name='{name1}') "
    f"WHERE name IN ('{name2}', '{name3}');")

s1 = relog(name1, pw1)
s2 = relog(name2, pw2)
s3 = relog(name3, pw3)
cmd(s1, "toggle pk")
cmd(s2, "toggle pk")

out = cmd(s1, f"attack {name2}")
check("you attack" in out.lower() or "you engage" in out.lower(), "combat starts")

out3 = cmd(s3, "look", 0.5)
check(f"{name1} is here, fighting {name2}." in out3,
      "a third party sees the attacker's line naming the target")
check(f"{name2} is here, fighting {name1}." in out3,
      "a third party sees the target's line naming the attacker")

out1 = cmd(s1, "look", 0.5)
check(f"{name2} is here, fighting you." in out1,
      "the attacker's own look shows the opponent as 'fighting you'")

out2 = cmd(s2, "look", 0.5)
check(f"{name1} is here, fighting you." in out2,
      "the defender's own look shows the opponent as 'fighting you' too")

# `flee` is only ~2-in-3 to succeed per attempt (cmd_flee.c's own
# placeholder odds) -- retry a bounded number of times rather than
# assume a single try clears the fight. Checked against these two
# characters' OWN lines specifically, not a blanket "fighting" absence
# -- the default starting room has real ambient traffic (other players,
# aggressive mobs) that can independently be fighting something else at
# any given moment, unrelated to this test.
out_after = ""
for _ in range(10):
    cmd(s1, "flee", 0.5)
    recv_all(s1, 1.0)
    out_after = cmd(s3, "look", 0.5)
    if f"{name2} is here, fighting" not in out_after:
        break
check(f"{name2} is here, fighting" not in out_after,
      "once combat ends, the 'fighting' clause disappears from the room listing")

s1.close(); s2.close(); s3.close()

announce_done("smoke_test_look_fighting", host, port)
print("=== ALL CHECKS PASSED ===")
