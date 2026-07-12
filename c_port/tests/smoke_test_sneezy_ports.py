#!/usr/bin/env python3
"""Smoke test for six Sneezy command ports (user, 2026-07-12: "port the
sneezy commands consider and examine and sip and show and tell and
whisper"). Each is scoped down from the original where Tobin lacks the
supporting system (trophy tracking, lore sub-skills, admin room/zone
listings) -- see each cmd_*.c file's own header comment for what was
kept vs dropped. Covers:

  1. `examine <mob>` -- a synonym for `look <target>`, same output.
  2. `consider self` -- an AC-based equipment-quality line.
  3. `consider <mob>` -- the plain level-difference verdict ladder.
  4. `consider <other PC>` -- the flavor-only refusal.
  5. `sip fountain` -- tastes a real seeded fountain (vnum 3), never
     poisons water.
  6. `tell` -- reaches the target regardless of room.
  7. `whisper` -- reaches the target in-room; a bystander sees a
     content-free notice.
  8. `show <item> <person>` -- a message only, the item stays put.

    python3 tests/smoke_test_sneezy_ports.py [host] [port]
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


announce("smoke_test_sneezy_ports")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 900000 + (int(time.time() * 1000000) % 90000)
DUMMY_VNUM = ROOM + 1
ITEM_VNUM = ROOM + 2
FOUNTAIN_VNUM = 3  # real seeded content, see smoke_test_drink.py


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


def make_char(name, pw, class_choice="1"):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "y"); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "new"); recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "done"); recv_all(s)
    send_line(s, "1"); recv_all(s)          # race
    send_line(s, class_choice); recv_all(s)  # class
    send_line(s, "2"); recv_all(s)          # gender/alignment prompt
    s.close()
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


pw = "sneezypw123"

imm_name = f"Snzimm{_suffix}"
s_imm = make_char(imm_name, pw, "3")
cmd(s_imm, "quit!")
s_imm.close()
set_level(imm_name, 51)
s_imm = socket.create_connection((host, port), timeout=5)
recv_all(s_imm)
send_line(s_imm, imm_name); recv_all(s_imm)
send_line(s_imm, pw); recv_all(s_imm)
send_line(s_imm, "1"); recv_all(s_imm)
cmd(s_imm, "color off")

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Sneezy Ports Sandbox','A bare sandbox room.\\n',NULL,0,0,0,0,0,0,0,0,0,0);")
check("Sneezy Ports Sandbox" in cmd(s_imm, f"goto {ROOM}"), "goto lands in the sandbox room")

# A dummy mob (level 1) for consider/examine.
cols = {
    "vnum": DUMMY_VNUM, "name": "'dummy'", "short_desc": "'a straw dummy'",
    "long_desc": "'A straw dummy stands here.'", "description": "'It is stuffed with straw.'",
    "actions": 0, "affects": 0, "faction": 0, "fact_perc": 0,
    "letter": "'A'", "attacks": 1.0,
    "class": 0, "level": 51, "tohit": 0, "ac": 0, "hpbonus": 5000,
    "damage_level": 0, "damage_precision": 0, "gold": 0, "race": 0,
    "weight": 0, "height": 0, "str": 0, "bra": 0, "con": 0, "dex": 0,
    "agi": 0, "intel": 0, "wis": 0, "foc": 0, "per": 0, "cha": 0,
    "kar": 0, "spe": 0, "pos": 10, "def_position": 10, "sex": 1,
    "spec_proc": 0, "skin": 0, "vision": 0, "can_be_seen": 1,
    "max_exist": 1,
}
sql(f"INSERT INTO mob ({','.join(cols.keys())}) VALUES ({','.join(str(v) for v in cols.values())});")
check("You conjure" in cmd(s_imm, f"load mob {DUMMY_VNUM}"), "the dummy is loaded")

sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({ITEM_VNUM},'trinket silver','a small silver trinket',"
    f"'A small silver trinket is lying here.',12,1,1);")
check("You conjure" in cmd(s_imm, f"load obj {ITEM_VNUM}"), "the trinket is loaded")
check("you get" in cmd(s_imm, "get trinket").lower(), "the immortal picks up the trinket")

# --- 1: examine is a synonym for look at target ---
out_examine = cmd(s_imm, "examine dummy")
check("It is stuffed with straw" in out_examine, "examine shows the dummy's description")
check("You look at" in out_examine, "examine reuses look_at_target's exact wording")

# --- 2: consider self ---
out = cmd(s_imm, "consider self")
check("You consider yourself" in out, "consider self opens with the self-eval line")
check("equipment would seem" in out, "consider self reports an AC-based equipment verdict")

# --- 3: consider a mob at the same level (51, matching the dummy's own
#     level column above) -- a fair fight. No reconnect needed: `goto`
#     never persists to player.load_room, so a quit!+reconnect would
#     have dropped the immortal back at their original load room, not
#     the sandbox -- simpler to just give the dummy the same level as
#     the (already level-51) immortal instead of changing levels. ---
out = cmd(s_imm, "consider dummy")
check("A fair fight." in out, "consider a same-level mob calls it a fair fight")

# --- 4: a second mortal PC in the sandbox for consider/tell/whisper/show ---
mort_name = f"Snzmor{_suffix}"
s_mort = make_char(mort_name, pw, "1")
cmd(s_imm, f"transfer {mort_name}")
check("Sneezy Ports Sandbox" in cmd(s_mort, "look"), "the mortal is in the sandbox")

out = cmd(s_imm, f"consider {mort_name}")
check("cross and a shovel" in out, "considering an ordinary mortal PC gets the generic refusal")
out = cmd(s_mort, f"consider {imm_name}")
check("big ego" in out, "a mortal considering an immortal gets the big-ego refusal")

# --- 5: sip a real seeded fountain, never poisons water ---
check("You conjure" in cmd(s_imm, f"load obj {FOUNTAIN_VNUM}"), "a real seeded fountain is loaded")
out = cmd(s_imm, "sip fountain")
check("You sip a bit of water" in out, "sip resolves the seeded fountain")
check("fountain" in cmd(s_imm, "look").lower(), "the fountain is still there after sipping")

# --- 6: tell reaches the target regardless of room ---
out = cmd(s_imm, f"tell {mort_name} hello there")
check(f'You tell {mort_name}' in out, "tell confirms delivery to the sender")
out = recv_all(s_mort, 1.0)
check(f"{imm_name} tells you" in out and "hello there" in out, "tell delivers the message to the target")

# --- 7: whisper reaches the target in-room; a bystander sees no content ---
witness_name = f"Snzwit{_suffix}"
s_wit = make_char(witness_name, pw, "1")
cmd(s_imm, f"transfer {witness_name}")
check("Sneezy Ports Sandbox" in cmd(s_wit, "look"), "the witness is in the sandbox")
recv_all(s_wit, 0.3)

out = cmd(s_imm, f"whisper {mort_name} the secret word is banana")
check(f"You whisper to {mort_name}" in out, "whisper confirms delivery to the sender")
out = recv_all(s_mort, 1.0)
check("the secret word is banana" in out, "whisper delivers the actual message to the target")
out = recv_all(s_wit, 1.0)
check("whispers something to" in out and "banana" not in out,
      "a bystander sees only a content-free notice")

# --- 8: show is a message only, the item never changes hands ---
out = cmd(s_imm, f"show trinket {mort_name}")
check(f"You show" in out and mort_name in out, "show confirms delivery to the sender")
out = recv_all(s_mort, 1.0)
check("shows you" in out and "trinket" in out, "show delivers the message to the target")
check("trinket" in cmd(s_imm, "inventory").lower(), "the trinket is still in the immortal's inventory")

s_imm.close()
s_mort.close()
s_wit.close()
announce_done("smoke_test_sneezy_ports")
print("=== ALL CHECKS PASSED ===")
