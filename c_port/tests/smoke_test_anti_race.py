#!/usr/bin/env python3
"""Smoke test for object anti-race flags (TODO.md priority item, user
2026-08-02: ANTI_HUMAN/ANTI_DWARF/ANTI_OGRE/ANTI_ELF/ANTI_GNOME/
ANTI_HOBBIT). Tobin-only design, no SneezyMUD equivalent to port
(upstream only restricts wear by class). Covers:

  1. A human-barred item refuses `wear` for a human character.
  2. The SAME item is wearable by a dwarf (no anti_race_flag bit for
     dwarf set).
  3. A dwarf-barred item refuses `wield` for a dwarf character (the
     hold/wield path, not just plain `wear`).
  4. `oedit`'s new "Anti-race flags" (menu 18) toggle submenu actually
     sets/persists the bit.

    python3 tests/smoke_test_anti_race.py [host] [port]
"""
import socket
import subprocess
import sys
import time

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 930000 + (int(time.time()) % 60000)
HUMAN_BARRED_ITEM = ROOM + 1
DWARF_BARRED_WEAPON = ROOM + 2
OEDIT_ITEM = ROOM + 3

WEAR_TAKE = 1
WEAR_HEAD = 16
WEAR_HOLD = 16384
TYPE_ARMOR = 9
TYPE_WEAPON = 5
ANTI_RACE_HUMAN = 1 << 0
ANTI_RACE_DWARF = 1 << 3


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


announce("smoke_test_anti_race")


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


def set_race(name, race):
    sql(f"UPDATE player SET race={race} WHERE name='{name}';")


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


imm_name = f"Racimm{_suffix}"
imm_pw = "racimmpw123"
human_name = f"Rachum{_suffix}"
human_pw = "rachumpw123"
dwarf_name = f"Racdwf{_suffix}"
dwarf_pw = "racdwfpw123"

s = socket.create_connection((host, port), timeout=5)
make_char(s, imm_name, imm_pw)
set_level(imm_name, 51)
s.close()
s = socket.create_connection((host, port), timeout=5)
recv_all(s)
send_line(s, imm_name); recv_all(s)
send_line(s, imm_pw); recv_all(s)
send_line(s, "1"); recv_all(s)
cmd(s, "color off")

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Anti-Race Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
check("Anti-Race Sandbox" in cmd(s, f"goto {ROOM}"), "goto lands in the SQL-bootstrapped sandbox room")

# --- human mortal, race=0 (RACE_HUMAN) by default from character creation ---
sh = socket.create_connection((host, port), timeout=5)
make_char(sh, human_name, human_pw)
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{human_name}';")
cmd(sh, "quit!")
sh.close()
sh = socket.create_connection((host, port), timeout=5)
recv_all(sh)
send_line(sh, human_name); recv_all(sh)
send_line(sh, human_pw); recv_all(sh)
send_line(sh, "1"); recv_all(sh)
cmd(sh, "color off")
check("Anti-Race Sandbox" in cmd(sh, "look"), "the human mortal lands directly in the sandbox room")

# --- dwarf mortal, forced to race=3 (RACE_DWARF) after creation ---
sd = socket.create_connection((host, port), timeout=5)
make_char(sd, dwarf_name, dwarf_pw)
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{dwarf_name}';")
set_race(dwarf_name, 3)
cmd(sd, "quit!")
sd.close()
sd = socket.create_connection((host, port), timeout=5)
recv_all(sd)
send_line(sd, dwarf_name); recv_all(sd)
send_line(sd, dwarf_pw); recv_all(sd)
send_line(sd, "1"); recv_all(sd)
cmd(sd, "color off")
check("Anti-Race Sandbox" in cmd(sd, "look"), "the dwarf mortal lands directly in the sandbox room")

sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,anti_race_flag,weight,val0,can_be_seen) "
    f"VALUES ({HUMAN_BARRED_ITEM},'elven circlet','an elven circlet','An elven circlet is lying here.\\n',"
    f"{TYPE_ARMOR},{WEAR_TAKE | WEAR_HEAD},{ANTI_RACE_HUMAN},1,0,1),"
    f"({DWARF_BARRED_WEAPON},'elven blade','an elven blade','An elven blade is lying here.\\n',"
    f"{TYPE_WEAPON},{WEAR_TAKE | WEAR_HOLD},{ANTI_RACE_DWARF},4,0,1);")

# --- 1/2: human-barred item refuses the human, but a dwarf can wear it ---
check("You conjure" in cmd(s, f"load obj {HUMAN_BARRED_ITEM}"), "the elven circlet is loaded")
check("you give" in cmd(s, f"give circlet {human_name}").lower(), "the immortal hands the circlet to the human")
out = cmd(sh, "wear circlet")
check("your race cannot wear" in out.lower(), "the human is refused -- ANTI_HUMAN blocks the wear")
check("you conjure" not in out.lower(), "(sanity) not a load-command echo")

check("you give" in cmd(sh, f"give circlet {dwarf_name}").lower(), "the human hands the circlet off to the dwarf")
out = cmd(sd, "wear circlet")
check("you wear" in out.lower(), "the SAME item is wearable by a dwarf (no ANTI_DWARF bit set on it)")
cmd(sd, "remove circlet")

# --- 3: dwarf-barred item refuses `wield` for the dwarf ---
check("You conjure" in cmd(s, f"load obj {DWARF_BARRED_WEAPON}"), "the elven blade is loaded")
check("you give" in cmd(s, f"give blade {dwarf_name}").lower(), "the immortal hands the blade to the dwarf")
out = cmd(sd, "wield blade")
check("your race cannot wield" in out.lower(), "the dwarf is refused -- ANTI_DWARF blocks the wield")

# --- 4: oedit's new Anti-race flags submenu actually sets the bit ---
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,weight,val0,can_be_seen) "
    f"VALUES ({OEDIT_ITEM},'test trinket','a test trinket','A test trinket is lying here.\\n',"
    f"{TYPE_ARMOR},{WEAR_TAKE},1,0,1);")
out = cmd(s, f"edit object {OEDIT_ITEM}")
check("anti-race flags" in out.lower(), "oedit's top menu shows the new Anti-race flags line (18)")
out = cmd(s, "18")
check("[ ] " in out or "[x]" in out.lower(), "menu 18 opens the anti-race toggle submenu")
check("gnome" in out.lower(), "the submenu lists GNOME as one of the six toggleable races")
cmd(s, "5")  # bit 5 = GNOME (ANTI_RACE_GNOME, matches player_race_t order)
cmd(s, "")   # blank returns to the main menu
cmd(s, "S")  # save
row = subprocess.run(["mariadb", "tobin", "-N", "-e",
                      f"select anti_race_flag from obj where vnum={OEDIT_ITEM};"],
                     capture_output=True, text=True, check=True).stdout.strip()
check(row == "32", f"oedit's toggle persisted anti_race_flag=32 (ANTI_RACE_GNOME) to the DB, got {row!r}")
cmd(s, "Q")

s.close()
sh.close()
sd.close()
announce_done("smoke_test_anti_race")
print("=== ALL CHECKS PASSED ===")
