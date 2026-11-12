#!/usr/bin/env python3
"""Copy the Counter-Strike 1.6 files the client mounts.

Searches a Steam library for App 10 and copies an allowlist into a directory
outside that library. A missing Steam install or a missing game is a failure.
The tool does not invent a path, does not start Steam and does not start the
game.

Tests pass an explicit library root. They do not read the machine Steam
install.
"""

from __future__ import annotations

import argparse
import os
import re
import shutil
import stat
import sys
import tempfile
from pathlib import Path

# Directories the mounted client reads. The Half-Life layout stays intact:
# cstrike, valve and platform/resource are separate trees with colliding names.
CONTENT_PREFIXES = (
    "cstrike/maps/",
    "cstrike/models/",
    "cstrike/sound/",
    "cstrike/sprites/",
    "cstrike/gfx/",
    "cstrike/resource/",
    "cstrike/events/",
    "cstrike/overviews/",
    "cstrike/classes/",
    "cstrike/logos/",
    "valve/maps/",
    "valve/models/",
    "valve/sound/",
    "valve/sprites/",
    "valve/gfx/",
    "valve/resource/",
    "valve/events/",
    "valve/overviews/",
    "valve/media/",
    "valve/scripts/",
    "valve/logos/",
    "platform/resource/",
)

# Stock files at the game-directory root. Personal configs are not in this set.
ROOT_FILES = frozenset(
    {
        "cstrike/liblist.gam",
        "cstrike/mapcycle.txt",
        "cstrike/titles.txt",
        "cstrike/motd.txt",
        "cstrike/commandmenu.txt",
        "cstrike/spectatormenu.txt",
        "cstrike/spectcammenu.txt",
        "cstrike/autobuy.txt",
        "cstrike/rebuy.txt",
        "cstrike/delta.lst",
        "cstrike/settings.scr",
        "cstrike/user.scr",
        "valve/liblist.gam",
        "valve/titles.txt",
        "valve/credits.txt",
        "valve/default.cfg",
        "valve/valve.rc",
        "valve/delta.lst",
        "valve/settings.scr",
        "valve/user.scr",
        "valve/spectatormenu.txt",
        "valve/spectcammenu.txt",
        "valve/mapcycle.txt",
        "valve/game.cfg",
    }
)

# A stock tree without these is not a mountable game. The copy fails closed.
REQUIRED_FILES = (
    "cstrike/liblist.gam",
    "cstrike/delta.lst",
    "valve/liblist.gam",
    "valve/resource/marlett.ttf",
    "valve/resource/BackgroundLayout.txt",
)
REQUIRED_PREFIXES = (
    "cstrike/maps/",
    "cstrike/models/",
    "cstrike/sound/",
    "cstrike/sprites/",
    "cstrike/gfx/",
    "cstrike/resource/",
    "cstrike/events/",
    "cstrike/overviews/",
    "cstrike/classes/",
    "valve/models/",
    "valve/sound/",
    "valve/resource/",
    "platform/resource/",
)

PERSONAL_NAMES = frozenset(
    {
        "config.cfg",
        "userconfig.cfg",
        "autoexec.cfg",
        "video.cfg",
        "opengl.cfg",
        "joystick.cfg",
        "listenserver.cfg",
        "server.cfg",
        "language.cfg",
        "violence.cfg",
        "skill.cfg",
        "voice_ban.dt",
        "custom.hpk",
        "steam_appid.txt",
        "steam.inf",
    }
)
CACHE_NAMES = frozenset({"cached.wad", "tempdecal.wad"})
EXCLUDED_PARTS = frozenset(
    {
        "cl_dlls",
        "dlls",
        "screenshots",
        "logs",
        "save",
        "htmlcache",
        "downloads",
        "cstrike_downloads",
        "cstrike_hd",
        "redist",
        "penemy",
    }
)
BINARY_SUFFIXES = frozenset({".so", ".dll", ".exe", ".dylib"})
DEMO_SUFFIXES = frozenset({".dem"})
CACHE_SUFFIXES = frozenset({".cache"})

_TOKEN = re.compile(r'\s+|//[^\n]*|"((?:[^"\\]|\\.)*)"|([{}])')


def default_steam_roots() -> list[Path]:
    """Linux Steam client locations. Tests do not call this."""
    home = Path.home()
    return [
        home / ".steam" / "steam",
        home / ".local" / "share" / "Steam",
        home / ".var" / "app" / "com.valvesoftware.Steam" / ".local" / "share" / "Steam",
    ]


def parse_vdf(text: str) -> dict:
    """Parse a Steam KeyValues object. Duplicate keys and broken braces fail."""
    tokens: list[str] = []
    offset = 0
    while offset < len(text):
        found = _TOKEN.match(text, offset)
        if not found:
            raise ValueError("invalid VDF token")
        offset = found.end()
        if found.group(1) is not None:
            tokens.append(re.sub(r'\\(["\\])', r"\1", found.group(1)))
        elif found.group(2):
            tokens.append(found.group(2))
    cursor = 0

    def object_value(nested: bool, depth: int) -> dict:
        nonlocal cursor
        if depth > 32:
            raise ValueError("VDF nested too deep")
        result: dict = {}
        while cursor < len(tokens):
            key = tokens[cursor]
            cursor += 1
            if key == "}" and nested:
                return result
            if key in ("{", "}") or cursor >= len(tokens) or key in result:
                raise ValueError("invalid VDF key")
            value = tokens[cursor]
            cursor += 1
            if value == "{":
                value = object_value(True, depth + 1)
            elif value == "}":
                raise ValueError("invalid VDF value")
            result[key] = value
        if nested:
            raise ValueError("unclosed VDF object")
        return result

    return object_value(False, 0)


def _result(
    ok: bool,
    error: str | None,
    source: str | None = None,
    library: str | None = None,
    destination: str | None = None,
    copied: list[str] | None = None,
    skipped: list[dict[str, str]] | None = None,
    missing: list[str] | None = None,
) -> dict:
    return {
        "ok": ok,
        "error": error,
        "source": source,
        "library": library,
        "destination": destination,
        "copied": copied or [],
        "skipped": skipped or [],
        "missing": missing or [],
    }


def _resolved(path: Path) -> Path:
    return Path(os.path.realpath(path))


def _within(path: Path, parent: Path) -> bool:
    try:
        path.relative_to(parent)
    except ValueError:
        return False
    return True


def _library_section(document: dict) -> dict | None:
    for key, value in document.items():
        if key.casefold() == "libraryfolders" and isinstance(value, dict):
            return value
    return None


def _library_paths(section: dict) -> list[Path]:
    paths: list[Path] = []
    for key, entry in section.items():
        if isinstance(entry, dict) and isinstance(entry.get("path"), str) and entry["path"]:
            paths.append(Path(entry["path"]))
        elif key.isdigit() and isinstance(entry, str) and entry:
            paths.append(Path(entry))
    return paths


def _install_dir(value: object) -> str:
    if not isinstance(value, str) or not value or value.startswith(("/", "\\")) or ":" in value:
        raise ValueError("installdir is not a relative path")
    parts = value.replace("\\", "/").split("/")
    if any(part in ("", ".", "..") for part in parts):
        raise ValueError("installdir is not a relative path")
    return "/".join(parts)


def _read_manifest(path: Path) -> tuple[str, str] | None:
    """Return (installdir, buildid) for App 10, or None when this is another app."""
    document = parse_vdf(path.read_text(encoding="utf-8-sig"))
    state = document.get("AppState")
    if not isinstance(state, dict):
        raise ValueError("manifest has no AppState")
    if state.get("appid") != "10":
        return None
    return _install_dir(state.get("installdir")), str(state.get("buildid", ""))


def _candidate(library: Path) -> dict | None:
    manifest = library / "steamapps" / "appmanifest_10.acf"
    if not manifest.is_file():
        return None
    parsed = _read_manifest(manifest)
    if parsed is None:
        return None
    installdir, buildid = parsed
    common = _resolved(library / "steamapps" / "common")
    source = _resolved(common / installdir)
    if not _within(source, common):
        return None
    if not (source / "cstrike").is_dir() or not (source / "valve").is_dir():
        return None
    return {
        "library": _resolved(library),
        "source": source,
        "buildid": buildid,
        "manifest": manifest,
    }


def discover(roots: list[Path]) -> dict:
    """Find the first App 10 content root under the given Steam roots.

    `roots` is the only search list. This function does not read the home
    directory.
    """
    if not roots:
        return _result(False, "steam_missing")

    libraries: list[Path] = []
    seen: set[Path] = set()
    found_layout = False

    def add_library(path: Path) -> None:
        if not path.is_dir():
            return
        resolved = _resolved(path)
        if resolved in seen:
            return
        seen.add(resolved)
        libraries.append(resolved)

    for root in roots:
        if not root.is_dir():
            continue
        vdf_path = root / "steamapps" / "libraryfolders.vdf"
        has_vdf = vdf_path.is_file()
        has_apps = (root / "steamapps").is_dir()
        if not has_vdf and not has_apps:
            continue
        found_layout = True
        add_library(root)
        if not has_vdf:
            continue
        try:
            document = parse_vdf(vdf_path.read_text(encoding="utf-8-sig"))
        except (OSError, ValueError):
            return _result(False, "steam_metadata_invalid")
        section = _library_section(document)
        if section is None:
            return _result(False, "steam_metadata_invalid")
        for extra in _library_paths(section):
            add_library(extra)

    if not found_layout:
        return _result(False, "steam_missing")

    for library in libraries:
        manifest = library / "steamapps" / "appmanifest_10.acf"
        if not manifest.is_file():
            continue
        try:
            found = _candidate(library)
        except (OSError, ValueError):
            return _result(False, "steam_metadata_invalid")
        if found is None:
            continue
        return _result(
            True,
            None,
            source=str(found["source"]),
            library=str(found["library"]),
        )

    return _result(False, "game_missing")


def decision(relative: str) -> str:
    """Return 'copy' or the reason the file stays in the Steam tree."""
    parts = relative.split("/")
    if any(part in ("", ".", "..") for part in parts):
        return "not_content"
    name = parts[-1]
    folded = name.casefold()
    dot = folded.rfind(".")
    suffix = folded[dot:] if dot >= 0 else ""
    if folded in PERSONAL_NAMES or relative.casefold().startswith("platform/config/"):
        return "personal"
    if suffix in DEMO_SUFFIXES:
        return "demo"
    if folded in CACHE_NAMES or suffix in CACHE_SUFFIXES:
        return "cache"
    if suffix in BINARY_SUFFIXES or folded.startswith(("libsteam", "steamclient")):
        return "binary"
    if any(part in EXCLUDED_PARTS for part in parts):
        return "not_content"
    if relative in ROOT_FILES or _root_wad(relative) or relative.startswith(CONTENT_PREFIXES):
        return "copy"
    return "not_content"


def _root_wad(relative: str) -> bool:
    parts = relative.split("/")
    if len(parts) != 2 or parts[0] not in ("cstrike", "valve"):
        return False
    name = parts[1]
    return name.casefold().endswith(".wad") and name.casefold() not in CACHE_NAMES


def _missing_required(selected: set[str]) -> list[str]:
    missing = [name for name in REQUIRED_FILES if name not in selected]
    for prefix in REQUIRED_PREFIXES:
        if not any(name.startswith(prefix) for name in selected):
            missing.append(prefix)
    if not any(_root_wad(name) and name.startswith("cstrike/") for name in selected):
        missing.append("cstrike/*.wad")
    return missing


def _scan(source: Path) -> tuple[list[str], list[dict[str, str]]]:
    selected: list[str] = []
    skipped: list[dict[str, str]] = []
    source_resolved = _resolved(source)
    for directory, dirnames, filenames in os.walk(source, followlinks=False):
        dirnames.sort()
        current = Path(directory)
        kept_dirs = []
        for dirname in dirnames:
            path = current / dirname
            if stat.S_ISLNK(path.lstat().st_mode):
                skipped.append({"path": path.relative_to(source).as_posix(), "reason": "symlink"})
                continue
            kept_dirs.append(dirname)
        dirnames[:] = kept_dirs
        for filename in sorted(filenames):
            path = current / filename
            relative = path.relative_to(source).as_posix()
            info = path.lstat()
            if stat.S_ISLNK(info.st_mode):
                skipped.append({"path": relative, "reason": "symlink"})
                continue
            if not stat.S_ISREG(info.st_mode):
                skipped.append({"path": relative, "reason": "special"})
                continue
            if not _within(_resolved(path), source_resolved):
                skipped.append({"path": relative, "reason": "outside"})
                continue
            reason = decision(relative)
            if reason == "copy":
                selected.append(relative)
            else:
                skipped.append({"path": relative, "reason": reason})
    selected.sort()
    skipped.sort(key=lambda item: item["path"])
    return selected, skipped


def _destination_error(destination: Path, library: Path, source: Path) -> str | None:
    dest = _resolved(destination)
    if dest == Path(dest.anchor):
        return "destination_unavailable"
    if _within(dest, library) or _within(dest, source):
        return "destination_inside_source"
    if destination.exists() and not destination.is_dir():
        return "destination_unavailable"
    if destination.is_dir() and any(destination.iterdir()):
        return "destination_not_empty"
    parent = destination.parent
    if not parent.is_dir() and destination.exists():
        return "destination_unavailable"
    return None


def install_content(roots: list[Path], destination: Path) -> dict:
    """Detect App 10 under `roots` and copy the allowlist to `destination`."""
    found = discover(roots)
    if not found["ok"]:
        return found

    source = Path(found["source"])
    library = Path(found["library"])
    destination = _resolved(destination)
    refused = _destination_error(destination, library, source)
    if refused is not None:
        return _result(
            False,
            refused,
            source=found["source"],
            library=found["library"],
            destination=str(destination),
        )

    selected, skipped = _scan(source)
    missing = _missing_required(set(selected))
    if missing:
        return _result(
            False,
            "game_missing",
            source=found["source"],
            library=found["library"],
            skipped=skipped,
            missing=missing,
        )

    parent = destination.parent
    parent.mkdir(parents=True, exist_ok=True)
    if _within(_resolved(parent), library):
        return _result(
            False,
            "destination_inside_source",
            source=found["source"],
            library=found["library"],
            destination=str(destination),
        )
    staging = Path(tempfile.mkdtemp(prefix=".csretro-install-", dir=str(parent)))
    try:
        for relative in selected:
            target = staging / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source / relative, target)
        arrived = sorted(
            path.relative_to(staging).as_posix() for path in staging.rglob("*") if path.is_file()
        )
        if arrived != selected:
            raise RuntimeError("destination does not match the allowlist")
        if destination.exists():
            destination.rmdir()
        staging.rename(destination)
    except Exception:
        shutil.rmtree(staging, ignore_errors=True)
        raise
    finally:
        if staging.exists():
            shutil.rmtree(staging, ignore_errors=True)

    return _result(
        True,
        None,
        source=found["source"],
        library=found["library"],
        destination=str(_resolved(destination)),
        copied=selected,
        skipped=skipped,
    )


def _parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Copy mounted CS 1.6 content outside Steam.")
    parser.add_argument(
        "--steam-root",
        action="append",
        default=None,
        help="Steam client or library root. Repeatable. Defaults to the Linux Steam locations.",
    )
    parser.add_argument("--destination", required=True, help="Directory outside the Steam library.")
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = _parse_args(sys.argv[1:] if argv is None else argv)
    roots = [Path(value) for value in args.steam_root] if args.steam_root else default_steam_roots()
    try:
        result = install_content(roots, Path(args.destination))
    except OSError as error:
        print(f"error: copy_failed: {error}", file=sys.stderr)
        return 1
    if not result["ok"]:
        print(f"error: {result['error']}", file=sys.stderr)
        for name in result["missing"]:
            print(f"missing: {name}", file=sys.stderr)
        return 1
    print(f"ok copied={len(result['copied'])}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
