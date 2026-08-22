#!/usr/bin/env python3
"""Smoke test for the Thief `peek` skill/command (TODO.md "Thief 'peek'
skill" -- user: "a thief skill could be added to attempt a peak at the
targets inventory").

  1. A class that doesn't know "peek" (e.g. Warrior) gets the command gated
     off entirely ("Command not found").
  2. A Thief who knows it can `peek <target>` and see what they're CARRYING
     (loose inventory) -- not what's worn (that's `look <target>`).
  3. An unrecognized target reports cleanly instead of crashing.
  4. `help peek` describes the command.

    python3 tests/smoke_test_peek.py [host] [port]
"""
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, check, sql, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


announce("smoke_test_peek", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))


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


ROOM = 900000 + (int(time.time() * 1000) % 60000)

imm_name, imm_pw = f"Pkimm{_suffix}", "pkimmpw123"
s_imm = make_char(imm_name, imm_pw, cls="3")
s_imm.close()
sql(f"UPDATE player_progress SET level=51 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{imm_name}');")
s_imm = reconnect(imm_name, imm_pw)
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Peek Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
cmd(s_imm, f"goto {ROOM}")

thief_name, thief_pw = f"Pkthief{_suffix}", "pkthiefpw123"
warrior_name, warrior_pw = f"Pkwar{_suffix}", "pkwarpw123"
target_name, target_pw = f"Pktgt{_suffix}", "pktgtpw123"

s_thief = make_char(thief_name, thief_pw, cls="4")  # Thief
cmd(s_thief, "quit!")  # clean logout -- a raw close() leaves them linkdead,
s_thief.close()        # and a linkdead reconnect resumes the OLD in-memory
                        # room, ignoring the load_room UPDATE below entirely.
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{thief_name}';")
# being_knows_skill() gates SKILL_TIER_CLASS on basic_disc_pct > 0, not
# just min_level -- a fresh character starts at 0% (same discipline gate
# smoke_test_affects.py already had to seed for an Advanced-tier skill).
# NOTE: `peek` was SKILL_TIER_COMBAT when this test was first written
# (hence combat_disc_pct below); a later rebalance moved it to
# SKILL_TIER_CLASS (skill.c) without this test being updated -- found
# stale while working on a nearby Thief skill (`spy`). Seed both so this
# stays correct across either tier.
sql(f"UPDATE player_progress SET basic_disc_pct=100, combat_disc_pct=100 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{thief_name}');")
sql(f"INSERT INTO player_skill (player_id, skill_name, pct, last_gain_at) "
    f"SELECT id, 'peek', 100, 0 FROM player WHERE name='{thief_name}' "
    f"ON DUPLICATE KEY UPDATE pct=100;")
s_thief = reconnect(thief_name, thief_pw)

s_war = make_char(warrior_name, warrior_pw, cls="3")  # Warrior
cmd(s_war, "quit!")
s_war.close()
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{warrior_name}';")
s_war = reconnect(warrior_name, warrior_pw)

s_tgt = make_char(target_name, target_pw, cls="1")  # Mage
cmd(s_tgt, "quit!")
s_tgt.close()
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{target_name}';")

item_vnum = 900000 + (int(time.time() * 1000) % 60000) + 1
sql(f"INSERT INTO obj (vnum, name, short_desc, long_desc, action_desc) VALUES "
    f"({item_vnum}, 'peek trinket testitem', 'a small peek-test trinket', "
    f"'A small trinket sits here.', '');")
target_pid_row = subprocess.run(
    ["mariadb", "tobin", "-N", "-e", f"SELECT id FROM player WHERE name='{target_name}';"],
    capture_output=True, text=True, check=True)
target_pid = target_pid_row.stdout.strip()
sql(f"INSERT INTO player_inventory (player_id, vnum, slot) VALUES ({target_pid}, {item_vnum}, -1);")

s_tgt = reconnect(target_name, target_pw)

# --- gate: a Warrior doesn't know "peek" ---
check("Command not found" in cmd(s_war, f"peek {target_name}"),
      "a class that doesn't know peek gets it gated off")

# --- unrecognized target reports cleanly ---
check("don't see them here" in cmd(s_thief, "peek NoSuchPersonAtAll"),
      "peeking a nonexistent target reports cleanly")

# --- success: Thief sees the target's carried item ---
out = cmd(s_thief, f"peek {target_name}")
check("peek trinket testitem" not in out, "the label shown is the display name, not the raw keyword string")
check("peek-test trinket" in out, "a successful peek shows the target's carried item")

# --- help ---
check("carrying" in cmd(s_thief, "help peek").lower(), "help peek describes the command")

sql(f"DELETE FROM obj WHERE vnum={item_vnum};")
sql(f"DELETE FROM player_inventory WHERE vnum={item_vnum};")

s_imm.close()
s_thief.close()
s_war.close()
s_tgt.close()
announce_done("smoke_test_peek", host, port)
print("=== ALL CHECKS PASSED ===")
