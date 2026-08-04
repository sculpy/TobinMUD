#!/usr/bin/env python3
"""Smoke test for pool decay (user, 2026-07-11: "pools should absorb into
the ground little by little upon ticks"). obj_pool_decay_tick() (obj.c)
shrinks every ground puddle one size tier per world tick, reversing
obj_grow_pool()'s growth; a "puddle" (the smallest tier) is destroyed
outright on its next decay rather than shrinking further. Deterministic
via the immortal-only `aitick [count]` debug command (now also forces
pool decay alongside the existing mob AI ticks, cmd_aitick.c), same
precedent as smoke_test_mob_ai.py/smoke_test_bleeding.py.

  1. A single puddle decays away entirely after one forced tick.
  2. A grown pool (two `pee`s) shrinks back to a puddle after one tick,
     then disappears after a second.

    python3 tests/smoke_test_pool_decay.py [host] [port]
"""
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


announce("smoke_test_pool_decay", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 900000 + (int(time.time()) % 70000)


def set_level(name, level):
    sql(f"UPDATE player_progress SET level={level} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def make_char(sock, name, pw):
    recv_all(sock)
    send_line(sock, name); recv_all(sock)
    send_line(sock, "y"); recv_all(sock)
    send_line(sock, pw); recv_all(sock)
    send_line(sock, pw); recv_all(sock)
    send_line(sock, "new"); recv_all(sock)
    send_line(sock, name); recv_all(sock)
    send_line(sock, "1"); recv_all(sock)  # race: human (zero stat modifier)
    send_line(sock, "1"); recv_all(sock)  # territory: urban
    send_line(sock, "1"); recv_all(sock)  # class: mage
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


imm_name = f"Decayimm{_suffix}"
imm_pw = "decayimmpw123"

s = socket.create_connection((host, port), timeout=5)
make_char(s, imm_name, imm_pw)
set_level(imm_name, 51)
s.close()
s = login(imm_name, imm_pw)

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Decay Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
check("Decay Sandbox" in cmd(s, f"goto {ROOM}"), "goto lands in the SQL-bootstrapped sandbox room")

# --- 1: a single puddle decays away entirely after one tick ---
cmd(s, "pee")
out = cmd(s, "look")
check("a puddle of pee" in out.lower(), "a fresh puddle is on the ground")

cmd(s, "aitick 1")
out = cmd(s, "look")
check("pee" not in out.lower(), "the puddle is fully gone after a single decay tick")

# --- 2: a grown pool shrinks back to a puddle, then disappears ---
cmd(s, "pee")
cmd(s, "pee")
out = cmd(s, "look")
check("a pool of pee" in out.lower(), "two pees grew the puddle into a pool")

cmd(s, "aitick 1")
out = cmd(s, "look")
check("a puddle of pee" in out.lower(), "one decay tick shrinks the pool back down to a puddle")

cmd(s, "aitick 1")
out = cmd(s, "look")
check("pee" not in out.lower(), "a second decay tick fully removes the puddle")

s.close()
announce_done("smoke_test_pool_decay", host, port)
print("=== ALL CHECKS PASSED ===")
