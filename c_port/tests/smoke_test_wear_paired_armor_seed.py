#!/usr/bin/env python3
"""Smoke test for db/tobin/wear_paired_armor_seed.sql (TODO.md's WEAR_PAIRED
armor follow-up): Rare/Legendary-material armor in the 4 paired-eligible
slots (legs/feet/hands/arms) now carries WEAR_PAIRED (bit 512), a real
material-tier-based criterion (user-approved 2026-08-22) rather than a
keyword guess -- every candidate item is described singular ("a boot"),
so there was no textual signal to key off. The WEAR_PAIRED mechanic
itself (occupancy, refusals, load-restore) is already covered end to end
by smoke_test_wear_paired.py against synthetic objects; this test only
checks the SEED itself: a real Legendary-tier leg item is paired, a real
Fine-tier one is not.

    python3 tests/smoke_test_wear_paired_armor_seed.py [host] [port]
"""
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
LEGENDARY_LEGGING = 1127   # "a legging of admantium", material=161 (Legendary)
FINE_LEGGING = 469         # "a polished iron legging", material=158 (Fine)


def make_char(name, pw, cls):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw, "new", name, "1", "1", cls, "done", "done"):
        send_line(s, step)
        recv_all(s)
    cmd(s, "quit!")
    s.close()


def relog(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


announce("smoke_test_wear_paired_armor_seed", host, port)

ROOM = 978500 + (int(time.time()) % 1000)
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Paired Armor Seed Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")

imm_name, imm_pw = f"Wpsimm{_suffix}", "wpsimmpw12345"
make_char(imm_name, imm_pw, "1")
sql(f"UPDATE player_progress SET level=60 WHERE player_id=(SELECT id FROM player WHERE name='{imm_name}');")
si = relog(imm_name, imm_pw)
cmd(si, f"goto {ROOM}")

war_name, war_pw = f"Wpswar{_suffix}", "wpswarpw12345"
make_char(war_name, war_pw, "3")  # Warrior
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{war_name}';")
sw = relog(war_name, war_pw)

sockets = [si, sw]
try:
    # --- a real Legendary-tier legging carries WEAR_PAIRED and fills both legs ---
    check("You conjure" in cmd(si, f"load obj {LEGENDARY_LEGGING}"), "imm loads the legendary legging")
    cmd(si, "drop legging")
    check("you get" in cmd(sw, "get legging").lower(), "warrior picks up the legendary legging")
    out = cmd(sw, "wear legging")
    check("you wear" in out.lower(), "wear the legendary legging")

    check("You conjure" in cmd(si, f"load obj {LEGENDARY_LEGGING}"), "imm loads a second one")
    cmd(si, "drop legging")
    check("you get" in cmd(sw, "get legging").lower(), "warrior picks up a second legendary legging")
    out2 = cmd(sw, "wear legging")
    check("already wearing something there" in out2.lower(),
          "the legendary legging occupies BOTH legs -- no room for a second")
    cmd(sw, "remove legging")

    # --- a real Fine-tier legging does NOT carry WEAR_PAIRED -- both legs
    # can each hold one independently ---
    check("You conjure" in cmd(si, f"load obj {FINE_LEGGING}"), "imm loads the fine-tier legging")
    cmd(si, "drop legging")
    check("you get" in cmd(sw, "get legging").lower(), "warrior picks up the fine-tier legging")
    out3 = cmd(sw, "wear legging")
    check("you wear" in out3.lower(), "wear the fine-tier legging (one leg)")

    check("You conjure" in cmd(si, f"load obj {FINE_LEGGING}"), "imm loads a second fine-tier legging")
    cmd(si, "drop legging")
    check("you get" in cmd(sw, "get legging").lower(), "warrior picks up a second fine-tier legging")
    out4 = cmd(sw, "wear legging")
    check("you wear" in out4.lower(),
          "the fine-tier legging is NOT paired -- the other leg is still free")

    announce_done("smoke_test_wear_paired_armor_seed", host, port)
    print("=== ALL CHECKS PASSED ===")
finally:
    for sock in sockets:
        try:
            sock.close()
        except OSError:
            pass
    sql(f"DELETE FROM player_progress WHERE player_id IN (SELECT id FROM player WHERE name LIKE 'Wps%{_suffix}');")
    sql(f"DELETE FROM player_attrs WHERE player_id IN (SELECT id FROM player WHERE name LIKE 'Wps%{_suffix}');")
    sql(f"DELETE FROM player_skill WHERE player_id IN (SELECT id FROM player WHERE name LIKE 'Wps%{_suffix}');")
    sql(f"DELETE FROM player WHERE name LIKE 'Wps%{_suffix}';")
    sql(f"DELETE FROM room WHERE vnum={ROOM};")
