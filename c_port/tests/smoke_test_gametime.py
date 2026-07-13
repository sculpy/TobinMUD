#!/usr/bin/env python3
"""Smoke test for the time/day/date system (Session 43, ported from
Sneezy's GameTime class -- user: "implement time/day/date system from
sneezys example"). Covers:
  1. `time` shows a well-formed clock ("H:MM AM/PM"), a real weekday name,
     and a date line ("The Nth day of <Month>, Year N.").
  2. The shown weekday matches Sneezy's own formula
     ((28*month + day + 1) % 7), computed independently in this test from
     the displayed date -- not just "some weekday string appeared".
  3. The clock actually advances over real time (waits past one ~60s
     pulse tick) -- specifically exercises the pulse_register() path that
     had a silent-failure risk fixed this same session (MAX_PULSE_PROCESSES).

    python3 tests/smoke_test_gametime.py [host] [port]
"""
import re
import socket
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


announce("smoke_test_gametime")

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


def check(condition, message):
    if not condition:
        raise AssertionError(message)
    print(f">>> OK: {message}")


MONTHS = ["January", "February", "March", "April", "May", "June", "July",
          "August", "September", "October", "November", "December"]
WEEKDAYS = ["Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"]

TIME_RE = re.compile(
    r"It is (\d{1,2}):(\d{2}) (AM|PM), on (\w+)\r?\n"
    r"The (\d{1,2})\w{2} day of (\w+), Year (\d+)\.")


def parse_time(out):
    m = TIME_RE.search(out)
    check(m is not None, f"`time` output matches the expected format (got: {out!r})")
    hour12, minute, ampm, weekday, day, month, year = m.groups()
    hour24 = int(hour12) % 12 + (12 if ampm == "PM" else 0)
    return {
        "hour24": hour24, "minute": int(minute), "weekday": weekday,
        "day": int(day), "month": month, "year": int(year),
    }


name = f"Gametime{_suffix}"
pw = "gametimepw123"
s = socket.create_connection((host, port), timeout=5)
recv_all(s)
send_line(s, name); recv_all(s)
send_line(s, "y"); recv_all(s)
send_line(s, pw); recv_all(s)
send_line(s, pw); recv_all(s)
send_line(s, "new"); recv_all(s)
send_line(s, name); recv_all(s)
send_line(s, "1"); recv_all(s)  # race: human (zero stat modifier)
send_line(s, "1"); recv_all(s)  # class: mage
send_line(s, "done"); recv_all(s)
send_line(s, "2"); recv_all(s)  # alignment: neutral
s.close()

s = socket.create_connection((host, port), timeout=5)
recv_all(s)
send_line(s, name); recv_all(s)
send_line(s, pw); recv_all(s)
send_line(s, "1"); recv_all(s)
cmd(s, "color off")

# --- 1/2: well-formed output + weekday formula check ---
out1 = cmd(s, "time")
t1 = parse_time(out1)
check(t1["month"] in MONTHS, "the shown month is a real month name")
check(t1["weekday"] in WEEKDAYS, "the shown weekday is a real weekday name")
check(1 <= t1["day"] <= 28, "the shown day is in [1, 28] (a 28-day month)")

day0 = t1["day"] - 1  # the command displays 1-indexed; the formula is 0-indexed
month0 = MONTHS.index(t1["month"])
expected_weekday = WEEKDAYS[(28 * month0 + day0 + 1) % 7]
check(t1["weekday"] == expected_weekday,
      f"the shown weekday ({t1['weekday']}) matches Sneezy's own formula "
      f"(28*month + day + 1) mod 7 -> expected {expected_weekday}")

# --- 3: the clock actually advances over real time (one pulse tick, ~60s) ---
time.sleep(70)
out2 = cmd(s, "time")
t2 = parse_time(out2)
total1 = t1["hour24"] * 60 + t1["minute"]
total2 = t2["hour24"] * 60 + t2["minute"]
delta = (total2 - total1) % (24 * 60)
check(delta > 0, "the clock actually advanced after waiting past a pulse tick "
                  "(exercises the pulse_register() path fixed for silent failure this session)")
check(delta % 15 == 0, f"the clock advanced by a whole number of 15-minute ticks (delta={delta})")

s.close()
announce_done("smoke_test_gametime")
print("=== ALL CHECKS PASSED ===")
