#!/usr/bin/env python3
"""Smoke test for `poofin`/`poofout` (user, 2026-07-11: "immorts should be
able to set their own enter or leave messages. Like Jesus drags his cross
in from the east. of course gender specific in the messaging").

  1. Immortal-only: a mortal gets "Command not found" from `poofin`/`poofout`.
  2. Setting a message with `$d` (direction) and `$p` (gender_possess()
     pronoun) tokens replaces the default "exits to the <dir>"/"has
     arrived" wording -- verified for both a male and a female immortal,
     confirming the pronoun actually changes with gender.
  3. `poofin none`/`poofout none` clears it, reverting to the default
     wording.

    python3 tests/smoke_test_poof.py [host] [port]
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


announce("smoke_test_poof")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM_A = 900000 + (int(time.time()) % 70000)
ROOM_B = ROOM_A + 1


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
    subprocess.run(["mariadb", "tobin", "-e", stmt], check=True)


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


sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM_A},0,0,0,'Poof Room A','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM_B},0,0,0,'Poof Room B','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
# direction 1 = east (A -> B), direction 3 = west (B -> A) -- DIR_NAMES order.
sql(f"INSERT INTO roomexit (vnum, direction, name, description, type, "
    f"condition_flag, lock_difficulty, weight, key_num, destination) "
    f"VALUES ({ROOM_A}, 1, '', '', 0, 0, 0, 0, 0, {ROOM_B});")
sql(f"INSERT INTO roomexit (vnum, direction, name, description, type, "
    f"condition_flag, lock_difficulty, weight, key_num, destination) "
    f"VALUES ({ROOM_B}, 3, '', '', 0, 0, 0, 0, 0, {ROOM_A});")

# --- 1: mortal can't set a bamf message ---
mort_name = f"Poofmort{_suffix}"
mort_pw = "Poofmortpw123"
sm = socket.create_connection((host, port), timeout=5)
make_char(sm, mort_name, mort_pw)
out = cmd(sm, "poofout drags $p cross out to the $d")
check("Command not found" in out, "poofout is refused for a mortal")
sm.close()

# --- 2: a male immortal's bamf messages use "his" ---
male_name = f"Poofmale{_suffix}"
male_pw = "Poofmalepw123"
s = socket.create_connection((host, port), timeout=5)
make_char(s, male_name, male_pw)
sql(f"UPDATE player SET gender=1 WHERE name='{male_name}';")  # 1 = male
sql(f"UPDATE player SET load_room={ROOM_A} WHERE name='{male_name}';")
cmd(s, "quit!")
s.close()
set_level(male_name, 51)
s = login(male_name, male_pw)

out = cmd(s, "poofout drags $p cross out to the $d")
check("Poofout set to: drags $p cross out to the $d" in out, "poofout confirms the stored template")
out = cmd(s, "poofin drags $p cross in from the $d")
check("Poofin set to: drags $p cross in from the $d" in out, "poofin confirms the stored template")

bystander_name = f"Poofwitness{_suffix}"
bystander_pw = "Poofwitnesspw123"
sb = socket.create_connection((host, port), timeout=5)
make_char(sb, bystander_name, bystander_pw)
sql(f"UPDATE player SET load_room={ROOM_A} WHERE name='{bystander_name}';")
cmd(sb, "quit!")
sb.close()
sb = login(bystander_name, bystander_pw)
check("Poof Room A" in cmd(sb, "look"), "the bystander lands in room A with the male immortal")

cmd(s, "east")
witness_out = recv_all(sb, timeout=0.5)
check(f"{male_name} drags his cross out to the east." in witness_out,
      "the bystander in room A sees the male immortal's custom departure message with 'his'")

sb2_name = f"Poofwittwo{_suffix}"
sb2_pw = "Bamfwit2pw123"
sb2 = socket.create_connection((host, port), timeout=5)
make_char(sb2, sb2_name, sb2_pw)
sql(f"UPDATE player SET load_room={ROOM_B} WHERE name='{sb2_name}';")
cmd(sb2, "quit!")
sb2.close()
sb2 = login(sb2_name, sb2_pw)
check("Poof Room B" in cmd(sb2, "look"), "the second bystander lands in room B")

out = cmd(s, "west")  # back to room A, then...
recv_all(sb2, timeout=0.3)
out = cmd(s, "east")
witness2_out = recv_all(sb2, timeout=0.5)
check(f"{male_name} drags his cross in from the west." in witness2_out,
      "the bystander in room B sees the male immortal's custom arrival message with 'his', direction reversed")

# --- 3: a female immortal's bamf messages use "her" ---
female_name = f"Pooffem{_suffix}"
female_pw = "Pooffempw123"
sf = socket.create_connection((host, port), timeout=5)
make_char(sf, female_name, female_pw)
sql(f"UPDATE player SET gender=2 WHERE name='{female_name}';")  # 2 = female
sql(f"UPDATE player SET load_room={ROOM_A} WHERE name='{female_name}';")
cmd(sf, "quit!")
sf.close()
set_level(female_name, 51)
sf = login(female_name, female_pw)
cmd(sf, "poofout drags $p cross out to the $d")

recv_all(sb, timeout=0.3)
out = cmd(sf, "east")
witness_out = recv_all(sb, timeout=0.5)
check(f"{female_name} drags her cross out to the east." in witness_out,
      "the female immortal's identical template renders 'her' instead of 'his'")

# --- 4: clearing reverts to the default wording ---
cmd(sf, "west")
out = cmd(sf, "poofout none")
check("Poofout cleared" in out, "poofout none clears the custom message")
recv_all(sb, timeout=0.3)
out = cmd(sf, "east")
witness_out = recv_all(sb, timeout=0.5)
check("exits to the east" in witness_out, "after clearing, the default 'exits to the <dir>' wording is back")
check("drags" not in witness_out, "the cleared custom message no longer appears")

s.close()
sf.close()
sb.close()
sb2.close()
announce_done("smoke_test_poof")
print("=== ALL CHECKS PASSED ===")
