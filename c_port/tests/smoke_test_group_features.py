#!/usr/bin/env python3
"""Smoke test for the group functionality rewrite (TODO.md priority item,
user 2026-07-30): leader/follower movement, `gtell`/`gt` group tell, and
`assist` in combat.

    python3 tests/smoke_test_group_features.py [host] [port]
"""
import re
import socket
import subprocess
import sys
import time

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))


def recv_all(sock, timeout=1.0):
    sock.settimeout(timeout)
    chunks = []
    try:
        while True:
            data = sock.recv(4096)
            if not data:
                break
            chunks.append(data)
    except socket.timeout:
        pass
    return b"".join(chunks).decode(errors="replace")


def send_line(sock, line):
    sock.sendall((line + "\r\n").encode())


def cmd(sock, line, timeout=1.0):
    send_line(sock, line)
    return recv_all(sock, timeout)


def check(condition, message):
    if not condition:
        raise AssertionError(message)
    print(f">>> OK: {message}")


def strip(s):
    return re.sub(r"\x1b\[[0-9;]*m", "", s)


def sql(stmt):
    subprocess.run(["mariadb", "tobin", "-e", stmt], check=True)


CLASS_WARRIOR = 2


def make_char(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "y"); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "y"); recv_all(s)
    send_line(s, ""); recv_all(s)
    send_line(s, "new"); recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "1"); recv_all(s)
    send_line(s, "1"); recv_all(s)
    send_line(s, "done"); recv_all(s)
    send_line(s, "done"); recv_all(s)
    return s


def make_single(prefix, cls, level=None, room=None):
    name, pw = f"{prefix}{_suffix}", f"{prefix}pw12345"
    s0 = make_char(name, pw)
    cmd(s0, "quit!"); s0.close()
    sql(f"UPDATE player SET class={cls} WHERE name='{name}';")
    if level is not None:
        sql(f"UPDATE player_progress SET level={level} WHERE player_id="
            f"(SELECT id FROM player WHERE name='{name}');")
    if room is not None:
        sql(f"UPDATE player SET load_room={room} WHERE name='{name}';")
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s, 1.0)
    cmd(s, "color off")
    cmd(s, "toggle pk")
    return name, s


print("=== Group Functionality Test ===\n")

# --- setup: two rooms with a real exit, both in the sandbox range ---
ROOM_A = 970000 + (int(time.time()) % 9000)
ROOM_B = ROOM_A + 1
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM_A},0,0,0,'Group Sandbox A','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0),"
    f"({ROOM_B},1,0,0,'Group Sandbox B','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO roomexit (vnum,direction,name,description,type,condition_flag,"
    f"lock_difficulty,weight,key_num,destination) VALUES "
    f"({ROOM_A},0,'','',0,0,0,0,0,{ROOM_B});")

nameA, sA = make_single("Grpldr", CLASS_WARRIOR, level=10, room=ROOM_A)
nameB, sB = make_single("Grpflw", CLASS_WARRIOR, level=10, room=ROOM_A)

# =================== 1. gtell / gt ===================
out = strip(cmd(sB, f"follow {nameA}"))
check("now follow" in out.lower(), "follower attaches via `follow`")
out = strip(cmd(sA, "group all"))
check("group in all" in out.lower(), "leader groups in the follower")

recv_all(sB, 0.2)
out = strip(cmd(sA, "gt hello group"))
check("you group-tell" in out.lower(), "leader's own gtell echo")
out_b = strip(recv_all(sB, 0.5))
check("group-tells" in out_b.lower() and "hello group" in out_b.lower(),
      "follower receives the gtell (via the 'gt' abbreviation)")

# =================== 2. leader/follower movement ===================
recv_all(sB, 0.2)
send_line(sA, "north")
recv_all(sA, 0.5)
out_b = strip(recv_all(sB, 0.5))
check("you follow" in out_b.lower(), "follower gets a 'You follow' message")
check("group sandbox b" in out_b.lower(), "follower's own look shows the new room")

out = strip(cmd(sA, "look"))
check(nameB.lower() in out.lower(), "the follower is physically in the new room with the leader")

# =================== 3. assist ===================
(nameC, sC) = make_single("Grpenmy", CLASS_WARRIOR, level=10, room=ROOM_B)
send_line(sA, f"attack {nameC}")
recv_all(sA, 0.5)
recv_all(sC, 0.3)
out = strip(cmd(sB, f"assist {nameA}"))
check("wade in" in out.lower(), "assist succeeds, joining the leader's fight")
out_c = strip(recv_all(sC, 0.5))
check("wades in" in out_c.lower(), "the enemy sees the assister join in")

sA.close(); sB.close(); sC.close()

print("\n=== ALL CHECKS PASSED ===")
