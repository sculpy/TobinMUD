#!/usr/bin/env python3
"""Smoke test for `possess`/`return` (Sneezy → Tobin feature audit,
"Switch / return (puppet a mob)"). Named `possess` since Tobin's own
`switch` already means something else (swap held items). Covers:
  1. `possess <mob>` puppets a mob's body -- `score`/`look` reflect the
     mob, not the immortal's own character.
  2. Can't possess twice in a row without `return`ing first.
  3. Can't possess a player character.
  4. Can't possess a mob someone else is already possessing.
  5. `return` restores the immortal's own body; `score` shows them again.
  6. `return` with nothing possessed is rejected.
  7. A hard disconnect while possessing restores the immortal's own
     character to the linkdead state (not the mob) -- reconnecting logs
     back into the immortal, not the puppet.

    python3 tests/smoke_test_possess.py [host] [port]
"""
import socket
import subprocess
import sys
import time

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


def announce(test_name, host=host, port=port):
    try:
        s = socket.create_connection((host, port), timeout=3)
        s.settimeout(0.5)
        try:
            while s.recv(4096):
                pass
        except socket.timeout:
            pass
        s.sendall(f"@test {test_name}\r\n".encode())
        s.settimeout(0.5)
        try:
            while s.recv(4096):
                pass
        except socket.timeout:
            pass
        s.close()
    except OSError:
        pass


def announce_done(test_name, host=host, port=port):
    announce(f"done {test_name}", host, port)


announce("smoke_test_possess")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 900000 + (int(time.time()) % 70000)
MOB_VNUM = 900000 + (int(time.time()) % 70000) + 1


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


def sql(stmt):
    subprocess.run(["mariadb", "sneezy", "-e", stmt], check=True)


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
    send_line(sock, "1"); recv_all(sock)  # race: human
    send_line(sock, "1"); recv_all(sock)  # class: mage
    send_line(sock, "done"); recv_all(sock)
    send_line(sock, "2"); recv_all(sock)  # alignment: neutral


def login(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


imm_name = f"Possimm{_suffix}"
imm_pw = "possimmpw1234"
imm2_name = f"Possimmb{_suffix}"
imm2_pw = "possimm2pw123"
mortal_name = f"Possmort{_suffix}"
mortal_pw = "possmortpw1234"

s = socket.create_connection((host, port), timeout=5)
make_char(s, imm_name, imm_pw)
set_level(imm_name, 59)
s.close()
s = login(imm_name, imm_pw)

s2 = socket.create_connection((host, port), timeout=5)
make_char(s2, imm2_name, imm2_pw)
set_level(imm2_name, 59)
s2.close()
s2 = login(imm2_name, imm2_pw)

sm = socket.create_connection((host, port), timeout=5)
make_char(sm, mortal_name, mortal_pw)
sm.close()

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Possess Sandbox','A bare sandbox room.\\n',NULL,0,0,0,0,0,0,0,0,0,0);")
cols = {
    "vnum": MOB_VNUM, "name": "'puppet'", "short_desc": "'a puppet dummy'",
    "long_desc": "'A puppet dummy stands here.'", "description": "'desc'",
    "actions": 0, "affects": 0, "faction": 0, "fact_perc": 0,
    "letter": "'A'", "attacks": 1.0,
    "class": 0, "level": 1, "tohit": 0, "ac": 0, "hpbonus": 0,
    "damage_level": 0, "damage_precision": 0, "gold": 0, "race": 0,
    "weight": 0, "height": 0, "str": 0, "bra": 0, "con": 0, "dex": 0,
    "agi": 0, "intel": 0, "wis": 0, "foc": 0, "per": 0, "cha": 0,
    "kar": 0, "spe": 0, "pos": 10, "def_position": 10, "sex": 1,
    "spec_proc": 0, "skin": 0, "vision": 0, "can_be_seen": 1,
    "max_exist": 1,
}
col_names = ",".join(cols.keys())
col_values = ",".join(str(v) for v in cols.values())
sql(f"INSERT INTO mob ({col_names}) VALUES ({col_values});")

cmd(s, f"goto {ROOM}")
cmd(s, f"load mob {MOB_VNUM}")
cmd(s2, f"goto {ROOM}")

# --- 1: possess puppets the mob ---
out = cmd(s, "possess puppet")
check("You possess" in out, "possess confirms")
out = cmd(s, "score")
check(imm_name.lower() not in out.lower(), "score no longer shows the immortal's own name while possessing")

# --- 2: can't possess twice ---
check("already possessing" in cmd(s, "possess puppet"), "can't possess again without returning first")

# --- 3: can't possess a player ---
check("don't see that mob" in cmd(s2, f"possess {mortal_name}"), "possess refuses a player character")

# --- 4: can't possess an already-possessed mob ---
check("already in control" in cmd(s2, "possess puppet"), "possess refuses a mob someone else is already puppeting")

# --- 5: return restores the immortal ---
out = cmd(s, "return")
check("return to your own body" in out, "return confirms")
out = cmd(s, "score")
check(imm_name.lower() in out.lower(), "score reflects the immortal's own character again")

# --- 6: return with nothing possessed ---
check("aren't possessing" in cmd(s, "return"), "return with nothing possessed is rejected")

# --- 7: disconnect while possessing restores the immortal, not the mob ---
cmd(s, "possess puppet")
s.close()
time.sleep(1.0)
s = login(imm_name, imm_pw)
out = cmd(s, "score")
check(imm_name.lower() in out.lower(),
      "reconnecting after a disconnect-while-possessing logs back into the immortal, not the puppet")

s.close()
s2.close()
announce_done("smoke_test_possess")
print("=== ALL CHECKS PASSED ===")
