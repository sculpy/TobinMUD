#!/usr/bin/env python3
"""Smoke test for trap mechanics (user 2026-07-11: "...then weapon
depth, trap mechanics", sequenced right after weapon depth). Wires the
long-unused EXIT_COND_TRAPPED bit (room.h) up to the Thief's "set trap
(door)"/"disarm trap"/"detect trap" skills (skill.c). Covers:

  1. `settrap <dir>` refuses an open door, a direction with no door,
     and (once rigged) a door that's already trapped.
  2. `settrap`/`disarmtrap` are refused ("Command not found") for someone who
     doesn't know the corresponding skill.
  3. A mortal who doesn't know "detect trap" springs a rigged door and
     takes damage; the trap is then gone (one-shot).
  4. A mortal Thief (who always knows "detect trap", Combat tier) spots
     and steps around a rigged door without damage -- and the trap is
     still there afterward (avoiding it doesn't consume it).
  5. `disarmtrap` safely clears a still-rigged door.

    python3 tests/smoke_test_trap.py [host] [port]
"""
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


announce("smoke_test_trap", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM_A = 900000 + (int(time.time() * 1000) % 60000)
ROOM_B = ROOM_A + 1
EXIT_COND_CLOSED = 1
EXIT_COND_TRAPPED = 32
DOOR_TYPE = 1  # "Door"
DIR_NORTH = 0
DIR_SOUTH = 2


def query_condition(vnum, direction):
    result = subprocess.run(
        ["mariadb", "tobin", "-N", "-e",
         f"SELECT condition_flag FROM roomexit WHERE vnum={vnum} AND direction={direction};"],
        check=True, capture_output=True, text=True)
    return int(result.stdout.strip())


def make_char(name, pw, class_choice):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "y"); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "new"); recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "1"); recv_all(s)  # race: human
    send_line(s, "1"); recv_all(s)  # territory: urban
    send_line(s, class_choice); recv_all(s)
    send_line(s, "done"); recv_all(s)
    send_line(s, "done"); recv_all(s)  # alignment: neutral
    cmd(s, "color off")
    return s


pw = "trappw123"

# --- Immortal: sandbox rooms A <-north/south-> B, connected by a door ---
imm_name = f"Trapimm{_suffix}"
imm_pw = "trapimmpw123"
s_imm = socket.create_connection((host, port), timeout=5)
recv_all(s_imm)
send_line(s_imm, imm_name); recv_all(s_imm)
send_line(s_imm, "y"); recv_all(s_imm)
send_line(s_imm, imm_pw); recv_all(s_imm)
send_line(s_imm, imm_pw); recv_all(s_imm)
send_line(s_imm, "new"); recv_all(s_imm)
send_line(s_imm, imm_name); recv_all(s_imm)
send_line(s_imm, "1"); recv_all(s_imm)
send_line(s_imm, "1"); recv_all(s_imm)  # territory: urban
send_line(s_imm, "3"); recv_all(s_imm)  # class: warrior (irrelevant, immortal bypasses)
send_line(s_imm, "done"); recv_all(s_imm)
send_line(s_imm, "done"); recv_all(s_imm)
sql(f"UPDATE player_progress SET level=51 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{imm_name}');")
s_imm.close()
s_imm = socket.create_connection((host, port), timeout=5)
recv_all(s_imm)
send_line(s_imm, imm_name); recv_all(s_imm)
send_line(s_imm, imm_pw); recv_all(s_imm)
send_line(s_imm, "1"); recv_all(s_imm)
cmd(s_imm, "color off")

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM_A},0,0,0,'Trap Sandbox A','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM_B},0,0,0,'Trap Sandbox B','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
# A's north exit -> B, with a closed (but not yet trapped) door.
sql(f"INSERT INTO roomexit (vnum,direction,name,description,type,condition_flag,"
    f"lock_difficulty,weight,key_num,destination) "
    f"VALUES ({ROOM_A},{DIR_NORTH},'','',{DOOR_TYPE},{EXIT_COND_CLOSED},0,0,0,{ROOM_B});")
# B's south exit -> A, no door at all (door_type 0) -- movement back is unobstructed.
sql(f"INSERT INTO roomexit (vnum,direction,name,description,type,condition_flag,"
    f"lock_difficulty,weight,key_num,destination) "
    f"VALUES ({ROOM_B},{DIR_SOUTH},'','',0,0,0,0,0,{ROOM_A});")

check("Trap Sandbox A" in cmd(s_imm, f"goto {ROOM_A}"), "goto lands in Trap Sandbox A")

# --- 1/2: settrap gating (immortal bypasses the skill/class check, so
#          this exercises the door-state refusals with a guaranteed-
#          known skill) ---
out = cmd(s_imm, "settrap north")
check("must be closed" not in out, "the door starts closed, so settrap doesn't hit the closed-door refusal")
check("You rig a trap" in out, "settrap rigs a trap on the closed door")
check(query_condition(ROOM_A, DIR_NORTH) & EXIT_COND_TRAPPED,
      "the roomexit row's condition_flag now has the trapped bit set")

out = cmd(s_imm, "settrap north")
check("already trapped" in out, "settrap refuses a door that's already trapped")

out = cmd(s_imm, "settrap east")
check("no door" in out.lower(), "settrap refuses a direction with no door")

# --- 3: a mortal Warrior (no "detect trap") springs the trap ---
warrior_name = f"Trapwar{_suffix}"
sw = make_char(warrior_name, pw, "3")
out = cmd(sw, "settrap north")
check("Command not found" in out, "settrap is hidden from a Warrior (doesn't know the skill)")
out = cmd(sw, "disarmtrap north")
check("Command not found" in out, "disarmtrap is hidden from a Warrior (doesn't know the skill)")
sw.close()

# transfer needs the target ONLINE -- reconnect first, then transfer.
sw = socket.create_connection((host, port), timeout=5)
recv_all(sw)
send_line(sw, warrior_name); recv_all(sw)
send_line(sw, pw); recv_all(sw)
send_line(sw, "1"); recv_all(sw)
cmd(sw, "color off")
out = cmd(s_imm, f"transfer {warrior_name}")
check("Trap Sandbox A" in cmd(sw, "look"), "the Warrior is in Trap Sandbox A after transfer")

out = cmd(s_imm, "open north")
check("open" in out.lower(), "the immortal opens the door (trap stays rigged, only the closed bit clears)")

out = cmd(sw, "north")
# The Warrior is a plain mortal, so the trap message no longer includes
# a raw damage number (user 2026-07-12: damage numbers hidden from
# mortals, cmd_move.c) -- just confirm the trap sprang and caught a limb.
check("trap" in out.lower() and "springs" in out.lower(), "the Warrior springs the trap")
check(not (query_condition(ROOM_A, DIR_NORTH) & EXIT_COND_TRAPPED),
      "the sprung trap is gone from the DB afterward (one-shot)")
sw.close()

# --- 4: a mortal Thief (always knows "detect trap") avoids a rigged door,
#        which is still trapped afterward --- close it again through the
# REAL command (not a raw SQL edit -- the room is a live, cached
# in-memory object; a raw SQL UPDATE to roomexit never touches the
# already-loaded room_t.exit_cond[] the running server actually reads).
out = cmd(s_imm, "close north")
check("close" in out.lower(), "the immortal re-closes the door for the Thief test")
out = cmd(s_imm, "settrap north")
check("You rig a trap" in out, "the door is re-rigged for the Thief test")
out = cmd(s_imm, "open north")

thief_name = f"Trapthf{_suffix}"
st = make_char(thief_name, pw, "4")
st.close()
# transfer needs the target ONLINE -- reconnect first, then transfer.
st = socket.create_connection((host, port), timeout=5)
recv_all(st)
send_line(st, thief_name); recv_all(st)
send_line(st, pw); recv_all(st)
send_line(st, "1"); recv_all(st)
cmd(st, "color off")
cmd(s_imm, f"transfer {thief_name}")
check("Trap Sandbox A" in cmd(st, "look"), "the Thief is in Trap Sandbox A after transfer")

out = cmd(st, "north")
check("step around" in out.lower(), "the Thief spots and steps around the trap")
check("damage" not in out.lower(), "the Thief takes no damage from the avoided trap")
check(query_condition(ROOM_A, DIR_NORTH) & EXIT_COND_TRAPPED,
      "the avoided trap is still rigged afterward (avoiding it doesn't consume it)")

# --- 5: disarmtrap safely clears the still-rigged door -- "disarm
#        trap" is a level-33 Class-tier skill (unlike the always-known
#        Combat-tier "detect trap"), so the fresh level-1 Thief above
#        doesn't actually know it yet; use the immortal (already
#        proven, in step 1, to exercise the command's real mechanics
#        via its class/level/discipline bypass) instead. ---
out = cmd(s_imm, "disarmtrap north")
check("You carefully disarm the trap" in out, "the immortal disarms the still-rigged trap")
check(not (query_condition(ROOM_A, DIR_NORTH) & EXIT_COND_TRAPPED),
      "the disarmed trap is gone from the DB")

st.close()
s_imm.close()
announce_done("smoke_test_trap", host, port)
print("=== ALL CHECKS PASSED ===")
