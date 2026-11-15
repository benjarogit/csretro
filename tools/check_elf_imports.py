#!/usr/bin/env python3
"""Screen strong ELF imports against an explicit provider graph without loading it.

This is not a loader, ABI or gameplay certificate. Providers are assumed to be
loaded together; DT_NEEDED, search paths, relocation and load order are not proven.
"""
from __future__ import annotations

import argparse
import json
import os
import struct
import subprocess
from pathlib import Path


def _inspect(path: Path) -> tuple[tuple[int, int, int], list[dict]]:
    with path.open("rb") as stream:
        header = stream.read(64)
    if len(header) < 52 or header[:4] != b"\x7fELF":
        raise ValueError("missing or truncated ELF header")
    elf_class, encoding, version = header[4:7]
    if elf_class not in (1, 2) or encoding not in (1, 2) or version != 1:
        raise ValueError("unsupported ELF class, encoding or version")
    if elf_class == 2 and len(header) < 64:
        raise ValueError("truncated ELF64 header")
    order = "<" if encoding == 1 else ">"
    kind, machine = struct.unpack_from(order + "HH", header, 16)
    if struct.unpack_from(order + "I", header, 20)[0] != 1:
        raise ValueError("unsupported ELF header version")
    expected_size = 64 if elf_class == 2 else 52
    if struct.unpack_from(order + "H", header, 52 if elf_class == 2 else 40)[0] != expected_size:
        raise ValueError("invalid ELF header size")
    if kind not in (2, 3) or not machine:
        raise ValueError("expected an ELF executable or shared object")
    output = subprocess.run(
        ["readelf", "--dyn-syms", "--wide", "--", str(path)],
        capture_output=True, text=True, check=False, timeout=30,
        env={**os.environ, "LC_ALL": "C"},
    )
    if output.returncode:
        raise ValueError("readelf failed: " + output.stderr.strip())
    symbols = []
    for line in output.stdout.splitlines():
        fields = line.split()
        if not fields or not fields[0].endswith(":") or not fields[0][:-1].isdigit():
            continue
        if len(fields) < 7:
            raise ValueError("malformed dynamic symbol row")
        # The zero/null symbol has no name. Version indexes follow the name.
        if len(fields) == 7:
            if fields[4] == "GLOBAL" and fields[6] == "UND":
                raise ValueError("unnamed strong import")
            continue
        if len(fields) > 8 and not (len(fields) == 9 and fields[8].startswith("(")
                                    and fields[8].endswith(")") and fields[8][1:-1].isdigit()):
            raise ValueError("ambiguous dynamic symbol name")
        name = fields[7]
        base, separator, suffix = name.partition("@")
        default = suffix.startswith("@")
        version_name = suffix[1:] if default else suffix
        if not base or (separator and not version_name):
            raise ValueError("malformed versioned symbol name")
        symbols.append({"name": name, "base": base, "version": version_name,
                        "default": default, "type": fields[3], "bind": fields[4],
                        "visibility": fields[5], "index": fields[6]})
    if not symbols and "Symbol table" not in output.stdout:
        raise ValueError("no dynamic symbol table")
    return (elf_class, encoding, machine), symbols


def inspect_imports(consumer: Path, providers: list[Path]) -> dict:
    """Return a conservative symbol screen; never execute inspected artifacts."""
    result = {"symbol_screen": "fail", "abi": "unverified", "playability": "unverified",
              "provider_graph": "explicit, assumed loaded; not DT_NEEDED resolution",
              "missing_imports": [], "type_mismatches": [], "provider_errors": [], "errors": []}
    try:
        architecture, consumer_symbols = _inspect(consumer)
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        result["errors"].append(f"consumer: {error}")
        return result
    definitions: dict[str, list[tuple[str, dict]]] = {}

    def add_definitions(path: Path, symbols: list[dict]) -> None:
        for symbol in symbols:
            if (symbol["index"] != "UND" and symbol["bind"] in {"GLOBAL", "WEAK", "UNIQUE"}
                    and symbol["visibility"] in {"DEFAULT", "PROTECTED"}):
                definitions.setdefault(symbol["base"], []).append((str(path), symbol))

    add_definitions(consumer, consumer_symbols)
    for provider in providers:
        try:
            provider_architecture, symbols = _inspect(provider)
            if provider_architecture != architecture:
                raise ValueError("ELF class, encoding or machine differs from consumer")
            add_definitions(provider, symbols)
        except (OSError, ValueError, subprocess.SubprocessError) as error:
            result["provider_errors"].append({"provider": str(provider), "error": str(error)})
    required = [symbol for symbol in consumer_symbols if symbol["index"] == "UND" and symbol["bind"] == "GLOBAL"]
    for imported in required:
        candidates = [(path, exported) for path, exported in definitions.get(imported["base"], [])
                      if
                      (imported["version"] == exported["version"] if imported["version"] else
                       not exported["version"] or exported["default"])]
        if not candidates:
            result["missing_imports"].append(imported["name"])
            continue
        path, exported = candidates[0]
        kinds = {"TLS": "tls", "OBJECT": "object", "FUNC": "function", "IFUNC": "function", "GNU_IFUNC": "function"}
        required_kind = kinds.get(imported["type"])
        provided_kind = kinds.get(exported["type"])
        if required_kind and provided_kind and required_kind != provided_kind:
            result["type_mismatches"].append({"symbol": imported["name"], "required_type": imported["type"],
                                               "provider": path, "provided_type": exported["type"]})
    result["missing_imports"] = sorted(set(result["missing_imports"]))
    result["required_import_count"] = len(required)
    if not any(result[key] for key in ("errors", "provider_errors", "missing_imports", "type_mismatches")):
        result["symbol_screen"] = "pass"
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("consumer", type=Path)
    parser.add_argument("--provider", type=Path, action="append", default=[])
    parser.add_argument("--json", action="store_true", help="emit JSON (also the default)")
    args = parser.parse_args()
    result = inspect_imports(args.consumer, args.provider)
    print(json.dumps(result, indent=2))
    return 0 if result["symbol_screen"] == "pass" else 1


if __name__ == "__main__":
    raise SystemExit(main())
