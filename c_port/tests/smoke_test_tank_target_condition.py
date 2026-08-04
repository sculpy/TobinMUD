#!/usr/bin/env python3
"""Smoke test for the tank/target condition prompt display (user,
2026-08-03, refined across several messages this session):
  1. "when fighting display tank condition and target condition, in
     words not %"
  2. "HP: 176 Gold: 3120 Vit: 68 ExpNeed: 14555 > <Tank: condition>
     <Vict: condition> all on one line just the one word condition
     perfect, awful etc. and tastefully colored like item condition"
  3. "color codes arent working" -- a real bug: descriptor_write() (the
     raw socket primitive the prompt was writing through) never ran
     color tags through colorstring_translate() at all, so "<g>" etc
     showed up as literal text. Fixed by routing the prompt through
     descriptor_send() instead (game_loop.c).
  4. "and use () instead of <>" -- visible delimiters that can't be
     confused with the color tags they wrap.
  5. "instead of <Tank: > and <Vict: > if the tank is you the character
     then no need to display condition, and for vict, substitute with
     the keyword/name of the mob you are fighting. so if groupped, the
     others in a group should see the tank condition, but the tank can
     skip that."
  6. "(Vict: An obedient zombie good) should be (zombie: good)" -- drop
     the literal Tank:/Vict: label entirely in favor of the target's
     own first keyword (prompt_short_keyword(), game_loop.c) --
     e.g. mob vnum 31351's raw name is "zombie obedient", so its first
     token "zombie" is what's shown, not the full "an obedient zombie"
     display name.

Final shape (game_loop.c's prompt_append_tank_vict()): the character
actually fighting sees only "(<opponent keyword>: <word>)" (their own
condition is redundant with the HP number already on the same line);
a GROUPED bystander who isn't fighting themselves sees "(<fighter
keyword>: <word>)" for whichever grouped member is actually engaged.

Covers:
  1. The fighter's own prompt shows "(<opponent's first keyword>:
     <word>)" -- NOT a "(Tank: ...)"/self-condition tag.
  2. The word is a real color tag once translated (color on), not
     literal "<g>"/"<1>" text (the "color codes arent working" bug).
  3. A grouped bystander (not fighting) sees "(<fighter's own name>:
     <word>)" for their engaged groupmate.
  4. Once combat ends, the tags disappear from the prompt entirely.

    python3 tests/smoke_test_tank_target_condition.py [host] [port]
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


announce("smoke_test_tank_target_condition", host, port)


def make_char(name, pw, class_num):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw, "new", name, "1", "1", str(class_num), "done", "done"):
        send_line(s, step)
        recv_all(s)
    return s


def relog(name, pw, color_on=False):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, pw, "1"):
        send_line(s, step)
        recv_all(s)
    cmd(s, "color on" if color_on else "color off")
    return s


name1, pw1 = f"Tankc{_suffix}", "tankcpw12345"
name2, pw2 = f"Tgtc{_suffix}", "tgtcpw123456"
name3, pw3 = f"Byst{_suffix}", "bystpw123456"
s1 = make_char(name1, pw1, 3)  # Warrior (the fighter)
s2 = make_char(name2, pw2, 3)  # Warrior (the opponent)
s3 = make_char(name3, pw3, 3)  # Warrior (grouped bystander)
cmd(s1, "quit!"); s1.close()
cmd(s2, "quit!"); s2.close()
cmd(s3, "quit!"); s3.close()

for nm in (name1, name2, name3):
    sql(f"UPDATE player_progress SET hp=5000, max_hp=5000 WHERE player_id="
        f"(SELECT id FROM player WHERE name='{nm}');")
sql(f"UPDATE player SET load_room=(SELECT load_room FROM player WHERE name='{name1}') "
    f"WHERE name IN ('{name2}', '{name3}');")

# name1 stays with color OFF (easy substring checks); name3 (the group
# bystander) goes color ON specifically to prove tags actually render
# as color once translated, not literal "<g>"/"<1>" text (the "color
# codes arent working" bug this session found and fixed).
s1 = relog(name1, pw1, color_on=False)
s2 = relog(name2, pw2, color_on=False)
s3 = relog(name3, pw3, color_on=True)
cmd(s1, "toggle pk")
cmd(s2, "toggle pk")

# Group s1 (the fighter) and s3 (the bystander) together -- `follow`
# establishes the master/follower link first, THEN the leader `group`s
# them in (cmd_group.c: follow alone doesn't grant group benefits).
cmd(s3, "follow " + name1)
cmd(s1, f"group {name3}")
group_out = cmd(s1, "group")
check(name3.lower() in group_out.lower() or "grouped" in group_out.lower(),
      "group formed between the fighter and the bystander")

out = cmd(s1, f"attack {name2}")
check("you attack" in out.lower() or "you engage" in out.lower(), "combat starts")

seen_vict_tag = False
seen_tank_tag_for_bystander = False
seen_colored = False
for _ in range(6):
    out1 = cmd(s1, "look", 1.0)
    out3 = cmd(s3, "look", 1.0)

    # Both PC names are already single-word, so the "first keyword"
    # IS the full name -- the tag is "(<name>: <word>)", no Tank:/Vict:
    # label at all.
    if re.search(rf"\({re.escape(name2)}:\s*\w[\w ]*?\)", out1, re.IGNORECASE):
        seen_vict_tag = True
    check(f"({name1}:" not in out1,
          "the fighter's OWN prompt never shows a self-condition tag for themselves")

    # s3 is logged in with color ON (see below) specifically to prove
    # real ANSI escapes render -- so unlike out1's check above, the
    # condition word here may be wrapped in an escape sequence
    # (e.g. "(Tankcxcew: \x1b[0;32mexcellent\x1b[0m)"); tolerate an
    # optional ESC[...m run on either side of the word.
    ansi = r"(?:\x1b\[[0-9;]*m)*"
    if re.search(rf"\({re.escape(name1)}:\s*{ansi}\w[\w ]*?{ansi}\)", out3, re.IGNORECASE):
        seen_tank_tag_for_bystander = True
    if "\x1b[" in out3 and f"({name1}:" in out3:
        seen_colored = True

    if seen_vict_tag and seen_tank_tag_for_bystander and seen_colored:
        break

check(seen_vict_tag, "the fighter sees '(<opponent's keyword>: <word>)' in their own prompt")
check(seen_tank_tag_for_bystander,
      "the grouped bystander (not fighting) sees '(<fighter's keyword>: <word>)' instead")
check(seen_colored, "the bystander's tag renders as a real ANSI color escape, not literal '<g>'/'<1>' text")

cmd(s1, "flee")
recv_all(s1, 1.0)
out_after = cmd(s1, "look")
check(f"({name2}:" not in out_after,
      "once combat ends, the tag disappears from the prompt entirely")

s1.close(); s2.close(); s3.close()

announce_done("smoke_test_tank_target_condition", host, port)
print("=== ALL CHECKS PASSED ===")
