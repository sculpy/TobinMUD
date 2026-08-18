#!/usr/bin/env python3
"""Smoke test for `dodge` (Unimplemented skills/spells backlog, Session
158 audit: Thief, skill.c level 1 -- passive avoidance). See combat.c's
to-hit modifier block.

`dodge` is a passive: it fires as a defender-side to-hit reduction on
every incoming swing, training via skill_learn_from_doing() the same way
`focused avoidance`/`advanced defense` do. This proves the hook actually
runs in real combat -- the skill starts at 0 and a real hit taken trains
it off the floor (the deterministic first-attempt guarantee
skill_learn_from_doing() documents), which can only happen if the new
combat.c dodge block executed.

    python3 tests/smoke_test_dodge.py [host] [port]
"""
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26 ** i) % 26) for i in range(4))
ROOM = 955000 + (int(time.time()) % 20000)


def query(stmt):
    return subprocess.run(["mariadb", "-N", "tobin", "-e", stmt],
                          check=True, capture_output=True, text=True).stdout.strip()


def skill_pct(name, skill_name):
    out = query(f"SELECT pct FROM player_skill WHERE player_id="
                f"(SELECT id FROM player WHERE name='{name}') AND skill_name='{skill_name}';")
    return int(out) if out else 0


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


announce("smoke_test_dodge", host, port)

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Dodge Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")

# Immortal attacker
iname, ipw = f"Ddi{_suffix}", "ddipw1234567"
si = make_char(iname, ipw, 3)
cmd(si, "quit!"); si.close()
sql(f"UPDATE player_progress SET level=60 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{iname}');")
si = relog(iname, ipw)

# Defender: a Thief who KNOWS dodge (class discipline practiced), with a
# big HP pool so it survives long enough to take several hits.
dname, dpw = f"Ddt{_suffix}", "ddtpw1234567"
sd = make_char(dname, dpw, 4)  # Thief
cmd(sd, "quit!"); sd.close()
# class 3 == CLASS_THIEF in the stored player_class_t enum (Mage=0,
# Cleric=1, Warrior=2, Thief=3, Druid=4, Monk=5) -- NOT the creation-menu
# choice number. Creation already made a Thief; this just pins it.
sql(f"UPDATE player SET class=3, load_room={ROOM} WHERE name='{dname}';")
sql(f"UPDATE player_progress SET level=30, basic_disc_pct=100, combat_disc_pct=100, "
    f"advanced_disc_pct=100, hp=800, max_hp=800 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{dname}');")
sd = relog(dname, dpw)
check("Dodge Sandbox" in cmd(sd, "look"), "the Thief defender lands in the sandbox room")
check(skill_pct(dname, "dodge") == 0, "dodge starts untrained")

# Immortal attacks the Thief -> Thief is the defender, dodge is checked
# on each incoming swing.
cmd(si, f"goto {ROOM}")
send_line(si, f"hit {dname}")
time.sleep(3.0)
recv_all(si, 0.3)
recv_all(sd, 0.3)

check(skill_pct(dname, "dodge") >= 1, "dodge trains from real incoming swings (the passive hook fired)")

cmd(si, "quit!"); si.close()
cmd(sd, "quit!"); sd.close()

announce_done("smoke_test_dodge", host, port)
print("=== ALL CHECKS PASSED ===")
