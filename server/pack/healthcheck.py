#!/usr/bin/env python3
"""Docker healthcheck: the server answers an rcon status request.

ReHLDS forwards A2S queries to the Steam game-server layer, and the Default
Public Steam client no longer provides the interface version ReHLDS was built
against. The test server therefore answers no A2S_INFO, but rcon is handled by
the engine itself and proves that the map is running.
"""

import os
import re
import socket
import sys

HOST = "127.0.0.1"
PORT = int(os.environ.get("CSR_PORT", "27015"))
PASSWORD = os.environ.get("CSR_RCON_PASSWORD", "")
TIMEOUT = 2.0

CHALLENGE_REQUEST = b"\xff\xff\xff\xffgetchallenge\x00"
CHALLENGE_RE = re.compile(rb"A00000000 (\d+)")


def exchange(payload: bytes) -> bytes:
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.settimeout(TIMEOUT)
        sock.sendto(payload, (HOST, PORT))
        data, _ = sock.recvfrom(4096)
    return data


def main() -> int:
    if not PASSWORD:
        print("CSR_RCON_PASSWORD is not set")
        return 1

    try:
        challenge = CHALLENGE_RE.search(exchange(CHALLENGE_REQUEST))
        if challenge is None:
            print("no challenge response")
            return 1

        request = b"\xff\xff\xff\xffrcon " + challenge.group(1) + b" " + PASSWORD.encode() + b" status\x00"
        response = exchange(request)
    except OSError as exc:
        print(f"no rcon response: {exc}")
        return 1

    if not response.startswith(b"\xff\xff\xff\xffl"):
        print(f"unexpected rcon response: {response[:32]!r}")
        return 1

    status = response[5:].rstrip(b"\x00").decode("latin-1", "replace")
    if "map" not in status or "players" not in status:
        print(f"unexpected rcon status: {status[:120]!r}")
        return 1

    map_name = next((line.split(":", 1)[1].strip() for line in status.splitlines()
                     if line.startswith("map")), "?")
    print(f"rcon status ok: map {map_name}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
