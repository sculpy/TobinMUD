#!/usr/bin/env python3
"""Smoke test for `bamfin`/`bamfout` (user, 2026-07-11: "bamfin|out should
modify goto messaging and the current bamfin|out should be called
something else following the in|out syntax"; follow-ups the same session:
"<N> should work in this as well as $g" and "and $p"). The WALKING
move-message feature that used to own this name is now `poofin`/`poofout`
(see smoke_test_poof.py) -- this file covers `goto`'s own teleport
messages.

  1. Immortal-only: a mortal gets "Command not found" from `bamfin`/`bamfout`.
  2. `goto` broadcasts a departure message (bamfout) to the room left and
     an arrival message (bamfin) to the room entered -- the mover's own
     private "You vanish..." line is unaffected either way.
  3. A custom template's `<N>` token embeds the mover's name (no separate
     name prefix then); `$p` renders the mover's gender_possess() pronoun;
     `$g` renders the destination/departure room's ground-surface word
     (plain sandbox rooms -> "ground").
  4. `bamfin none`/`bamfout none` clears it, reverting to the default
     "<Name> disappears/appears in a puff of smoke." wording.

    python3 tests/smoke_test_bamf.py [host] [port]
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


announce("smoke_test_bamf")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM_A = 900000 + (int(time.time()) % 70000)
ROOM_B = ROOM_A + 1


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


sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM_A},0,0,0,'Bamf Room A','A bare sandbox room.\\n',NULL,0,0,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM_B},0,0,0,'Bamf Room B','A bare sandbox room.\\n',NULL,0,0,0,0,0,0,0,0,0,0);")

# --- 1: mortal can't set a bamf message ---
mort_name = f"Bamfmort{_suffix}"
mort_pw = "Bamfmortpw123"
sm = socket.create_connection((host, port), timeout=5)
make_char(sm, mort_name, mort_pw)
out = cmd(sm, "bamfout <N> steps through a shimmering rift.")
check("Command not found" in out, "bamfout is refused for a mortal")
sm.close()

# --- 2/3: an immortal's custom bamfout/bamfin fire on goto, with tokens ---
imm_name = f"Bamfimm{_suffix}"
imm_pw = "Bamfimmpw123"
s = socket.create_connection((host, port), timeout=5)
make_char(s, imm_name, imm_pw)
sql(f"UPDATE player SET gender=1 WHERE name='{imm_name}';")  # 1 = male
sql(f"UPDATE player SET load_room={ROOM_A} WHERE name='{imm_name}';")
cmd(s, "quit!")
s.close()
set_level(imm_name, 51)
s = login(imm_name, imm_pw)

out = cmd(s, "bamfout <N> sinks into $p own shadow, leaving the $g bare.")
check("Bamfout set to: <N> sinks into $p own shadow, leaving the $g bare." in out,
      "bamfout confirms the stored template")
out = cmd(s, "bamfin <N> rises up out of the $g, dusting $p shoulders off.")
check("Bamfin set to: <N> rises up out of the $g, dusting $p shoulders off." in out,
      "bamfin confirms the stored template")

witness_a_name = f"Bamfwitna{_suffix}"
witness_a_pw = "Bamfwitnessapw123"
sa = socket.create_connection((host, port), timeout=5)
make_char(sa, witness_a_name, witness_a_pw)
sql(f"UPDATE player SET load_room={ROOM_A} WHERE name='{witness_a_name}';")
cmd(sa, "quit!")
sa.close()
sa = login(witness_a_name, witness_a_pw)
check("Bamf Room A" in cmd(sa, "look"), "the first bystander lands in room A with the immortal")

witness_b_name = f"Bamfwitnb{_suffix}"
witness_b_pw = "Bamfwitnessbpw123"
sb = socket.create_connection((host, port), timeout=5)
make_char(sb, witness_b_name, witness_b_pw)
sql(f"UPDATE player SET load_room={ROOM_B} WHERE name='{witness_b_name}';")
cmd(sb, "quit!")
sb.close()
sb = login(witness_b_name, witness_b_pw)
check("Bamf Room B" in cmd(sb, "look"), "the second bystander lands in room B")

recv_all(sa, timeout=0.3)
recv_all(sb, timeout=0.3)
mover_out = cmd(s, f"goto {ROOM_B}", timeout=1.5)
check("You vanish in a puff of smoke." in mover_out,
      "the mover's own private message is unaffected by the custom bamfout template")

witness_a_out = recv_all(sa, timeout=0.5)
check(f"{imm_name} sinks into his own shadow, leaving the ground bare." in witness_a_out,
      "room A sees the custom bamfout: <N> is the name, $p is 'his', $g is 'ground'")

witness_b_out = recv_all(sb, timeout=0.5)
check(f"{imm_name} rises up out of the ground, dusting his shoulders off." in witness_b_out,
      "room B sees the custom bamfin: <N>/$g/$p all rendered")

# --- 4: clearing reverts to the default puff-of-smoke wording ---
out = cmd(s, "bamfout none")
check("Bamfout cleared" in out, "bamfout none clears the custom message")
out = cmd(s, "bamfin none")
check("Bamfin cleared" in out, "bamfin none clears the custom message")

recv_all(sa, timeout=0.3)
recv_all(sb, timeout=0.3)
cmd(s, f"goto {ROOM_A}")
witness_b_out2 = recv_all(sb, timeout=0.5)
check(f"{imm_name} disappears in a puff of smoke." in witness_b_out2,
      "after clearing, room B sees the default departure wording")

s.close()
sa.close()
sb.close()

print("=== ALL CHECKS PASSED ===")
announce_done("smoke_test_bamf")
