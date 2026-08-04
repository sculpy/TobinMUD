"""Shared low-level helpers for the smoke test suite: raw socket I/O,
the @test announce hook, SQL, and command send/receive. These used to be
copy-pasted verbatim (or near-verbatim) into every smoke_test_*.py file;
consolidated here 2026-08-03 so a change to any of them -- protocol
framing, the @test hook, the mariadb invocation -- only needs editing
once instead of being hunted down across 240+ files.

A handful of tests intentionally keep their OWN local copy of one of
these functions where it's genuinely customized (a different timeout/
idle-gap strategy for a slow-responding command, pagination draining,
etc.) -- those were deliberately left alone by the migration and are
not bugs.

See also mud_creation.py for the character-creation-flow helper.
"""
import socket
import subprocess


def send_line(sock, line):
    sock.sendall((line + "\r\n").encode())


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


def cmd(sock, line, timeout=1.0):
    send_line(sock, line)
    return recv_all(sock, timeout)


def check(condition, message):
    if not condition:
        raise AssertionError(message)
    print(f">>> OK: {message}")


def sql(stmt):
    subprocess.run(["mariadb", "tobin", "-e", stmt], check=True)


def announce(test_name, host, port):
    """Tell the running MUD which smoke test is executing: emits a [TEST]
    log line (visible to online immortals and in the day's log file) via
    the loopback-only `@test` server hook. Best-effort -- never fails the
    test."""
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


def announce_done(test_name, host, port):
    """Companion to announce() -- emits a [TEST] "finished" log line."""
    announce(f"done {test_name}", host, port)
