#!/usr/bin/env python3
"""Smoke test for the `skills` command (user 2026-07-11: "assign all
warrior skills to warriors in three disciplines: combat, warrior skills,
advanced warrior skills" -- repeated per class through Thief/Monk/Cleric/
Mage). Covers:

  1. A fresh Warrior sees the Combat/Warrior Skills/Advanced Warrior
     Skills headers, with level-1 skills shown known and higher-level
     ones shown locked with their required level.
  2. A fresh Thief, Monk, Cleric, and Mage each see their own class's
     headers and at least one signature skill/spell.
  3. Leveling up unlocks a previously-locked skill (no more "(level N)"
     suffix, no longer dimmed).

    python3 tests/smoke_test_skills.py [host] [port]
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


announce("smoke_test_skills")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))


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


def cmd_paged(sock, line, timeout=1.0, max_pages=10):
    # `skills` now pages its output (2026-07-17 pagination fix) -- a
    # single class's 3-tier roster easily exceeds the pager's default
    # 20-line page, so the checks below need the WHOLE listing, not just
    # page 1. Keep hitting ENTER until no "more" prompt remains.
    out = cmd(sock, line, timeout)
    pages = 0
    while "ENTER" in out and "more" in out and pages < max_pages:
        out += cmd(sock, "", timeout)
        pages += 1
    return out


def check(condition, message):
    if not condition:
        raise AssertionError(message)
    print(f">>> OK: {message}")


def sql(stmt):
    subprocess.run(["mariadb", "sneezy", "-e", stmt], check=True)


def set_level(name, level):
    sql(f"UPDATE player_progress SET level={level} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def set_disciplines(name, basic=100, combat=100, advanced=100):
    # Practice-system redesign (2026-07-17): Combat is no longer "innate" --
    # it's a real discipline gated by combat_disc_pct just like Basic/
    # Advanced, so a fresh 0%-everywhere character now sees every skill as
    # locked-by-discipline. This test is about the SKILLS DISPLAY (headers,
    # level-gating), not the practice system itself (see
    # smoke_test_practice.py for that) -- so max out all three disciplines
    # up front, sidestepping the discipline gate entirely.
    sql(f"UPDATE player_progress SET basic_disc_pct={basic}, combat_disc_pct={combat}, "
        f"advanced_disc_pct={advanced} WHERE player_id="
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
    send_line(s, "1"); recv_all(s)  # race: human
    send_line(s, class_choice); recv_all(s)
    send_line(s, "done"); recv_all(s)
    send_line(s, "2"); recv_all(s)  # alignment: neutral
    cmd(s, "color off")
    set_disciplines(name)
    s.close()
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


# --- 1: warrior sees all 3 tiers, level-gated skills marked locked ---
warrior_name = f"Skwar{_suffix}"
pw = "skillstestpw123"
sw = make_char(warrior_name, pw, "3")
out = cmd_paged(sw, "skills")
check("-- Combat --" in out, "warrior sees the Combat tier header")
check("-- Warrior Skills --" in out, "warrior sees the Warrior Skills tier header")
check("-- Advanced Warrior Skills --" in out, "warrior sees the Advanced Warrior Skills tier header")
check("bash" in out and "(level" not in out.split("bash")[1].split("\n")[0],
      "a level-1 skill (bash) shows known, not locked")
check("(level 50)" in out,
      "a higher-level skill shows its required level while locked "
      "('close quarters fighting', the warrior roster's highest min_level)")

# --- 2: level up unlocks a previously-locked skill ---
set_level(warrior_name, 45)
sw.close()
sw = socket.create_connection((host, port), timeout=5)
recv_all(sw)
send_line(sw, warrior_name); recv_all(sw)
send_line(sw, pw); recv_all(sw)
send_line(sw, "1"); recv_all(sw)
cmd(sw, "color off")
out = cmd_paged(sw, "skills")
disarm_line = [l for l in out.splitlines() if "disarm" in l][0]
check("(level" not in disarm_line, "leveling up to 45 unlocks 'disarm' (no longer marked locked)")

# --- 3: thief, monk, cleric, mage each see their own class's tiers ---
for class_choice, cls_label, signature in (
    ("4", "Thief", "backstab"),
    ("6", "Monk", "yoginsa"),
    ("2", "Cleric", "heal light"),
    ("1", "Mage", "wizardry"),
    ("5", "Druid", "barkskin"),
):
    name = f"Sk{cls_label[:3]}{_suffix}"
    s = make_char(name, pw, class_choice)
    out = cmd_paged(s, "skills")
    check(f"-- {cls_label} Skills --" in out, f"{cls_label} sees the '{cls_label} Skills' tier header")
    check(f"-- Advanced {cls_label} Skills --" in out, f"{cls_label} sees the 'Advanced {cls_label} Skills' tier header")
    check(signature in out, f"{cls_label} sees its signature skill/spell ({signature})")
    s.close()

# --- 4: an immortal's `skills` shows every class's full roster in one go
# (2026-07-20 crash fix). This branch previously segfaulted the live
# server: the ~300-skill catalog across every class, all shown as known
# at once, ran well past its old 16000-byte buffer, and several of the
# buffer-accumulating writes had no overflow guard at all. Real
# reproduction, not a hypothetical -- caught live via gdb during a full
# regression sweep. The whole point of this check is that the connection
# survives to `=== ALL CHECKS PASSED ===`, not just that these strings
# appear. ---
imm_name = f"Skimm{_suffix}"
si = make_char(imm_name, pw, "3")
set_level(imm_name, 60)
si.close()
si = socket.create_connection((host, port), timeout=5)
recv_all(si)
send_line(si, imm_name); recv_all(si)
send_line(si, pw); recv_all(si)
send_line(si, "1"); recv_all(si)
cmd(si, "color off")
out = cmd_paged(si, "skills", timeout=0.3, max_pages=60)
check("=== Warrior ===" in out and "=== Mage ===" in out and "=== Thief ===" in out,
      "an immortal's `skills` shows every class's own section")
check("-- Advanced Mage Skills --" in out,
      "the full catalog (every class, every tier) renders without truncating early")
check("Exits:" in cmd(si, "look"),
      "the connection is still alive and responsive after the full listing (server didn't crash)")
si.close()

sw.close()
announce_done("smoke_test_skills")
print("=== ALL CHECKS PASSED ===")
