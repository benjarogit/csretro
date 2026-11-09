#!/usr/bin/env python3
"""Check the Default Public address providers against the retail binaries.

The providers in `nitro_api` name every engine and client symbol they resolve.
A name that the binary does not export, or a string literal that is not unique,
would silently leave a hook unresolved. This tool reads the provider sources,
compares them with the installed game and reports every mismatch.

Usage:

    python3 tools/check_address_providers.py --game-dir /path/to/Half-Life

Exit code 0 means every entry resolves. Exit code 1 means at least one entry is
missing or ambiguous and the failures were printed. The tool needs `nm` from
binutils and no Python packages.
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

PROVIDERS = [
    (
        "hw.so",
        ROOT / "client/nextclient/nitro_api/src/modules/engine/EngineAddressProviderPublicLinux.cpp",
    ),
    (
        "cstrike/cl_dlls/client.so",
        ROOT / "client/nextclient/nitro_api/src/modules/client/ClientAddressProviderPublicLinux.cpp",
    ),
]

ENTRY = re.compile(r'SearchConfig\("((?:[^"\\]|\\.)*)",\s*SearchType::(\w+)')
SUPPORTED_TYPES = {"SymbolOrPattern", "String"}


def read_symbols(binary: Path) -> set[str]:
    """Union of dynamic and full symbol tables, without symbol versions."""

    symbols: set[str] = set()

    for arguments in (["-D", "--defined-only"], ["--defined-only"]):
        result = subprocess.run(
            ["nm", *arguments, str(binary)],
            capture_output=True,
            text=True,
        )

        if result.returncode != 0:
            raise SystemExit(f"nm failed for {binary}: {result.stderr.strip()}")

        for line in result.stdout.splitlines():
            fields = line.split()

            if not fields:
                continue

            name = fields[-1].split("@", 1)[0]
            symbols.add(name)

    return symbols


def check_provider(binary: Path, source: Path) -> list[str]:
    failures: list[str] = []
    symbols = read_symbols(binary)
    data = binary.read_bytes()

    entries = ENTRY.findall(source.read_text(encoding="utf-8"))

    if not entries:
        return [f"{source.name}: no entries found"]

    checked_symbols: set[str] = set()
    checked_strings: set[str] = set()

    for name, search_type in entries:
        if search_type not in SUPPORTED_TYPES:
            failures.append(f"{source.name}: {name} uses unsupported SearchType::{search_type}")
            continue

        if search_type == "SymbolOrPattern":
            if name in checked_symbols:
                continue

            checked_symbols.add(name)

            if name not in symbols:
                failures.append(f"{source.name}: symbol not found in {binary.name}: {name}")

            continue

        if name in checked_strings:
            continue

        checked_strings.add(name)

        occurrences = data.count(name.encode("utf-8"))

        if occurrences != 1:
            failures.append(
                f"{source.name}: string occurs {occurrences} times in {binary.name}: {name!r}"
            )

    print(
        f"{source.name}: {len(checked_symbols)} symbols, {len(checked_strings)} strings "
        f"checked against {binary.name}"
    )

    return failures


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--game-dir",
        required=True,
        type=Path,
        help="Half-Life installation that contains hw.so and cstrike/cl_dlls/client.so",
    )
    arguments = parser.parse_args()

    failures: list[str] = []

    for relative, source in PROVIDERS:
        binary = arguments.game_dir / relative

        if not binary.is_file():
            failures.append(f"missing binary: {binary}")
            continue

        failures.extend(check_provider(binary, source))

    if failures:
        for failure in failures:
            print(f"FAIL: {failure}")

        return 1

    print("All address provider entries resolve against the retail binaries.")

    return 0


if __name__ == "__main__":
    sys.exit(main())
