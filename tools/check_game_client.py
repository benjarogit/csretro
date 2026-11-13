#!/usr/bin/env python3
"""Inspect a native game client's ELF header and core exports without loading it.

This is an export screen, not an ABI, gameplay or performance certification.
The CSRETRO profile uses the traditional named GoldSrc entry points; a lone
CreateInterface or GetClientAPI does not establish that contract.
"""

from __future__ import annotations

import argparse
import json
import struct
import subprocess
from pathlib import Path

# Core callbacks used by our client integration. Signatures live in
# client/nextclient/goldsrc_abi/include/goldsrc/client_api.h.
REQUIRED_EXPORTS = (
    "Initialize", "HUD_Init", "HUD_VidInit", "HUD_Redraw",
    "HUD_UpdateClientData", "HUD_PlayerMove", "HUD_PlayerMoveInit",
    "CL_CreateMove", "V_CalcRefdef", "HUD_GetStudioModelInterface",
)


def inspect_client(binary: Path) -> dict:
    result = {"export_screen": "fail", "playability": "unverified", "errors": []}
    errors = result["errors"]
    try:
        with binary.open("rb") as stream:
            header = stream.read(64)
        if len(header) != 64 or header[:6] != b"\x7fELF\x02\x01":
            errors.append("expected a little-endian ELF64 shared library")
            return result
        kind, machine = struct.unpack_from("<HH", header, 16)
        if kind != 3 or machine != 62:
            errors.append("expected ET_DYN for x86_64")
            return result
        symbols = subprocess.run(
            ["nm", "-D", "--defined-only", "--format=posix", "--", str(binary)],
            capture_output=True, text=True, check=False,
        )
        if symbols.returncode:
            errors.append("cannot read dynamic symbols: " + symbols.stderr.strip())
            return result
        functions = set()
        for line in symbols.stdout.splitlines():
            fields = line.split()
            if len(fields) >= 2 and fields[1] in {"T", "W"}:
                functions.add(fields[0].split("@", 1)[0])
        missing = sorted(set(REQUIRED_EXPORTS) - functions)
        if missing:
            errors.append("missing dynamic function exports: " + ", ".join(missing))
        else:
            result["export_screen"] = "pass"
    except (OSError, ValueError) as error:
        errors.append(str(error))
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("client", type=Path, help="game client shared library, not the hook module")
    args = parser.parse_args()
    result = inspect_client(args.client)
    print(json.dumps(result, indent=2))
    return 0 if result["export_screen"] == "pass" else 1


if __name__ == "__main__":
    raise SystemExit(main())
