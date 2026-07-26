#!/usr/bin/env python3
"""Smoke test for the menu-driven object-prototype editor (`edit object
<vnum>`/oedit, TODO.md's "NEXT UP" item -- Sneezy -> Tobin feature audit's
builder-tools-OLC gap). Covers:
  1. A mortal (below BUILD_MIN_LEVEL) can't reach `edit` at all -- same
     invisible-command behavior as every other level-gated verb.
  2. edobject opens the menu, showing the real prototype row's fields.
  3. Editing name/weight/four-values/a take-flag bit marks the working
     copy dirty (shown in the menu) but doesn't touch the DB until Save.
  4. Save persists all of it in one write.
  5. A nonexistent vnum auto-creates a blank object and opens straight
     into the editor, rather than being refused (2026-07-25, user: "if
     one doesn't exist a blank one should be created", then "objects and
     rooms should behave the same" -- edmobile got the identical
     treatment).
  6. Quit with unsaved changes prompts Save/Discard/Cancel; Discard
     actually discards (DB unchanged).

    python3 tests/smoke_test_edobject.py [host] [port]
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


announce("smoke_test_edobject")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
VNUM = 960000 + (int(time.time()) % 30000)


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
    send_line(sock, "2"); recv_all(sock)  # alignment: neutral


def login(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


imm_name = f"Oedi{_suffix}"
imm_pw = "oediimmpw123"
mort_name = f"Oedm{_suffix}"
mort_pw = "oedimortpw123"

sql(f"DELETE FROM obj WHERE vnum={VNUM};")
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,"
    f"val0,val1,val2,val3,weight,price,can_be_seen,max_exist,max_struct,"
    f"cur_struct,decay,volume,material,spec_proc,action_flag) VALUES "
    f"({VNUM},'oedit sandbox test','an oedit sandbox test item',"
    f"'An oedit sandbox test item is here.',12,9,0,0,0,0,5,10,1,0,10,10,-1,5,0,0,0);")

s = socket.create_connection((host, port), timeout=5)
make_char(s, imm_name, imm_pw)
set_level(imm_name, 59)
s.close()
s = login(imm_name, imm_pw)

s2 = socket.create_connection((host, port), timeout=5)
make_char(s2, mort_name, mort_pw)
s2.close()
s2 = login(mort_name, mort_pw)

# --- 1: a mortal can't reach `edit` at all (invisible, not refused) ---
out = cmd(s2, f"edit object {VNUM}")
check("command not found" in out.lower(), "a mortal typing edit gets the generic unknown-command message")

# --- 5: a nonexistent vnum auto-creates a blank object ---
VNUM_NEW = VNUM + 1
sql(f"DELETE FROM obj WHERE vnum={VNUM_NEW};")
out = cmd(s, f"edit object {VNUM_NEW}")
check("no object existed at that vnum" in out.lower() and "created a blank one" in out.lower(),
      "a nonexistent vnum gets a blank object created instead of being refused")
check("an unfinished object" in out, "the auto-created blank object opens straight into the editor")
check(query(f"SELECT name FROM obj WHERE vnum={VNUM_NEW};") == "an unfinished object",
      "the blank object was actually persisted to the DB immediately, not just held in a working copy")
cmd(s, "Q")
sql(f"DELETE FROM obj WHERE vnum={VNUM_NEW};")

# --- 2: opens the menu, showing the real prototype fields ---
out = cmd(s, f"edit object {VNUM}")
check(f"Editing object:" in out and "oedit sandbox test" in out, "edobject opens the menu")
check("Take flags: [ TAKE ] [ BODY ]" in out, "the seeded wear_flag (9 = TAKE|BODY) decodes correctly")
check("Weight: 5.0" in out, "the seeded weight shows in the menu")

# --- 3: edits mark it dirty, no DB write yet ---
out = cmd(s, "1")
check("Enter new name" in out, "menu item 1 prompts for a new name")
out = cmd(s, "an oedit TEST item renamed")
check("unsaved changes" in out.lower(), "the rename marks the working copy dirty")
check(query(f"SELECT name FROM obj WHERE vnum={VNUM};") == "oedit sandbox test",
      "the DB is untouched before Save")

cmd(s, "5")
out = cmd(s, "12.5")
check("Weight: 12.5" in out, "weight updated in the working copy")

cmd(s, "10")
out = cmd(s, "7 8 9 10")
check("Four values: 7 8 9 10" in out, "the four values updated together in the working copy")

out = cmd(s, "8")
check("Take flags for" in out, "menu item 8 opens the take-flags toggle submenu")
out = cmd(s, "3")
check("[ ] BODY" in out, "toggling bit 3 (BODY) off shows unchecked in the submenu redisplay")
out = cmd(s, "")
check("Take flags: [ TAKE ]" in out, "blank returns to the main menu; BODY is gone from the summary")
check(query(f"SELECT wear_flag FROM obj WHERE vnum={VNUM};") == "9",
      "the flag toggle is still just a working-copy edit -- DB untouched before Save")

# --- 4: Save persists everything in one write ---
out = cmd(s, "S")
check("Object saved" in out, "Save reports success")
check(query(f"SELECT name, weight, val0, val1, val2, val3, wear_flag FROM obj WHERE vnum={VNUM};")
      == "an oedit TEST item renamed\t12.5\t7\t8\t9\t10\t1",
      "Save actually persisted name/weight/values/wear_flag together")

# --- 6: Quit with unsaved changes -- Discard actually discards ---
cmd(s, "1")
cmd(s, "Should Not Persist")
out = cmd(s, "Q")
check("unsaved changes" in out.lower(), "Quit with unsaved changes prompts to save/discard/cancel")
out = cmd(s, "D")
check("Leaving the object editor" in out, "Discard leaves the editor")
check(query(f"SELECT name FROM obj WHERE vnum={VNUM};") == "an oedit TEST item renamed",
      "Discard did NOT persist the last unsaved rename")

s.close()
s2.close()

sql(f"DELETE FROM obj WHERE vnum={VNUM};")
for nm in (imm_name, mort_name):
    sql(f"DELETE FROM player_inventory WHERE player_id=(SELECT id FROM player WHERE name='{nm}');")
    sql(f"DELETE FROM player_attrs WHERE player_id=(SELECT id FROM player WHERE name='{nm}');")
    sql(f"DELETE FROM player_progress WHERE player_id=(SELECT id FROM player WHERE name='{nm}');")
    sql(f"DELETE FROM player WHERE name='{nm}';")

announce_done("smoke_test_edobject")
print("=== ALL CHECKS PASSED ===")
