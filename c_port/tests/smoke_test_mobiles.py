#!/usr/bin/env python3
"""Smoke test for mobiles (Phase 2D: being_create_mob(), mload, and mob
combat integration). Covers:
  1. `mload` is immortal-only (invisible to a mortal, "Huh?!").
  2. `mload <vnum>` spawns a mob prototype into the room; `look` lists it
     generically (same room-contents loop as PCs) and `look <mobname>`
     (a multi-word, abbreviated keyword) shows its description. `mload
     <name>` works too (a substring match against `mob.name`).
  3. A mortal `kill`/`attack`s the mob by an abbreviated keyword; combat
     resolves over multiple rounds via the existing pulse engine.
  4. On defeat, the mob is removed from the room entirely (a bystander's
     `look` no longer shows it) -- mob death is permanent, unlike a PC's.

All setup happens in a SQL-bootstrapped sandbox room + mob prototype at a
high vnum (900000+); the seeded world's own mobs are never touched. The
`mob` table's columns are NOT NULL with no defaults (unlike `obj`), so the
INSERT below supplies every one of them explicitly.

    python3 tests/smoke_test_mobiles.py [host] [port]
"""
import socket
import subprocess
import sys
import time

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

def announce(test_name, host=host, port=port):
    """Tell the running MUD which smoke test is executing: emits a [TEST]
    log line (visible to online immortals and in the day's log file) via
    the loopback-only `@test` server hook. Best-effort -- never fails the
    test. Self-contained (own socket, doesn't depend on this file's other
    helpers) so it can run at any point in the script."""
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
    """Companion to announce() -- emits a [TEST] "finished" log line. Call
    once at the very end of a smoke test, just before it reports success."""
    announce(f"done {test_name}", host, port)


announce("smoke_test_mobiles")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
BASE = 900000 + (int(time.time()) % 70000)

ROOM = BASE
MOB = BASE + 1
MOB2 = BASE + 2  # a uniquely-named fixture for the mload-by-name check --
                 # "vrock demon" is a REAL name already in the seeded `mob`
                 # table at a lower vnum, so a plain substring search would
                 # find that one first, not this fixture


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


def make_char(sock, name, pw):
    recv_all(sock)
    send_line(sock, name); recv_all(sock)
    send_line(sock, pw); recv_all(sock)
    send_line(sock, pw); recv_all(sock)
    send_line(sock, "new"); recv_all(sock)
    send_line(sock, name); recv_all(sock)
    send_line(sock, "done"); recv_all(sock)


def login(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


imm_name = f"Mobtest{_suffix}"
imm_pw = "mobtestpw123"
mort_name = f"Mobmort{_suffix}"
mort_pw = "mobmortpw123"

s = socket.create_connection((host, port), timeout=5)
make_char(s, imm_name, imm_pw)
set_level(imm_name, 51)
s.close()
s = login(imm_name, imm_pw)

# --- bootstrap: one sandbox room, one sandbox mob prototype ---
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Mobile Sandbox','A bare sandbox room.\\n',NULL,0,0,0,0,0,0,0,0,0,0);")

# The `mob` table's columns are NOT NULL with no defaults (unlike `obj`), so
# every one of them needs an explicit value. Built as matched (column,
# value) pairs -- not hand-counted comma-separated strings -- so a mismatch
# between the column list and the value list is structurally impossible.
def mob_insert(vnum, name, short_desc, long_desc, description, level=1, hpbonus=0.3):
    mob_columns_values = [
        ("vnum", str(vnum)),
        ("name", f"'{name}'"),
        ("short_desc", f"'{short_desc}'"),
        ("long_desc", f"'{long_desc}'"),
        ("description", f"'{description}'"),
        ("actions", "0"),
        ("affects", "0"),
        ("faction", "0"),
        ("fact_perc", "0"),
        ("letter", "'A'"),
        ("attacks", "1.0"),
        ("class", "0"),
        ("level", str(level)),
        ("tohit", "0"),
        ("ac", "0"),
        ("hpbonus", str(hpbonus)),
        ("damage_level", "0"),
        ("damage_precision", "0"),
        ("gold", "0"),
        ("race", "0"),
        ("weight", "0"),
        ("height", "0"),
        ("str", "0"),
        ("bra", "0"),
        ("con", "0"),
        ("dex", "0"),
        ("agi", "0"),
        ("intel", "0"),
        ("wis", "0"),
        ("foc", "0"),
        ("per", "0"),
        ("cha", "0"),
        ("kar", "0"),
        ("spe", "0"),
        ("pos", "10"),
        ("def_position", "10"),
        ("sex", "1"),
        ("spec_proc", "0"),
        ("skin", "0"),
        ("vision", "0"),
        ("can_be_seen", "1"),
        ("max_exist", "1"),
    ]
    mob_cols = ",".join(c for c, _ in mob_columns_values)
    mob_vals = ",".join(v for _, v in mob_columns_values)
    sql(f"INSERT INTO mob ({mob_cols}) VALUES ({mob_vals});")


mob_insert(MOB, "vrock demon", "a vrock demon",
           "A vrock demon stands here, waiting to feast on flesh.",
           "This vrock looks like a cross between a vulture and a human.")
# A distinctively-named fixture (unique random tag) so "mload <name>" can be
# tested deterministically -- "vrock"/"demon" would match the real seeded
# mob above (or an even-lower-vnum real one) instead of a test fixture.
namesearch_word = f"zzztestimp{_suffix}"
mob_insert(MOB2, namesearch_word, f"a {namesearch_word}",
           f"A {namesearch_word} lurks here.", f"A small {namesearch_word} grins at you.")

check("Mobile Sandbox" in cmd(s, f"goto {ROOM}"), "goto lands in the SQL-bootstrapped sandbox room")

# --- 1: mload is immortal-only ---
s2 = socket.create_connection((host, port), timeout=5)
tmp_mort = f"Mobtmp{_suffix}"
make_char(s2, tmp_mort, "mobtmppw123")
cmd(s2, "color off")
check("Huh?!" in cmd(s2, f"mload {MOB}"), "mload is invisible to a mortal")
s2.close()

# --- 2: mload spawns the prototype; look lists it; look <name> shows description ---
check("You conjure" in cmd(s, f"mload {MOB}"), "mload confirms")
out = cmd(s, "look")
check("vrock demon is here" in out.lower(), "look lists the mob generically, same as a PC")
out = cmd(s, "look vrock")
check("cross between a vulture" in out, "look <mob> (by one keyword) shows its description")
out = cmd(s, "look demon")
check("cross between a vulture" in out, "look <mob> also matches its OTHER keyword")

# --- 2b: mload also accepts a name/keyword, not just a vnum ---
check("You conjure" in cmd(s, f"mload {namesearch_word}"), "mload accepts a name in place of a vnum")
# The room listing shows a mob's short_desc + "is here." (same as a PC),
# not its long_desc -- matches the earlier "vrock demon is here" check.
check(f"a {namesearch_word} is here" in cmd(s, "look").lower(),
      "the name-looked-up mobile actually spawned")

# --- 3: a mortal fights the mob by an abbreviated keyword ---
sm = socket.create_connection((host, port), timeout=5)
make_char(sm, mort_name, mort_pw)
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{mort_name}';")
sm.close()
sm = login(mort_name, mort_pw)
check("Mobile Sandbox" in cmd(sm, "look"), "the mortal lands directly in the sandbox room")

out = cmd(sm, "kill vroc")
check("Huh?!" not in out, "kill reaches the mob by an abbreviated keyword")

# Let a few combat rounds actually play out.
deadline = time.time() + 20
mob_dead = False
all_out = ""
while time.time() < deadline:
    all_out += recv_all(sm, timeout=1.5)
    if "have slain" in all_out.lower() or "have defeated" in all_out.lower():
        mob_dead = True
        break
check(mob_dead or "hit" in all_out.lower() or "miss" in all_out.lower(),
      "combat rounds are actually happening (hits/misses exchanged)")

# --- 4: finish it off with an immortal instakill, confirm permanent removal ---
out = cmd(s, "kill vrock")
check("slain" in out.lower() or "aren't here" in out.lower(),
      "an immortal's kill instakills the mob (or it's already dead from step 3)")

out = cmd(s, "look")
check("vrock demon" not in out.lower(), "the mob is gone from the room for good after defeat")

s.close()
sm.close()
announce_done("smoke_test_mobiles")
print("=== ALL CHECKS PASSED ===")
