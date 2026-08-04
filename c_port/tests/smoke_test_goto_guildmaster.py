#!/usr/bin/env python3
"""Smoke test for `goto`'s mortal-usable landmark forms (user 2026-07-12,
first round: "add a goto class function that mortals can do to help find
their guildmasters"; second round: "goto guildmaster should give them
directions, not transfer. also add a goto rent, goto surplus for now with
goto expanding for mortals"). Covers:

  1. `goto guildmaster` gives a mortal Mage walking directions to a real
     seeded guildmaster of their own class -- it does NOT teleport them.
  2. `goto rent` gives directions to the inn (room 557).
  3. `goto surplus` gives directions to the surplus store (room 563).
  4. Already being in the target room reports "already there" instead of
     a (meaningless) direction list.
  5. A mortal still cannot `goto <vnum>` or `goto <player>` -- only the
     three landmark forms are open to them.
  6. An immortal can still use the full `goto <vnum>` teleport form.

Note: a "class with no seeded guildmaster anywhere" negative case was
deliberately left out -- a live DB check (`SELECT DISTINCT class FROM mob
WHERE name LIKE '%guildmaster%'`) showed every one of Sneezy's class bits
(1,2,4,8,16,32,64,128) already has a seeded guildmaster somewhere in the
world, so every one of Tobin's 6 real classes has a real match and there's
no safe way to construct a true "nobody trains this" scenario without
mutating real seed content. The refusal path itself (`if (!goal)` in
cmd_goto.c's goto_send_directions()) is a simple early-return, low risk.

    python3 tests/smoke_test_goto_guildmaster.py [host] [port]
"""
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

RENT_ROOM = 557    # The Roaring Lion Inn (user-specified 2026-07-12)
SURPLUS_ROOM = 563 # Surplus (user-specified 2026-07-12)


announce("smoke_test_goto_guildmaster", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time() * 1000) // 26**i) % 26) for i in range(4))


def make_char(sock, name, pw, class_choice):
    recv_all(sock)
    send_line(sock, name); recv_all(sock)
    send_line(sock, "y"); recv_all(sock)
    send_line(sock, pw); recv_all(sock)
    send_line(sock, pw); recv_all(sock)
    send_line(sock, "new"); recv_all(sock)
    send_line(sock, name); recv_all(sock)
    send_line(sock, "1"); recv_all(sock)  # race: human
    send_line(sock, "1"); recv_all(sock)  # territory: urban
    send_line(sock, class_choice); recv_all(sock)
    send_line(sock, "done"); recv_all(sock)
    send_line(sock, "done"); recv_all(sock)  # alignment: neutral


def login(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


pw = "gotogmpw123"

# --- 1: mortal mage gets directions to a real seeded guildmaster ---
mage_name = f"Ggmmage{_suffix}"
s = socket.create_connection((host, port), timeout=5)
make_char(s, mage_name, pw, "1")  # class: mage
cmd(s, "color off")
room_before = cmd(s, "look")
out = cmd(s, "goto guildmaster", timeout=2.0)
check("You vanish in a puff of smoke" not in out, "goto guildmaster does NOT teleport the mortal")
check("To reach a guildmaster of your discipline:" in out, "goto guildmaster gives a direction list")
check("room" in out and "away" in out, "goto guildmaster reports a hop count")
room_after = cmd(s, "look")
check(room_before == room_after, "the mortal is still in the same room after goto guildmaster")

# --- 2 & 3: goto rent / goto surplus give directions too ---
out = cmd(s, "goto rent", timeout=2.0)
check("To reach the inn:" in out or "already there -- the inn is right here" in out,
      "goto rent gives directions to the inn")
check("You vanish" not in out, "goto rent does not teleport")

out = cmd(s, "goto surplus", timeout=2.0)
check("To reach the surplus store:" in out or "already there -- the surplus store is right here" in out,
      "goto surplus gives directions to the surplus store")
check("You vanish" not in out, "goto surplus does not teleport")

# --- 4: already being at the target reports "already there" ---
# --- 5: a mortal still cannot use the vnum/player forms ---
out = cmd(s, "goto 100")
check("Command not found" in out, "goto <vnum> is refused for a mortal")
out = cmd(s, f"goto {mage_name}")
check("Command not found" in out, "goto <player> is refused for a mortal")
s.close()

# --- 6: an immortal can still use the vnum form, and landmark forms
# report "already there" once actually standing in the target room ---
imm_name = f"Ggmimm{_suffix}"
s_imm = socket.create_connection((host, port), timeout=5)
make_char(s_imm, imm_name, pw, "1")
cmd(s_imm, "quit!")
s_imm.close()
sql(f"UPDATE player_progress SET level=51 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{imm_name}');")
s_imm = login(imm_name, pw)
out = cmd(s_imm, "goto 100")
check("Center Square" in out, "an immortal can still goto a room by vnum")

out = cmd(s_imm, f"goto {RENT_ROOM}")
check("You vanish in a puff of smoke" in out, "immortal vnum teleport still works for the inn room")
out = cmd(s_imm, "goto rent", timeout=2.0)
check("already there -- the inn is right here" in out,
      "goto rent reports 'already there' once standing in the inn")

out = cmd(s_imm, f"goto {SURPLUS_ROOM}")
check("You vanish in a puff of smoke" in out, "immortal vnum teleport still works for the surplus room")
out = cmd(s_imm, "goto surplus", timeout=2.0)
check("already there -- the surplus store is right here" in out,
      "goto surplus reports 'already there' once standing in the surplus store")

s_imm.close()
announce_done("smoke_test_goto_guildmaster", host, port)
print("=== ALL CHECKS PASSED ===")
