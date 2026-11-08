#!/usr/bin/env python3
"""Checks for the CSRETRO test server.

The test server runs in Docker and is driven by tests/e2e/run.sh, which passes
the captured container log and the published port on the command line. The
checks are deliberately protocol-level: they prove that the engine, Metamod-R,
ReGameDLL, AMX Mod X and the CSRETRO module all came up, that the map loaded,
and that rcon answers.

A2S is not checked. ReHLDS forwards connectionless queries to the Steam
game-server layer, and the test server answers the SteamGameServer011 ABI with
a stub instead of the Steam client, so no A2S_INFO is available. rcon is handled
by the engine itself and is the health signal for this gate.
"""

from __future__ import annotations

import argparse
import re
import socket
import sys

CHALLENGE_REQUEST = b"\xff\xff\xff\xffgetchallenge\x00"
CHALLENGE_RE = re.compile(rb"A00000000 (\d+)")

# Lines that must appear in the server log.
REQUIRED_LOG_LINES = [
    "Metamod-r v1.3.0.149",
    "AMX Mod X version 1.10.0.5487",
    "[nclapi] Next Client API Successfully loaded, version 1.6.0",
    'Started map "de_dust2"',
]

# Lines that indicate a broken server even though the process is still alive.
FORBIDDEN_LOG_LINES = [
    "FATAL ERROR",
    "Fencepost",
    "Segmentation fault",
    "Uploading dump",
    "[AMXX] Run time error",
]


class CheckFailure(Exception):
    pass


def check(condition: bool, message: str) -> None:
    if not condition:
        raise CheckFailure(message)


def rcon_exchange(host: str, port: int, payload: bytes, timeout: float) -> bytes:
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.settimeout(timeout)
        sock.sendto(payload, (host, port))
        data, _ = sock.recvfrom(65535)
    return data


def rcon_command(host: str, port: int, password: str, command: str) -> str:
    challenge_packet = rcon_exchange(host, port, CHALLENGE_REQUEST, timeout=3.0)
    challenge = CHALLENGE_RE.search(challenge_packet)
    check(challenge is not None, f"no challenge in reply: {challenge_packet[:40]!r}")

    request = (
        b"\xff\xff\xff\xffrcon "
        + challenge.group(1)
        + b" "
        + password.encode()
        + b" "
        + command.encode()
        + b"\x00"
    )

    chunks: list[bytes] = []
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.settimeout(3.0)
        sock.sendto(request, (host, port))
        while True:
            try:
                data, _ = sock.recvfrom(65535)
            except socket.timeout:
                break
            chunks.append(data)
            if not data.endswith(b"\x00"):
                break

    check(bool(chunks), f"rcon {command} returned no data")
    response = b"".join(chunks)
    check(response.startswith(b"\xff\xff\xff\xffl"), f"unexpected rcon header: {response[:16]!r}")

    return response[5:].rstrip(b"\x00").decode("latin-1", "replace")


def check_rcon(host: str, port: int, password: str, expected_map: str) -> list[str]:
    results: list[str] = []

    status = rcon_command(host, port, password, "status")
    lines = status.splitlines()

    for key in ("hostname", "version", "map", "players"):
        check(any(line.startswith(key) for line in lines), f"rcon status has no {key} line: {status[:200]!r}")
        results.append(next(line for line in lines if line.startswith(key)))

    map_line = next(line for line in lines if line.startswith("map"))
    check(expected_map in map_line, f"rcon status reports the wrong map: {map_line!r}")

    module_version = rcon_command(host, port, password, "nclapi_version")
    check("1.6.0" in module_version, f"nclapi_version is not 1.6.0: {module_version!r}")
    results.append(module_version.strip())

    plugins = rcon_command(host, port, password, "amxx plugins")
    check("Test NCL API" in plugins, f"test plugin is not listed: {plugins[:300]!r}")
    check("1 running" in plugins, f"test plugin is not running: {plugins[:300]!r}")
    results.append("amxx plugins: Test NCL API running")

    return results


def check_log(log: str) -> list[str]:
    missing = [line for line in REQUIRED_LOG_LINES if line not in log]
    check(not missing, "server log is missing: " + ", ".join(missing))

    found = [line for line in FORBIDDEN_LOG_LINES if line in log]
    check(not found, "server log contains: " + ", ".join(found))

    return [line for line in REQUIRED_LOG_LINES if line in log]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--log", required=True, help="captured docker compose log of the server service")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, required=True, help="published rcon/udp port")
    parser.add_argument("--password", required=True, help="rcon password")
    parser.add_argument("--map", default="de_dust2", help="map that must be running")
    args = parser.parse_args()

    with open(args.log, "r", encoding="utf-8", errors="replace") as handle:
        log = handle.read()

    failures = 0

    try:
        for line in check_log(log):
            print(f"ok   log: {line}")
    except CheckFailure as exc:
        failures += 1
        print(f"FAIL log: {exc}")

    try:
        for line in check_rcon(args.host, args.port, args.password, args.map):
            print(f"ok   rcon: {line}")
    except (CheckFailure, OSError) as exc:
        failures += 1
        print(f"FAIL rcon: {exc}")

    if failures:
        print(f"\n{failures} check(s) failed")
        return 1

    print("\nall e2e checks passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
