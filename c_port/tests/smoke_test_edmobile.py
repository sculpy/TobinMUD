#!/usr/bin/env python3
"""Smoke test for the menu-driven mob-prototype editor (`edit mob
<vnum>`/medit, TODO.md's last builder-tools-OLC gap -- edroom/edzone/
edobject already existed). Covers the final wireframe-corrected 23-field
menu (2026-07-25, user-supplied wireframe -- Faction/manually-editable-
Characteristics/spec proc/local+adjacent sound dropped entirely):
  1. A mortal (below BUILD_MIN_LEVEL) can't reach `edit` at all -- same
     invisible-command behavior as every other level-gated verb.
  2. edmobile opens the menu, showing the real prototype row's fields.
  3. Editing name/level marks the working copy dirty (shown in the menu)
     but doesn't touch the DB until Save.
  4. Save persists all of it in one write, AND auto-computes
     str/con/wis/intel/dex/cha from level+class (user: "according to
     race and class" -- race has no mapping yet, so only class
     contributes beyond the level base), matching being_create_mob()'s
     own live-spawn formula exactly.
  5. A nonexistent vnum auto-creates a blank mob and opens straight into
     the editor, rather than being refused (same behavior edroom/edobject
     use).
  6. Quit with unsaved changes prompts Save/Discard/Cancel; Discard
     actually discards (DB unchanged).

    python3 tests/smoke_test_edmobile.py [host] [port]
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


announce("smoke_test_edmobile")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
VNUM = 930000 + (int(time.time()) % 30000)


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


def query(stmt):
    out = subprocess.run(["mariadb", "tobin", "-N", "-e", stmt],
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
    send_line(sock, "done"); recv_all(sock)  # alignment: neutral


def login(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


imm_name = f"Medi{_suffix}"
imm_pw = "mediimmpw123"
mort_name = f"Medm{_suffix}"
mort_pw = "medimortpw123"

sql(f"DELETE FROM mob WHERE vnum={VNUM};")
# class=4 (Warrior, per mob_class_mask_to_tobin) so the Save-time
# auto-calculated characteristics (str/con/wis/intel/dex/cha) are
# predictable: class_stat_bonus() gives Warrior a fixed
# constitution+3, strength+3, charisma-3, wisdom-3 on top of the
# level-derived base (ATTR_BASE=120 + level, capped at ATTR_MAX=250).
sql(f"INSERT INTO mob (vnum,name,short_desc,long_desc,description,actions,affects,"
    f"faction,fact_perc,letter,attacks,class,level,tohit,ac,hpbonus,damage_level,"
    f"damage_precision,gold,race,weight,height,str,bra,con,dex,agi,intel,wis,foc,"
    f"per,cha,kar,spe,pos,def_position,sex,spec_proc,skin,vision,can_be_seen,"
    f"local_sound,adjacent_sound,max_exist,align) VALUES "
    f"({VNUM},'medit sandbox test','a medit sandbox test mob',"
    f"'A medit sandbox test mob stands here.','desc',0,0,1,25,'A',1.0,4,5,0,0.0,0.0,"
    f"1.0,0,10,0,150,70,120,120,120,120,120,120,120,120,120,120,120,120,10,10,1,0,0,0,1,"
    f"NULL,NULL,1,0);")

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
out = cmd(s2, f"edit mob {VNUM}")
check("command not found" in out.lower(), "a mortal typing edit gets the generic unknown-command message")

# --- 5: a nonexistent vnum auto-creates a blank mob (2026-07-25, user:
# "if one doesn't exist a blank one should be created", then "objects and
# rooms should behave the same" -- edroom/edobject got the identical
# treatment) rather than being refused. ---
VNUM_NEW = VNUM + 1
sql(f"DELETE FROM mob WHERE vnum={VNUM_NEW};")
out = cmd(s, f"edit mob {VNUM_NEW}")
check("no mob existed at that vnum" in out.lower() and "created a blank one" in out.lower(),
      "a nonexistent vnum gets a blank mob created instead of being refused")
check("an unfinished mob" in out, "the auto-created blank mob opens straight into the editor")
check(query(f"SELECT name FROM mob WHERE vnum={VNUM_NEW};") == "an unfinished mob",
      "the blank mob was actually persisted to the DB immediately, not just held in a working copy")
cmd(s, "Q")
sql(f"DELETE FROM mob WHERE vnum={VNUM_NEW};")

# --- 2: opens the menu, showing the real prototype fields ---
out = cmd(s, f"edit mob {VNUM}")
check("Editing mob:" in out and "medit sandbox test" in out, "edmobile opens the menu")
check("Level: 5" in out, "the seeded level shows in the menu")
check("Faction" not in out, "Faction was dropped from the final wireframe -- not shown")
check("Characteristics" not in out,
      "Characteristics is no longer a directly-editable menu field -- auto-computed on Save")

# --- 3: edits mark it dirty, no DB write yet ---
out = cmd(s, "1")
check("Enter new name" in out, "menu item 1 prompts for a new name")
out = cmd(s, "a medit TEST mob renamed")
check("unsaved changes" in out.lower(), "the rename marks the working copy dirty")
check(query(f"SELECT name FROM mob WHERE vnum={VNUM};") == "medit sandbox test",
      "the DB is untouched before Save")

cmd(s, "8")
out = cmd(s, "25")
check("Level: 25" in out, "level (menu item 8 in the final numbering) updated in the working copy")

check(query(f"SELECT level, str, con FROM mob WHERE vnum={VNUM};") == "5\t120\t120",
      "none of the working-copy edits above touched the DB yet")

# --- 4: Save persists everything, AND auto-computes characteristics from
# level+class (user: "according to race and class" -- race isn't mapped
# yet, so only class contributes beyond the level base). Seeded class=4
# (Warrior): base = min(ATTR_BASE + level, ATTR_MAX) = min(120+25,250)
# = 145 for all six, then class_stat_bonus() applies Warrior's fixed
# constitution+3, strength+3, charisma-3, wisdom-3 (being.c) -- dex and
# intel are untouched by Warrior's bonus, so they stay at the 145 base. ---
out = cmd(s, "S")
check("Mob saved" in out, "Save reports success")
check(query(f"SELECT name, level, str, con, wis, intel, dex, cha, bra FROM mob WHERE vnum={VNUM};")
      == "a medit TEST mob renamed\t25\t148\t148\t142\t145\t145\t142\t120",
      "Save persisted the rename/level, auto-computed str/con/wis/intel/dex/cha from "
      "level+Warrior class, and left the upstream-only `bra` column at its original "
      "seeded value (120), never touched by medit at all")

# --- 6: Quit with unsaved changes -- Discard actually discards ---
cmd(s, "1")
cmd(s, "Should Not Persist")
out = cmd(s, "Q")
check("unsaved changes" in out.lower(), "Quit with unsaved changes prompts to save/discard/cancel")
out = cmd(s, "D")
check("Leaving the mob editor" in out, "Discard leaves the editor")
check(query(f"SELECT name FROM mob WHERE vnum={VNUM};") == "a medit TEST mob renamed",
      "Discard did NOT persist the last unsaved rename")

s.close()
s2.close()

sql(f"DELETE FROM mob WHERE vnum={VNUM};")
for nm in (imm_name, mort_name):
    sql(f"DELETE FROM player_inventory WHERE player_id=(SELECT id FROM player WHERE name='{nm}');")
    sql(f"DELETE FROM player_attrs WHERE player_id=(SELECT id FROM player WHERE name='{nm}');")
    sql(f"DELETE FROM player_progress WHERE player_id=(SELECT id FROM player WHERE name='{nm}');")
    sql(f"DELETE FROM player WHERE name='{nm}';")

announce_done("smoke_test_edmobile")
print("=== ALL CHECKS PASSED ===")
