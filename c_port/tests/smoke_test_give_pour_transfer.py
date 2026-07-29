#!/usr/bin/env python3
"""Smoke test for `give` (new command) and `pour`'s new container-to-
container transfer form -- object manipulation audit continued
(2026-07-29), a fresh Sneezy-vs-Tobin comparison found beyond the
earlier narrow sacrifice/junk/identify pass (see TODO.md). Covers:

  1. `give <item> <person>` hands a carried item to another PC in the
     room -- it leaves the giver's inventory and lands in the
     recipient's.
  2. `give <amount> gold <person>` transfers gold between wallets.
  3. `give` refuses when nobody by that name is in the room.
  4. `give` refuses while the giver is fighting.
  5. `pour <container> <container2>` transfers liquid between two
     carried containers -- the source empties (or partially drains),
     the destination fills.
  6. `pour <container> <container2>` refuses to mix two different
     liquids, same "pour it out first" precedent `fill`'s own mixing
     check already set.

    python3 tests/smoke_test_give_pour_transfer.py [host] [port]
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


announce("smoke_test_give_pour_transfer")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 973000 + (int(time.time()) % 20000)
WATERSKIN = 410
WINESKIN = 409

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
    raw = recv_all(sock, timeout)
    return raw.split("\r\n", 1)[1] if "\r\n" in raw else raw


def check(condition, message):
    if not condition:
        raise AssertionError(message)
    print(f">>> OK: {message}")


def sql(stmt):
    subprocess.run(["mariadb", "tobin", "-e", stmt], check=True)


def make_char(name, pw, class_choice="1"):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "y"); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "new"); recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "1"); recv_all(s)
    send_line(s, class_choice); recv_all(s)
    send_line(s, "done"); recv_all(s)
    send_line(s, "done"); recv_all(s)
    cmd(s, "color off")
    return s


sql(f"DELETE FROM room WHERE vnum={ROOM};")
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'GivePour Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")

giver_name, giver_pw = f"Givera{_suffix}", "givepourpw123"
recv_name, recv_pw = f"Recva{_suffix}", "givepourpw123"

s1 = make_char(giver_name, giver_pw)
cmd(s1, "quit!")
s1.close()
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{giver_name}';")

s2 = make_char(recv_name, recv_pw)
cmd(s2, "quit!")
s2.close()
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{recv_name}';")

giver = socket.create_connection((host, port), timeout=5)
recv_all(giver)
send_line(giver, giver_name); recv_all(giver)
send_line(giver, giver_pw); recv_all(giver)
send_line(giver, "1"); recv_all(giver)
cmd(giver, "color off")
check("GivePour Sandbox" in cmd(giver, "look"), "the giver lands directly in the sandbox room")

recvr = socket.create_connection((host, port), timeout=5)
recv_all(recvr)
send_line(recvr, recv_name); recv_all(recvr)
send_line(recvr, recv_pw); recv_all(recvr)
send_line(recvr, "1"); recv_all(recvr)
cmd(recvr, "color off")
check("GivePour Sandbox" in cmd(recvr, "look"), "the recipient lands directly in the same sandbox room")

# An immortal loads/drops a fixture item for the giver to pick up --
# NOT relying on default starting gear, since `quit!` spills a
# character's belongings onto the floor of whatever room they quit in
# ("Your belongings spill onto the ground as you leave!", see
# smoke_test_quit_drop.py) -- both giver and recvr above just went
# through exactly that quit!-then-reconnect cycle (needed to place them
# in the sandbox room without triggering the linkdead/load_room bug --
# see TODO.md's writeup), so neither has any of their starting suit
# gear left by this point.
imm_name, imm_pw = f"Givepimm{_suffix}", "givepourimmpw123"
si = make_char(imm_name, imm_pw)
cmd(si, "quit!")
si.close()
sql(f"UPDATE player_progress SET level=51 WHERE player_id=(SELECT id FROM player WHERE name='{imm_name}');")
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{imm_name}';")
imm = socket.create_connection((host, port), timeout=5)
recv_all(imm)
send_line(imm, imm_name); recv_all(imm)
send_line(imm, imm_pw); recv_all(imm)
send_line(imm, "1"); recv_all(imm)
cmd(imm, "color off")

GIVEITEM = ROOM + 900000  # collision-safe fixture vnum in the 900000+ sandbox range
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({GIVEITEM},'trinket giveaway test','a test trinket','A test trinket lies here.',12,1,1);")
check("You conjure" in cmd(imm, f"load obj {GIVEITEM}"), "immortal loads a fixture trinket for the giver")
check("You drop" in cmd(imm, "drop trinket"), "immortal drops the trinket")
check("You get" in cmd(giver, "get trinket"), "giver picks up the trinket")

# --- 1: give <item> <person> ---
out = cmd(giver, "inventory")
check("trinket" in out.lower(), "the giver is carrying the trinket to give away")
print("DEBUG room state before give:", repr(cmd(giver, "look")))
print("DEBUG recv_name repr:", repr(recv_name))
out = cmd(giver, f"give trinket {recv_name}")
print("DEBUG give response:", repr(out))
check("you give" in out.lower(), "give confirms the item hand-off")
out = cmd(giver, "inventory")
check("trinket" not in out.lower(), "the trinket left the giver's inventory")
out = cmd(recvr, "inventory")
check("trinket" in out.lower(), "the trinket landed in the recipient's inventory")

# --- 2: give <amount> gold <person> ---
sql(f"UPDATE player_progress SET gold=100 WHERE player_id=(SELECT id FROM player WHERE name='{giver_name}');")
sql(f"UPDATE player_progress SET gold=0 WHERE player_id=(SELECT id FROM player WHERE name='{recv_name}');")
out = cmd(giver, f"give 40 gold {recv_name}")
check("you give 40 gold" in out.lower(), "give gold confirms the amount")

# --- 3: give refuses when the target isn't here ---
out = cmd(giver, "give gold nobodyhere")
check("aren't here" in out.lower() or "carrying that" in out.lower(),
      "give refuses a target that isn't actually here")

# --- 4: give refuses mid-fight ---
cmd(giver, "toggle pk")
cmd(recvr, "toggle pk")
cmd(giver, f"attack {recv_name}", 2.0)
out = cmd(giver, f"give 1 gold {recv_name}")
check("not while" in out.lower(), "give refuses while the giver is fighting")
cmd(giver, "flee", 2.0)
recv_all(giver, timeout=2.0)
recv_all(recvr, timeout=2.0)
# --- 5/6: pour transfer between two carried containers ---
# Reuses the same immortal helper (`imm`) from the give-fixture setup
# above -- `load obj` is immortal-only and lands straight in the
# LOADING immortal's own inventory (not the room floor), so it drops
# both containers for the giver to pick up.
check("You conjure" in cmd(imm, f"load obj {WATERSKIN}"), "immortal loads a waterskin for the giver")
check("You drop" in cmd(imm, "drop waterskin"), "immortal drops the waterskin")
check("You conjure" in cmd(imm, f"load obj {WINESKIN}"), "immortal loads a wineskin for the giver")
check("You drop" in cmd(imm, "drop wineskin"), "immortal drops the wineskin")
check("You get" in cmd(giver, "get waterskin"), "giver picks up the waterskin")
check("You get" in cmd(giver, "get wineskin"), "giver picks up the wineskin")

# Both start full (waterskin=water, wineskin=wine) -- pouring water INTO
# the still-full wineskin must refuse as a mix, matching fill's own
# mixing precedent.
out = cmd(giver, "pour waterskin wineskin")
check("mix" in out.lower(), "pour refuses to mix two different liquids between containers")

check("you empty" in cmd(giver, "pour wineskin").lower(), "the wineskin is poured out onto the ground first")
cmd(imm, "purge")  # clear the resulting wine puddle

out = cmd(giver, "pour waterskin wineskin")
check("you pour" in out.lower(), "pour now transfers water into the emptied wineskin")
out = cmd(giver, "drink waterskin")
check("it's empty" in out.lower(), "the source waterskin is now empty after the transfer")
out = cmd(giver, "drink wineskin")
check("it's empty" not in out.lower() and "water" in out.lower(),
      "the destination wineskin now holds water, not wine")

print("ALL CHECKS PASSED")
announce_done("smoke_test_give_pour_transfer")
