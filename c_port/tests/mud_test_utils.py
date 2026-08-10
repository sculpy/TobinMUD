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
import random
import re
import socket
import string
import subprocess
import time


def send_line(sock, line):
    sock.sendall((line + "\r\n").encode())


def recv_all(sock, timeout=1.0, deadline=None):
    """Read until `timeout` seconds pass with nothing arriving, OR
    `deadline` total seconds have elapsed since the call started --
    whichever comes first.

    2026-08-10 root-cause investigation (see TODO.md's former
    "Intermittent scripted-input desync/hang" item, now split in two):
    the original version below re-armed `timeout` on every single byte
    received, with no absolute cap. Any socket that keeps seeing ANY
    traffic faster than `timeout` -- a nearby fight, a telnet keepalive
    NOP, another test's characters in the same room -- blocked here
    forever. That was mistaken for a real server-side hang in earlier
    sessions; a controlled repro proved a 45x overshoot (a socket kept
    "warm" by a steady trickle never satisfied the idle gap at all).
    `deadline` defaults to a generous multiple of `timeout` specifically
    so nothing that was already passing changes behavior -- it only
    catches the genuinely pathological case that used to hang a whole
    test run.

    This function still has NO notion of a command/response boundary --
    it just returns whatever showed up in the time it had. For anything
    that needs to know its response actually arrived (not stale
    backlog, not split across a prompt), use read_until()/cmd_until()/
    sync() below instead. Kept as the default for cmd() because 280+
    existing tests already tolerate its heuristics; don't change what
    it returns in the common case, only bound the worst case.
    """
    if deadline is None:
        deadline = max(8.0, timeout * 8)
    end = time.monotonic() + deadline
    chunks = []
    while True:
        remaining = end - time.monotonic()
        if remaining <= 0:
            break
        sock.settimeout(min(timeout, remaining))
        try:
            data = sock.recv(4096)
        except socket.timeout:
            break
        if not data:
            break
        chunks.append(data)
    return b"".join(chunks).decode(errors="replace")


def cmd(sock, line, timeout=1.0):
    send_line(sock, line)
    return recv_all(sock, timeout)


def drain(sock, quiet=0.25, deadline=5.0):
    """Read and DISCARD from `sock` until `quiet` seconds pass with
    nothing arriving, or `deadline` total seconds elapse. Call this on
    EVERY socket that was part of a multi-character scene (combat,
    a group event, anything async) before trusting that socket's next
    `cmd()` response -- an undrained socket hands back whatever old
    backlog is sitting in it instead of the real response, which reads
    as a failed command when the command actually worked. This is
    exactly the bug smoke_test_missing_skills_batchc.py hit live
    (2026-08-10): a 15s melee only drained one of several participants'
    sockets, and the next command on an undrained one silently returned
    stale combat text.
    """
    recv_all(sock, timeout=quiet, deadline=deadline)


def read_until(sock, pattern, deadline=8.0):
    """Accumulate data from `sock` until `pattern` (a literal string or
    a compiled regex) appears in the accumulated text, or `deadline`
    seconds elapse. Returns everything read so far either way -- check
    the pattern's presence yourself if you need to distinguish success
    from timeout, or use cmd_until() which raises for you.

    Unlike recv_all(), this asserts on CONTENT ARRIVING, never on a
    fixed idle window -- the right primitive when you know what the
    real response looks like (a prompt, a specific word) and don't want
    to guess how long it takes to show up.
    """
    matcher = pattern if hasattr(pattern, "search") else re.compile(re.escape(pattern))
    end = time.monotonic() + deadline
    buf = ""
    sock.settimeout(0.5)
    while time.monotonic() < end:
        if matcher.search(buf):
            return buf
        try:
            data = sock.recv(4096)
        except socket.timeout:
            continue
        if not data:
            break
        buf += data.decode(errors="replace")
    return buf


def cmd_until(sock, line, pattern, deadline=8.0, presync=True):
    """The alignment-safe primitive for new tests: optionally drain any
    stale backlog first (presync=True, the safe default), send `line`,
    then read_until() the response actually contains `pattern`. Raises
    TimeoutError (naming the pattern and showing the tail of what WAS
    read) instead of silently returning a misaligned chunk the way a
    bare cmd() can.
    """
    if presync:
        drain(sock, quiet=0.1, deadline=1.0)
    send_line(sock, line)
    out = read_until(sock, pattern, deadline=deadline)
    matcher = pattern if hasattr(pattern, "search") else re.compile(re.escape(pattern))
    if not matcher.search(out):
        tail = out[-300:] if len(out) > 300 else out
        raise TimeoutError(
            f"cmd_until({line!r}): {pattern!r} never appeared within {deadline}s; "
            f"last {len(tail)} chars received: {tail!r}"
        )
    return out


def sync(sock, deadline=5.0):
    """In-band sequence point for a CONN_PLAYING socket: sends a unique
    junk token as a command and reads until the server's own echo of
    that token comes back. descriptor.c echoes a typed line back
    verbatim as the first chunk of its response (confirmed live,
    2026-08-10 investigation) -- seeing the token echoed guarantees
    every byte the server had already generated BEFORE this call is
    now sitting in the socket, drained along with it. This is the one
    POSITIVE boundary marker available (drain()/read_until() are both
    time-based heuristics); prefer it when you need real certainty
    that a prior async event (combat, another player's action) has
    fully landed before your next command. Does not work at a
    password/menu prompt that suppresses echo or doesn't dispatch
    typed text as a command -- CONN_PLAYING only.
    """
    token = "SYNC" + "".join(random.choices(string.ascii_uppercase, k=8))
    send_line(sock, token)
    return read_until(sock, token, deadline=deadline)


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
