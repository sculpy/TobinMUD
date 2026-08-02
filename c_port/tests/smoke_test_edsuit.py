#!/usr/bin/env python3
"""Smoke test for the menu-driven loadsuit editor (`edit suit`, TODO.md
priority item, user 2026-08-02: "Menu-driven loadsuit editor,
configurable per-wear-location quantities"). Covers:

  1. `edit suit <new name>` auto-creates a brand-new empty suit.
  2. Adding two DIFFERENT items with quantities, including a quantity
     greater than 1 (the actual per-wear-location-quantity feature --
     e.g. two wrist bands for two wrists).
  3. `loadsuit` on that suit actually grants the right item COUNTS,
     not just the right vnums (proves suit_grant() expands quantities,
     not just "in the suit or not").
  4. Changing an existing item's quantity from inside the editor.
  5. Deleting an item removes it from the suit.
  6. Setting/clearing the class restriction.

    python3 tests/smoke_test_edsuit.py [host] [port]
"""
import socket
import subprocess
import sys
import time

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 950000 + (int(time.time()) % 60000)
ITEM_A = ROOM + 1
ITEM_B = ROOM + 2

WEAR_TAKE = 1
TYPE_OTHER = 1


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


announce("smoke_test_edsuit")


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


def sql_scalar(stmt):
    return subprocess.run(["mariadb", "tobin", "-N", "-e", stmt],
                           capture_output=True, text=True, check=True).stdout.strip()


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
    send_line(sock, "1"); recv_all(sock)
    send_line(sock, "1"); recv_all(sock)
    send_line(sock, "done"); recv_all(sock)
    send_line(sock, "done"); recv_all(sock)


imm_name = f"Suitimm{_suffix}"
imm_pw = "suitimmpw123"

s = socket.create_connection((host, port), timeout=5)
make_char(s, imm_name, imm_pw)
set_level(imm_name, 56)
s.close()
s = socket.create_connection((host, port), timeout=5)
recv_all(s)
send_line(s, imm_name); recv_all(s)
send_line(s, imm_pw); recv_all(s)
send_line(s, "1"); recv_all(s)
cmd(s, "color off")

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Edsuit Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
check("Edsuit Sandbox" in cmd(s, f"goto {ROOM}"), "goto lands in the SQL-bootstrapped sandbox room")

sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,weight,val0,can_be_seen) "
    f"VALUES ({ITEM_A},'wrist band','a wrist band','A wrist band is lying here.\\n',"
    f"{TYPE_OTHER},{WEAR_TAKE},1,0,1),"
    f"({ITEM_B},'small torch','a small torch','A small torch is lying here.\\n',"
    f"{TYPE_OTHER},{WEAR_TAKE},1,0,1);")

suit_name = f"testsuit{_suffix}"

# --- 1: edit suit auto-creates a new suit ---
out = cmd(s, f"edit suit {suit_name}")
check("created a new empty one" in out.lower(), "a brand-new suit is auto-created")
check("no items yet" in out.lower(), "the fresh suit starts with no items")

# --- 2: add wrist band with quantity 2 (the per-wear-location feature) ---
out = cmd(s, "A")
check("enter the obj vnum" in out.lower(), "the Add flow prompts for a vnum")
out = cmd(s, str(ITEM_A))
check("enter quantity" in out.lower() and "wrist band" in out.lower(),
      "the Add flow shows the item name and asks for a quantity")
out = cmd(s, "2")
check("item added" in out.lower(), "the wrist band is added")
check("x2" in out.lower(), "the suit listing shows the wrist band at quantity 2")

# --- add small torch with default quantity (blank = 1) ---
cmd(s, "A")
cmd(s, str(ITEM_B))
out = cmd(s, "")
check("item added" in out.lower(), "the torch is added with the default quantity")
check("x1" in out.lower(), "the torch shows at quantity 1")

# --- 3: loadsuit actually grants the right counts ---
mort_name = f"Suitmor{_suffix}"
mort_pw = "suitmorpw123"
sv = socket.create_connection((host, port), timeout=5)
make_char(sv, mort_name, mort_pw)
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{mort_name}';")
cmd(sv, "quit!")
sv.close()
sv = socket.create_connection((host, port), timeout=5)
recv_all(sv)
send_line(sv, mort_name); recv_all(sv)
send_line(sv, mort_pw); recv_all(sv)
send_line(sv, "1"); recv_all(sv)
cmd(sv, "color off")

out = cmd(s, f"loadsuit {suit_name} {mort_name}")
check("3 items" in out.lower(), "loadsuit reports granting 3 items total (2 wrist bands + 1 torch)")
out = cmd(sv, "inventory")
check("wrist band (x2)" in out.lower(), "the mortal's inventory actually shows 2 wrist bands stacked")
check("small torch" in out.lower(), "the mortal's inventory shows the torch too")

# --- 4: change the wrist band's quantity from inside the editor ---
cmd(s, f"edit suit {suit_name}")
out = cmd(s, "1")
check("quantity" in out.lower(), "selecting item 1 opens its detail view")
cmd(s, "1")
out = cmd(s, "5")
check("x5" in cmd(s, "").lower() or True, "(quantity change applied)")  # verified via SQL below

suit_id = sql_scalar(f"SELECT id FROM suit WHERE name='{suit_name}';")
qty = sql_scalar(f"SELECT quantity FROM suit_item WHERE suit_id={suit_id} AND obj_vnum={ITEM_A};")
check(qty == "5", f"the wrist band's quantity was updated to 5 in the DB, got {qty!r}")

# --- 5: delete the torch ---
cmd(s, f"edit suit {suit_name}")
out = cmd(s, "2")
check("delete this item" in out.lower(), "item 2's detail view offers Delete")
cmd(s, "D")
out = cmd(s, "yes")
check("item removed" in out.lower(), "confirming yes removes the item")
remaining = sql_scalar(f"SELECT COUNT(*) FROM suit_item WHERE suit_id={suit_id} AND obj_vnum={ITEM_B};")
check(remaining == "0", "the torch row is actually gone from suit_item")

# --- 6: set and clear the class restriction ---
cmd(s, f"edit suit {suit_name}")
out = cmd(s, "C")
cmd(s, "2")  # Warrior
cls = sql_scalar(f"SELECT class FROM suit WHERE id={suit_id};")
check(cls == "2", f"the class restriction was set to 2 (Warrior), got {cls!r}")
cmd(s, f"edit suit {suit_name}")
cmd(s, "C")
cmd(s, "any")
cls = sql_scalar(f"SELECT class FROM suit WHERE id={suit_id};")
check(cls is None or cls == "" or cls == "NULL", f"\"any\" clears the class restriction, got {cls!r}")

cmd(s, "")  # leave the editor

s.close()
sv.close()
announce_done("smoke_test_edsuit")
print("=== ALL CHECKS PASSED ===")
