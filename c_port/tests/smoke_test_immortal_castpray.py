#!/usr/bin/env python3
"""Smoke test for immortal class-restriction bypass on cast/pray/skills
(user 2026-07-12: "immortals can use any skill or spell in game, no
class restrictions"). Covers:

  1. An immortal (non-Mage/Druid class) can `cast` a Mage spell that a
     mortal of their own class would be refused ("Huh?!").
  2. An immortal can `pray` a Cleric spell despite not being a Cleric.
  3. An immortal's `skills` output includes spells from OTHER classes
     (not just their own), proving the full-roster view is active.
  4. A same-class mortal is still gated normally (regression check --
     the bypass must not leak to non-immortals).

    python3 tests/smoke_test_immortal_castpray.py [host] [port]
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


announce("smoke_test_immortal_castpray")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 900000 + (int(time.time()) % 70000)
COMPONENT = ROOM + 1
SYMBOL = ROOM + 2


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


def make_char(name, pw, class_choice):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "y"); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "new"); recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "done"); recv_all(s)
    send_line(s, "1"); recv_all(s)  # race: human
    send_line(s, class_choice); recv_all(s)
    send_line(s, "2"); recv_all(s)  # alignment: neutral
    cmd(s, "color off")
    return s


# --- Warrior mortal: still gated normally (regression check) ---
warrior_name = f"Imcpwar{_suffix}"
pw = "immortcastpw123"
sw = make_char(warrior_name, pw, "3")
out = cmd(sw, "cast heal light")
check("Huh?!" in out, "a mortal Warrior is still refused cast (regression check)")
out = cmd(sw, "pray heal light")
check("Huh?!" in out, "a mortal Warrior is still refused pray (regression check)")
sw.close()

# --- Immortal Warrior: bypasses class gate on cast/pray/skills ---
imm_name = f"Imcpimm{_suffix}"
s_imm = socket.create_connection((host, port), timeout=5)
recv_all(s_imm)
send_line(s_imm, imm_name); recv_all(s_imm)
send_line(s_imm, "y"); recv_all(s_imm)
send_line(s_imm, pw); recv_all(s_imm)
send_line(s_imm, pw); recv_all(s_imm)
send_line(s_imm, "new"); recv_all(s_imm)
send_line(s_imm, imm_name); recv_all(s_imm)
send_line(s_imm, "done"); recv_all(s_imm)
send_line(s_imm, "1"); recv_all(s_imm)  # race: human
send_line(s_imm, "3"); recv_all(s_imm)  # class: warrior
send_line(s_imm, "2"); recv_all(s_imm)  # alignment: neutral
set_level(imm_name, 51)
s_imm.close()
s_imm = socket.create_connection((host, port), timeout=5)
recv_all(s_imm)
send_line(s_imm, imm_name); recv_all(s_imm)
send_line(s_imm, pw); recv_all(s_imm)
send_line(s_imm, "1"); recv_all(s_imm)
cmd(s_imm, "color off")

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Immortal Castpray Sandbox','A bare sandbox room.\\n',NULL,0,0,0,0,0,0,0,0,0,0);")
check("Immortal Castpray Sandbox" in cmd(s_imm, f"goto {ROOM}"), "goto lands in the sandbox room")

out = cmd(s_imm, "cast gust")
check("don't have the spell components" in out, "immortal Warrior reaches the Mage spell 'gust' (component-gated, not class-gated)")

sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({COMPONENT},'pouch component reagent','a pouch of spell components',"
    f"'A pouch of spell components is lying here.',12,1,1);")
check("You conjure" in cmd(s_imm, f"load obj {COMPONENT}"), "the component pouch is loaded")
out = cmd(s_imm, "get pouch")
check("you get" in out.lower(), "the immortal Warrior picks up the component pouch")
out = cmd(s_imm, "cast gust")
check("You cast gust" in out, "immortal Warrior casts the Mage spell 'gust' despite being a Warrior")

out = cmd(s_imm, "pray heal light")
check("need a holy symbol" in out, "immortal Warrior still needs a holy symbol item to pray (item gate, not a class gate)")

sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({SYMBOL},'symbol holy silver','a tarnished silver holy symbol',"
    f"'A tarnished silver holy symbol is lying here.',12,1,1);")
check("You conjure" in cmd(s_imm, f"load obj {SYMBOL}"), "the holy symbol is loaded")
out = cmd(s_imm, "get symbol")
check("you get" in out.lower(), "the immortal Warrior picks up the holy symbol")
out = cmd(s_imm, "pray heal light")
check("You pray for heal light" in out, "immortal Warrior prays the Cleric spell 'heal light' despite being a Warrior")

out = cmd(s_imm, "skills")
check("=== Mage ===" in out, "immortal 'skills' shows the Mage section")
check("=== Cleric ===" in out, "immortal 'skills' shows the Cleric section")
check("=== Warrior ===" in out, "immortal 'skills' shows their own Warrior section too")
check("gust" in out.lower(), "immortal 'skills' lists a Mage-only spell")

s_imm.close()
announce_done("smoke_test_immortal_castpray")
print("=== ALL CHECKS PASSED ===")
