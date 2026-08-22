#!/usr/bin/env python3
"""Smoke test for the Thief `spy` skill/command (missing-skill audit
backlog, skill.c level 38, SKILL_TIER_ADVANCED). See cmd_spy.c's own doc
comment for why this is a single-hop remote glimpse (matching the
skill_help.sql roster description "Covertly watch a room from
elsewhere") rather than a port of real upstream's AFF_SCRYING toggle,
which hides a "$n looks at you" notice Tobin's own `look <target>` never
sends in the first place.

  1. A class that doesn't know "spy" (e.g. Warrior) gets the command
     gated off entirely ("Command not found").
  2. A bad/missing direction reports usage cleanly.
  3. A direction with no exit reports cleanly.
  4. A closed door blocks the view.
  5. A real, open exit shows the target room's name/description and who
     is standing in it.
  6. `help spy` describes the command.

    python3 tests/smoke_test_spy.py [host] [port]
"""
import socket
import sys
import time
from mud_test_utils import send_line, check, sql, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


def recv_all(sock, timeout=1.0, idle_gap=0.3):
    sock.settimeout(idle_gap)
    chunks = []
    deadline = time.time() + timeout
    try:
        while time.time() < deadline:
            data = sock.recv(4096)
            if not data:
                break
            chunks.append(data)
    except socket.timeout:
        pass
    return b"".join(chunks).decode(errors="replace")


def cmd(sock, line, timeout=1.0):
    send_line(sock, line)
    return recv_all(sock, timeout)


def make_char(name, pw, race="1", cls="1"):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "y"); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "new"); recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, race); recv_all(s)
    send_line(s, "1"); recv_all(s)  # territory: urban
    send_line(s, cls); recv_all(s)
    send_line(s, "done"); recv_all(s)
    send_line(s, "done"); recv_all(s)
    cmd(s, "color off")
    return s


def reconnect(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    cmd(s, "1")
    cmd(s, "color off")
    return s


announce("smoke_test_spy", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM_A = 910000 + (int(time.time() * 1000) % 40000)
ROOM_B = ROOM_A + 1
ROOM_C = ROOM_A + 2  # behind a closed door, east of A

for r, rname in ((ROOM_A, "Spy Sandbox A"), (ROOM_B, "Spy Sandbox B"), (ROOM_C, "Spy Sandbox C")):
    sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
        f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
        f"VALUES ({r},0,0,0,'{rname}','A bare sandbox room, distinct.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")

# direction 0 = north (open, A -> B); direction 1 = east (closed, A -> C)
sql(f"INSERT INTO roomexit (vnum, direction, name, description, type, "
    f"condition_flag, lock_difficulty, weight, key_num, destination) VALUES "
    f"({ROOM_A}, 0, '', '', 0, 0, 0, 0, 0, {ROOM_B});")
sql(f"INSERT INTO roomexit (vnum, direction, name, description, type, "
    f"condition_flag, lock_difficulty, weight, key_num, destination) VALUES "
    f"({ROOM_A}, 1, '', '', 0, 1, 0, 0, 0, {ROOM_C});")  # condition_flag 1 = EXIT_COND_CLOSED

thief_name, thief_pw = f"Spthief{_suffix}", "spthiefpw123"
warrior_name, warrior_pw = f"Spwar{_suffix}", "spwarpw123"
target_name, target_pw = f"Sptgt{_suffix}", "sptgtpw123"

s_thief = make_char(thief_name, thief_pw, cls="4")  # Thief
cmd(s_thief, "quit!")
s_thief.close()
sql(f"UPDATE player SET load_room={ROOM_A} WHERE name='{thief_name}';")
# SKILL_TIER_ADVANCED needs basic/combat at 100 and advanced_disc_pct > 0.
sql(f"UPDATE player_progress SET level=40, basic_disc_pct=100, combat_disc_pct=100, "
    f"advanced_disc_pct=100 WHERE player_id=(SELECT id FROM player WHERE name='{thief_name}');")
sql(f"INSERT INTO player_skill (player_id, skill_name, pct, last_gain_at) "
    f"SELECT id, 'spy', 100, 0 FROM player WHERE name='{thief_name}' "
    f"ON DUPLICATE KEY UPDATE pct=100;")
s_thief = reconnect(thief_name, thief_pw)

s_war = make_char(warrior_name, warrior_pw, cls="3")  # Warrior
cmd(s_war, "quit!")
s_war.close()
sql(f"UPDATE player SET load_room={ROOM_A} WHERE name='{warrior_name}';")
s_war = reconnect(warrior_name, warrior_pw)

s_tgt = make_char(target_name, target_pw, cls="1")  # Mage, stands in ROOM_B
cmd(s_tgt, "quit!")
s_tgt.close()
sql(f"UPDATE player SET load_room={ROOM_B} WHERE name='{target_name}';")
s_tgt = reconnect(target_name, target_pw)  # loads ROOM_B into memory

# --- gate: a Warrior doesn't know "spy" ---
check("Command not found" in cmd(s_war, "spy north"),
      "a class that doesn't know spy gets it gated off")

# --- bad/missing direction ---
check("Usage: spy" in cmd(s_thief, "spy"), "spy with no argument reports usage")
check("Usage: spy" in cmd(s_thief, "spy nowhere"), "spy with a bad direction reports usage")

# --- no exit that way ---
check("nothing that way" in cmd(s_thief, "spy up"),
      "spying a direction with no exit reports cleanly")

# --- closed door blocks the view ---
check("closed door blocks" in cmd(s_thief, "spy east"),
      "a closed door blocks the covert view")

# --- neither room is told anything happened (covert, unlike `scan`) ---
recv_all(s_tgt, 0.2)
out = cmd(s_thief, "spy north")
tgt_saw = recv_all(s_tgt, 0.3)
check(tgt_saw == "", "the spied-on room sees no notice at all")

# --- success: shows the target room's name, description, and occupant ---
check("Spy Sandbox B" in out, "a successful spy shows the target room's name")
check("bare sandbox room" in out, "a successful spy shows the target room's description")
check(target_name in out, "a successful spy shows who is standing in the target room")

# --- help ---
check("covert" in cmd(s_thief, "help spy").lower(), "help spy describes the command")

s_thief.close()
s_war.close()
s_tgt.close()
announce_done("smoke_test_spy", host, port)
print("=== ALL CHECKS PASSED ===")
