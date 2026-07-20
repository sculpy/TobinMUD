#!/usr/bin/env python3
"""Regression test for a weather bug report (user: "weather should not
affect rooms that are flagged indoors"). weather_announce() (weather.c)
used to notify EVERY connected character on a sky-state change,
regardless of room -- someone standing inside a building would still
see "Clouds begin to gather overhead"/"It begins to rain" despite being
unable to see the sky. Fixed with the same ROOM_FLAG_INDOORS check
room_is_dark_for() (being.c) already uses for the darkness half of this
same "Weather & light levels" audit item -- a weather-change
announcement is exactly the kind of sky-visibility-dependent content
that check exists for.

Covers: on the same weather-change tick, an outdoor character receives
the announcement and an indoor one (same tick, same change) does not.

    python3 tests/smoke_test_weather_indoors.py [host] [port]
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


announce("smoke_test_weather_indoors")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM_OUTDOOR = 990000 + (int(time.time()) % 8000)  # ALWAYS_LIT only -- not INDOORS
ROOM_INDOORS = ROOM_OUTDOOR + 1                      # ALWAYS_LIT | INDOORS

WEATHER_PHRASES = (
    "gather overhead", "sky clears up", "begins to rain",
    "leaving the sky overcast", "intensifies into a full storm",
    "eases back into steady rain",
)


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


def make_char(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "y"); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "new"); recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "1"); recv_all(s)  # race: human
    send_line(s, "1"); recv_all(s)  # class: mage
    send_line(s, "done"); recv_all(s)
    send_line(s, "2"); recv_all(s)  # alignment: neutral
    return s


def relog(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


def has_weather_phrase(text):
    low = text.lower()
    return any(p in low for p in WEATHER_PHRASES)


imm_name, imm_pw = f"Wtridimm{_suffix}", "wtridimmpw12"
si = make_char(imm_name, imm_pw)
cmd(si, "quit!"); si.close()
sql(f"UPDATE player_progress SET level=51 WHERE player_id=(SELECT id FROM player WHERE name='{imm_name}');")
si = relog(imm_name, imm_pw)

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM_OUTDOOR},0,0,0,'Weather Indoors Sandbox Outdoor','Open sky overhead.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM_INDOORS},0,0,0,'Weather Indoors Sandbox Indoors','A snug room with a solid roof.\\n',NULL,9,0,0,0,0,0,0,0,0,0);")

outA_name, outA_pw = f"Wtrida{_suffix}", "wtridapw123"
sA = make_char(outA_name, outA_pw)
cmd(sA, "quit!"); sA.close()
sql(f"UPDATE player SET load_room={ROOM_OUTDOOR} WHERE name='{outA_name}';")
sA = relog(outA_name, outA_pw)

inB_name, inB_pw = f"Wtridb{_suffix}", "wtridbpw123"
sB = make_char(inB_name, inB_pw)
cmd(sB, "quit!"); sB.close()
sql(f"UPDATE player SET load_room={ROOM_INDOORS} WHERE name='{inB_name}';")
sB = relog(inB_name, inB_pw)

# Weather changes are probabilistic per tick (weather.c's weighted
# transition table) -- force ticks and watch both sockets each round
# until a real change actually broadcasts, same "force + poll" shape
# smoke_test_weather.py already uses to land on a specific sky state.
changed = False
outdoor_saw_it = False
indoor_saw_it = False
for _ in range(50):
    cmd(si, "aitick 1")
    out_a = cmd(sA, "", timeout=0.3)
    out_b = cmd(sB, "", timeout=0.3)
    if has_weather_phrase(out_a) or has_weather_phrase(out_b):
        changed = True
        outdoor_saw_it = has_weather_phrase(out_a)
        indoor_saw_it = has_weather_phrase(out_b)
        break

check(changed, "a real weather-state change broadcast within a bounded number of forced ticks")
check(outdoor_saw_it, "the outdoor character received the weather-change announcement")
check(not indoor_saw_it, "the indoor character did NOT receive it -- weather stays outside")

cmd(sA, "quit!"); sA.close()
cmd(sB, "quit!"); sB.close()
si.close()
announce_done("smoke_test_weather_indoors")
print("=== ALL CHECKS PASSED ===")
