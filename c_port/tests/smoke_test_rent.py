#!/usr/bin/env python3
"""Smoke test for `rent` (Sneezy port, user 2026-07-12: "make rent work
from sneezy"). Covers:

  1. `rent` is refused while fighting.
  2. `rent` confirms delivery, announces to the room, and cleanly ends
     the session (back at the account menu, connection still alive).
  3. Reconnecting after enough real time has passed heals HP (capped at
     max_hp) and clears the `rented_at` marker so it only fires once.

    python3 tests/smoke_test_rent.py [host] [port]
"""
import re
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


announce("smoke_test_rent")

_suffix = "".join(chr(ord("a") + (int(time.time() * 1000) // 26**i) % 26) for i in range(4))


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


def query(stmt):
    out = subprocess.run(["mariadb", "sneezy", "-N", "-e", stmt],
                          check=True, capture_output=True, text=True)
    return out.stdout.strip()


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
    send_line(sock, "done"); recv_all(sock)  # alignment: neutral


def login(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


def rented_at(name):
    return int(query(f"SELECT rented_at FROM player_progress WHERE player_id="
                      f"(SELECT id FROM player WHERE name='{name}');"))


def db_hp(name):
    return int(query(f"SELECT hp FROM player_progress WHERE player_id="
                      f"(SELECT id FROM player WHERE name='{name}');"))


def db_max_hp(name):
    return int(query(f"SELECT max_hp FROM player_progress WHERE player_id="
                      f"(SELECT id FROM player WHERE name='{name}');"))


imm_name = f"Rentimm{_suffix}"
imm_pw = "rentimmpw123"
mort_name = f"Rentmor{_suffix}"
mort_pw = "rentmorpw123"

s_imm = socket.create_connection((host, port), timeout=5)
make_char(s_imm, imm_name, imm_pw)
s_imm.close()
sql(f"UPDATE player_progress SET level=51 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{imm_name}');")
s_imm = login(imm_name, imm_pw)

s_mort = socket.create_connection((host, port), timeout=5)
make_char(s_mort, mort_name, mort_pw)
s_mort.close()
s_mort = login(mort_name, mort_pw)

# --- 1: rent is refused while fighting ---
cmd(s_imm, "goto 100")
cmd(s_imm, f"hit {mort_name}")
time.sleep(0.6)
out = cmd(s_mort, "rent")
check("can't rent while fighting" in out, "rent is refused while fighting")
s_mort.close()  # abrupt close -- this character isn't needed again

# --- 2: rent confirms, announces, and ends the session cleanly ---
bystander_name = f"Rentwit{_suffix}"
bystander_pw = "rentwitpw123"
sb = socket.create_connection((host, port), timeout=5)
make_char(sb, bystander_name, bystander_pw)
sb.close()
sql(f"UPDATE player SET load_room=100 WHERE name='{bystander_name}';")
sb = login(bystander_name, bystander_pw)
check("here" in cmd(sb, "look").lower(), "bystander is in the mortal start room")

# Fresh mortal for the clean-rent path.
mort2_name = f"Rentmortwo{_suffix}"
mort2_pw = "rentmortwopw123"
s2 = socket.create_connection((host, port), timeout=5)
make_char(s2, mort2_name, mort2_pw)
s2.close()
sql(f"UPDATE player SET load_room=100 WHERE name='{mort2_name}';")
s2 = login(mort2_name, mort2_pw)

recv_all(sb, timeout=0.3)
out = cmd(s2, "rent")
check("store your belongings safely away" in out, "rent confirms delivery to the caller")
witness_out = recv_all(sb, timeout=0.5)
check(f"{mort2_name} rents a room and disappears." in witness_out, "the room is told the character rented out")

out = cmd(s2, "look")
check("Connect Player" in out, "rent leaves the caller at the account menu, connection still alive")

check(rented_at(mort2_name) > 0, "rented_at is stamped in the DB after renting")

# --- 3: reconnecting after enough time heals HP, clears the marker ---
sql(f"UPDATE player_progress SET hp=1, rented_at=rented_at-3600 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{mort2_name}');")
s2.close()

max_hp = db_max_hp(mort2_name)
s2 = login(mort2_name, mort2_pw)
check(db_hp(mort2_name) == max_hp, "an hour rented out heals all the way back to max HP")
check(rented_at(mort2_name) == 0, "rented_at is cleared after the healing is applied")

s_imm.close()
sb.close()
s2.close()
announce_done("smoke_test_rent")
print("=== ALL CHECKS PASSED ===")
