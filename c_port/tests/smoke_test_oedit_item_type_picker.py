#!/usr/bin/env python3
"""Smoke test for oedit's item-type picker (Object editor: item-type flag
listing/picker, TODO.md priority item, user 2026-08-02). Previously menu
option 3 ("Item type") just took a raw number blind, with an error
message telling builders to go check `stat`/`vnum` on some other object
first if they got it wrong. Now it lists every known type by number and
name up front.

    python3 tests/smoke_test_oedit_item_type_picker.py [host] [port]
"""
import socket
import subprocess
import sys
import time

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
OEDIT_ITEM = 940000 + (int(time.time()) % 60000)


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


announce("smoke_test_oedit_item_type_picker")


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
    send_line(sock, "1"); recv_all(sock)
    send_line(sock, "1"); recv_all(sock)
    send_line(sock, "done"); recv_all(sock)
    send_line(sock, "done"); recv_all(sock)


imm_name = f"Oeimm{_suffix}"
imm_pw = "oeimmpw123"

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

sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,weight,val0,can_be_seen) "
    f"VALUES ({OEDIT_ITEM},'test widget','a test widget','A test widget is lying here.\\n',"
    f"1,1,1,0,1);")

out = cmd(s, f"edit object {OEDIT_ITEM}")
check("item type" in out.lower(), "the oedit main menu shows the Item type line")

out = cmd(s, "3")
check("item types for" in out.lower(), "menu 3 opens the item type picker listing")
check("weapon" in out.lower() and "armor" in out.lower() and "food" in out.lower(),
      "the listing shows real type names (WEAPON/ARMOR/FOOD), not just numbers")
check("light" in out.lower(), "the listing includes LIGHT (type #1)")

# --- pick type 9 (ARMOR) from the list ---
out = cmd(s, "9")
check("item type" in out.lower(), "back at the main menu after picking a type")
check("armor" in out.lower(), "the main menu now shows ARMOR as the current item type")

# --- an out-of-range number redisplays the list instead of a bare error ---
out = cmd(s, "3")
out = cmd(s, "9999")
check("pick a type number from the list" in out.lower(), "an invalid number is rejected with guidance")
check("item types for" in out.lower(), "the list is redisplayed after an invalid entry")

cmd(s, "")   # cancel out of the type picker
cmd(s, "Q")  # quit oedit -- dirty (we picked ARMOR above), so this prompts
cmd(s, "D")  # discard the unsaved change

row = subprocess.run(["mariadb", "tobin", "-N", "-e",
                      f"select type from obj where vnum={OEDIT_ITEM};"],
                     capture_output=True, text=True, check=True).stdout.strip()
check(row == "1", f"quitting without Save left the DB row untouched (type={row!r}, still 1/LIGHT)")

s.close()
announce_done("smoke_test_oedit_item_type_picker")
print("=== ALL CHECKS PASSED ===")
