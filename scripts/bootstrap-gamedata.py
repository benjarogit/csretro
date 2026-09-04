#!/usr/bin/env python3
"""CS-Retro Game-Data-Bootstrap.

Findet Steam CS 1.6 (AppID 10) über lokale VDF-Dateien, kopiert nur das
Manifest in einen eigenen Datenbaum und verändert die Steam-Installation nie.

Xash startet danach mit XASH3D_RODIR = diesem Baum, nicht Steam-Half-Life.
"""
from __future__ import annotations

import argparse
import fnmatch
import hashlib
import json
import os
import shutil
import sys
import urllib.request
import zipfile
from datetime import datetime, timezone
from pathlib import Path

APPID = 10
INSTALLDIR_DEFAULT = "Half-Life"
ORIGIN_NAME = ".csretro-origin.json"
ZBOT_ZIP_URL = (
    "https://github.com/rehlds/ReGameDLL_CS/raw/master/regamedll/extra/zBot/bot_profiles.zip"
)
NAV_URL_TMPL = (
    "https://raw.githubusercontent.com/MysticDeathProject/Fixed-CSbot-Navigation/"
    "main/navigations/{map}.nav"
)

STEAM_BINARY_SUFFIXES = {".so", ".dll", ".exe", ".dylib"}
STEAM_BINARY_NAMES = {
    "hl_linux",
    "hl.sh",
    "hl.conf",
    "hl.exe",
    "hw.so",
    "sw.so",
    "steam_api.dll",
    "libsteam_api.so",
    "libsteam_api.dylib",
}


def repo_root() -> Path:
    return Path(__file__).resolve().parent.parent


def load_manifest() -> dict:
    path = repo_root() / "data" / "gamedata-manifest.json"
    return json.loads(path.read_text(encoding="utf-8"))


def now_iso() -> str:
    return datetime.now(timezone.utc).replace(microsecond=0).isoformat()


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


# --- Steam roots (no SteamAPI) ------------------------------------------------

def _win_steam_from_registry() -> list[Path]:
    if sys.platform != "win32":
        return []
    found: list[Path] = []
    try:
        import winreg  # type: ignore
    except ImportError:
        return []
    keys = (
        (winreg.HKEY_CURRENT_USER, r"Software\Valve\Steam"),
        (winreg.HKEY_LOCAL_MACHINE, r"SOFTWARE\WOW6432Node\Valve\Steam"),
        (winreg.HKEY_LOCAL_MACHINE, r"SOFTWARE\Valve\Steam"),
    )
    for hive, sub in keys:
        try:
            with winreg.OpenKey(hive, sub) as key:
                val, _ = winreg.QueryValueEx(key, "SteamPath")
        except OSError:
            continue
        p = Path(str(val))
        if p.is_dir():
            found.append(p)
    return found


def candidate_steam_roots() -> list[Path]:
    roots: list[Path] = []
    env_keys = ("CSRETRO_STEAM_ROOT", "STEAM_DIR", "STEAMROOT", "STEAM_PATH")
    for key in env_keys:
        val = os.environ.get(key)
        if val:
            roots.append(Path(val).expanduser())

    home = Path.home()
    if sys.platform.startswith("linux"):
        roots.extend(
            [
                home / ".steam" / "steam",
                home / ".steam" / "root",
                home / ".local" / "share" / "Steam",
                home / ".var" / "app" / "com.valvesoftware.Steam" / ".local" / "share" / "Steam",
            ]
        )
    elif sys.platform == "darwin":
        roots.append(home / "Library" / "Application Support" / "Steam")
    elif sys.platform == "win32":
        roots.extend(_win_steam_from_registry())
        pf = os.environ.get("ProgramFiles(x86)") or os.environ.get("ProgramFiles")
        if pf:
            roots.append(Path(pf) / "Steam")
        pd = os.environ.get("ProgramFiles")
        if pd:
            roots.append(Path(pd) / "Steam")

    out: list[Path] = []
    seen: set[Path] = set()
    for raw in roots:
        try:
            p = raw.resolve()
        except OSError:
            continue
        if p in seen or not p.is_dir():
            continue
        seen.add(p)
        out.append(p)
    return out


def libraryfolders_vdflist(steam_root: Path) -> list[Path]:
    return [
        steam_root / "steamapps" / "libraryfolders.vdf",
        steam_root / "config" / "libraryfolders.vdf",
    ]


def parse_vdf_paths(text: str) -> list[str]:
    """Extract string values of keys named path from a Steam VDF blob."""
    paths: list[str] = []
    key = None
    token = ""
    in_str = False
    i = 0
    while i < len(text):
        ch = text[i]
        if in_str:
            if ch == "\\" and i + 1 < len(text):
                token += text[i + 1]
                i += 2
                continue
            if ch == '"':
                in_str = False
                if key == "path":
                    paths.append(token)
                    key = None
                else:
                    key = token.lower() if token.lower() == "path" else None
                token = ""
            else:
                token += ch
            i += 1
            continue
        if ch == '"':
            in_str = True
            token = ""
        i += 1
    return paths


def steam_libraries(steam_root: Path) -> list[Path]:
    libs = [steam_root]
    for vdf in libraryfolders_vdflist(steam_root):
        if not vdf.is_file():
            continue
        for raw in parse_vdf_paths(vdf.read_text(encoding="utf-8", errors="replace")):
            p = Path(raw)
            if p.is_dir():
                libs.append(p)
    out: list[Path] = []
    seen: set[Path] = set()
    for raw in libs:
        try:
            p = raw.resolve()
        except OSError:
            continue
        if p in seen:
            continue
        seen.add(p)
        out.append(p)
    return out


def parse_acf_kv(text: str) -> dict[str, str]:
    wanted = {"appid", "installdir", "name", "lastupdated", "buildid", "stateflags"}
    out: dict[str, str] = {}
    tokens: list[str] = []
    token = ""
    in_str = False
    i = 0
    while i < len(text):
        ch = text[i]
        if in_str:
            if ch == "\\" and i + 1 < len(text):
                token += text[i + 1]
                i += 2
                continue
            if ch == '"':
                in_str = False
                tokens.append(token)
                token = ""
            else:
                token += ch
            i += 1
            continue
        if ch == '"':
            in_str = True
            token = ""
        i += 1
    for a, b in zip(tokens, tokens[1:]):
        if a.lower() in wanted and a.lower() not in out:
            out[a.lower()] = b
    return out


def find_cs16() -> dict:
    errors: list[str] = []
    roots = candidate_steam_roots()
    if not roots:
        raise SystemExit(
            "Steam-Root nicht gefunden. Counter-Strike 1.6 (AppID 10) muss lokal "
            "über Steam installiert sein. Optional: CSRETRO_STEAM_ROOT setzen."
        )
    for root in roots:
        for lib in steam_libraries(root):
            acf = lib / "steamapps" / f"appmanifest_{APPID}.acf"
            if not acf.is_file():
                continue
            kv = parse_acf_kv(acf.read_text(encoding="utf-8", errors="replace"))
            installdir = kv.get("installdir") or INSTALLDIR_DEFAULT
            source = lib / "steamapps" / "common" / installdir
            valve = source / "valve"
            cstrike = source / "cstrike"
            if not valve.is_dir() or not cstrike.is_dir():
                errors.append(
                    f"{source} hat appmanifest_{APPID}.acf, aber valve/ oder cstrike/ fehlt."
                )
                continue
            return {
                "steam_root": str(root),
                "library": str(lib),
                "acf": str(acf),
                "source": str(source),
                "installdir": installdir,
                "acf_meta": kv,
                "acf_sha256": sha256_file(acf),
                "acf_mtime": int(acf.stat().st_mtime),
            }
    msg = [
        "Counter-Strike 1.6 (Steam AppID 10, InstallDir Half-Life) nicht gefunden.",
        "Geprüfte Steam-Roots:",
        *[f"  - {r}" for r in roots],
    ]
    if errors:
        msg.append("Hinweise:")
        msg.extend(f"  - {e}" for e in errors)
    msg.append("Bitte CS 1.6 in Steam installieren. Steam selbst wird danach nicht zur Runtime.")
    raise SystemExit("\n".join(msg))


# --- Game-data location -------------------------------------------------------

def default_user_gamedata() -> Path:
    if sys.platform == "win32":
        base = os.environ.get("LOCALAPPDATA") or str(Path.home() / "AppData" / "Local")
        return Path(base) / "CS Retro" / "gamedata"
    if sys.platform == "darwin":
        return Path.home() / "Library" / "Application Support" / "CS Retro" / "gamedata"
    xdg = os.environ.get("XDG_DATA_HOME")
    if xdg:
        return Path(xdg) / "csretro" / "gamedata"
    return Path.home() / ".local" / "share" / "csretro" / "gamedata"


def gamedata_dir(explicit: str | None = None) -> Path:
    if explicit:
        return Path(explicit).expanduser().resolve()
    env = os.environ.get("CSRETRO_GAMEDATA")
    if env:
        return Path(env).expanduser().resolve()
    root = repo_root()
    if (root / ".git").exists() or (root / "scripts" / "bootstrap-gamedata.py").is_file():
        return (root / "gamedata").resolve()
    return default_user_gamedata().resolve()


def default_user_dir() -> Path:
    if sys.platform == "win32":
        base = os.environ.get("LOCALAPPDATA") or str(Path.home() / "AppData" / "Local")
        return Path(base) / "CS Retro" / "user"
    if sys.platform == "darwin":
        return Path.home() / "Library" / "Application Support" / "CS Retro" / "user"
    xdg = os.environ.get("XDG_DATA_HOME")
    if xdg:
        return Path(xdg) / "csretro" / "user"
    return Path.home() / ".local" / "share" / "csretro" / "user"


# --- Manifest matching --------------------------------------------------------

def match_glob(rel: str, pattern: str) -> bool:
    rel_f = rel.replace("\\", "/")
    pat = pattern.replace("\\", "/")
    if "**" in pat:
        return fnmatch.fnmatch(rel_f, pat) or fnmatch.fnmatch(Path(rel_f).name, pat)
    return fnmatch.fnmatch(rel_f, pat) or fnmatch.fnmatch(Path(rel_f).name, pat)


def is_ignored(rel: str, manifest: dict) -> bool:
    rel_f = rel.replace("\\", "/")
    name = Path(rel_f).name
    if name in STEAM_BINARY_NAMES:
        return True
    if Path(rel_f).suffix.lower() in STEAM_BINARY_SUFFIXES:
        return True
    for rule in manifest.get("ignore", []):
        if match_glob(rel_f, rule["glob"]):
            return True
    return False


def is_user_protect(rel: str, manifest: dict) -> bool:
    name = Path(rel.replace("\\", "/")).name
    return name in set(manifest.get("user_protect", []))


def iter_source_files(source: Path, rel_root: str) -> list[Path]:
    base = source / rel_root
    if base.is_file():
        return [base]
    if not base.is_dir():
        return []
    return [p for p in base.rglob("*") if p.is_file()]


def expand_glob(source: Path, pattern: str) -> list[Path]:
    rel = pattern.replace("\\", "/")
    if any(ch in rel for ch in "*?["):
        parent, name = rel.rsplit("/", 1) if "/" in rel else ("", rel)
        root = source / parent if parent else source
        if not root.is_dir():
            return []
        return [p for p in root.iterdir() if p.is_file() and fnmatch.fnmatch(p.name, name)]
    p = source / rel
    return [p] if p.is_file() else []


def rel_to_source(source: Path, path: Path) -> str:
    return path.resolve().relative_to(source.resolve()).as_posix()


def assert_copy_bounds(source: Path, dest_root: Path, src: Path, dest: Path) -> None:
    src_r = src.resolve()
    dest_r = dest.resolve()
    source_r = source.resolve()
    dest_root_r = dest_root.resolve()
    if not str(src_r).startswith(str(source_r) + os.sep) and src_r != source_r:
        raise RuntimeError(f"Quelle außerhalb der Steam-Installation: {src_r}")
    if not str(dest_r).startswith(str(dest_root_r) + os.sep) and dest_r != dest_root_r:
        raise RuntimeError(f"Ziel außerhalb des CS-Retro-Datenbaums: {dest_r}")
    if str(dest_r).startswith(str(source_r) + os.sep):
        raise RuntimeError("Schreibversuch in die Steam-Installation — abgebrochen.")


def copy_file(source: Path, dest_root: Path, src: Path) -> str:
    rel = rel_to_source(source, src)
    dest = dest_root / rel
    dest.parent.mkdir(parents=True, exist_ok=True)
    assert_copy_bounds(source, dest_root, src, dest)
    shutil.copy2(src, dest)
    return rel


# --- Import -------------------------------------------------------------------

def collect_copy_rels(source: Path, manifest: dict) -> list[str]:
    seen: set[str] = set()
    for group in manifest.get("copy_groups", []):
        for path in iter_source_files(source, group["root"]):
            rel = rel_to_source(source, path)
            if not is_ignored(rel, manifest) and not is_user_protect(rel, manifest):
                seen.add(rel)
    for rule in manifest.get("copy_globs", []):
        for path in expand_glob(source, rule["glob"]):
            rel = rel_to_source(source, path)
            if not is_ignored(rel, manifest) and not is_user_protect(rel, manifest):
                seen.add(rel)
    return sorted(seen)


LIBLIST_GAM = """game "CS Retro"
url_info ""
url_dl ""
version "0.1"
size "0"
svonly "0"
secure "0"
type "multiplayer_only"
cldll "1"
hlversion "1111"
nomodels "1"
nohimodel "1"
mpentity "info_player_start"
gamedll "dlls/mp.dll"
gamedll_linux "dlls/cs.so"
gamedll_osx "dlls/cs.dylib"
trainmap "tr_1"
edicts\t"1800"
"""



def patch_tracker_scheme_menu_item_height(dest_root: Path) -> list[str]:
    """Ensure Menu { ItemHeight 20 } for classic ComboBox dropdowns."""
    import re

    patched: list[str] = []
    for rel in (
        "valve/resource/TrackerScheme.res",
        "cstrike/resource/TrackerScheme.res",
        "platform/resource/TrackerScheme.res",
    ):
        path = dest_root / rel
        if not path.is_file():
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        m = re.search(r"(Menu\s*\{)([\s\S]*?)(\n\t\t\})", text)
        if not m:
            continue
        body = m.group(2)
        if re.search(r'"ItemHeight"\s+"\d+"', body):
            continue
        # Insert after TextInset when present, else at end of Menu body
        if re.search(r'"TextInset"\s+"\d+"', body):
            body2 = re.sub(
                r'("TextInset"\s+"\d+")',
                r'\1\n\t\t\t"ItemHeight"\t\t\t"20"',
                body,
                count=1,
            )
        else:
            body2 = body + '\n\t\t\t"ItemHeight"\t\t\t"20"'
        text2 = text[: m.start()] + m.group(1) + body2 + m.group(3) + text[m.end() :]
        path.write_text(text2, encoding="utf-8")
        patched.append(rel)
    return patched


def prune_stale(dest_root: Path, manifest: dict) -> list[str]:
    """Entfernt Steam-Reste, die im Zielbaum nichts zu suchen haben.

    Die `ignore`-Regeln verhindern das Kopieren; `prune` räumt Bäume auf, die aus
    einem früheren Import stammen, als die Regel noch nicht existierte. Ohne das
    bliebe alter Ballast für immer liegen, weil ein Refresh nur überschreibt.
    Deklarativ im Manifest, damit neue Regeln keinen Codeanbau brauchen.

    Aufrufer müssen prune *vor* den UI-Overrides ausführen: ein prune-Pfad kann
    ein Verzeichnis sein, in das der Override danach die CS-Retro-Datei legt
    (Menühintergrund in cstrike/resource/background).
    """
    removed: list[str] = []
    for rule in manifest.get("prune", []):
        rel = rule["path"]
        target = dest_root / rel
        if target.is_dir():
            shutil.rmtree(target)
        elif target.is_file():
            target.unlink()
        else:
            continue
        removed.append(rel)
    return removed


def apply_ui_overrides(dest_root: Path) -> list[str]:
    src_root = repo_root() / "data" / "ui-overrides"
    if not src_root.is_dir():
        return []
    copied: list[str] = []
    for path in src_root.rglob("*"):
        if not path.is_file():
            continue
        rel = path.relative_to(src_root).as_posix()
        dest = dest_root / rel
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, dest)
        copied.append(rel)
    return copied


def ensure_platform_resource(source: Path, dest_root: Path, manifest: dict) -> list[str]:
    if (dest_root / "platform" / "resource" / "TrackerScheme.res").is_file():
        return []
    copied: list[str] = []
    for path in iter_source_files(source, "platform/resource"):
        rel = rel_to_source(source, path)
        if is_ignored(rel, manifest) or is_user_protect(rel, manifest):
            continue
        copied.append(copy_file(source, dest_root, path))
    return copied


def write_liblist(dest_root: Path) -> str:
    path = dest_root / "cstrike" / "liblist.gam"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(LIBLIST_GAM, encoding="utf-8")
    return "cstrike/liblist.gam"


def write_startup_rc(dest_root: Path) -> None:
    for rel in ("valve/valve.rc", "cstrike/cstrike.rc"):
        path = dest_root / rel
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("exec autoexec.cfg\nstuffcmds\n", encoding="utf-8")


def library_names() -> tuple[str, str]:
    if sys.platform == "win32":
        return "client_amd64.dll", "cs_amd64.dll"
    if sys.platform == "darwin":
        if os.uname().machine == "arm64":
            return "client_arm64.dylib", "cs_arm64.dylib"
        return "client_amd64.dylib", "cs_amd64.dylib"
    return "client_amd64.so", "cs_amd64.so"


def deploy_csretro_modules(dest_root: Path, client: Path | None, gamedll: Path | None) -> list[str]:
    client_name, dll_name = library_names()
    deployed: list[str] = []
    mapping = (
        (gamedll, dest_root / "cstrike" / "dlls" / dll_name),
        (client, dest_root / "cstrike" / "cl_dlls" / client_name),
    )
    for src, dest in mapping:
        if src is None or not src.is_file():
            continue
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dest)
        deployed.append(str(dest.relative_to(dest_root)))
    write_startup_rc(dest_root)
    deployed.append(write_liblist(dest_root))
    deployed.extend(["valve/valve.rc", "cstrike/cstrike.rc"])
    return deployed


def install_zbot_extras(dest_root: Path, map_name: str) -> list[str]:
    extras: list[str] = []
    cache = Path(os.environ.get("CSRETRO_ZBOT_ZIP", "/tmp/csretro-zbot/bot_profiles.zip"))
    cache.parent.mkdir(parents=True, exist_ok=True)
    if not cache.is_file():
        urllib.request.urlretrieve(ZBOT_ZIP_URL, cache)
    with zipfile.ZipFile(cache) as zf:
        for member in zf.namelist():
            name = Path(member).name
            if name in {"BotProfile.db", "BotChatter.db"}:
                dest = dest_root / "cstrike" / name
                dest.parent.mkdir(parents=True, exist_ok=True)
                dest.write_bytes(zf.read(member))
                extras.append(f"cstrike/{name}")
    nav = dest_root / "cstrike" / "maps" / f"{map_name}.nav"
    if not nav.is_file() or nav.stat().st_size == 0:
        nav.parent.mkdir(parents=True, exist_ok=True)
        urllib.request.urlretrieve(NAV_URL_TMPL.format(map=map_name), nav)
    if nav.is_file():
        extras.append(f"cstrike/maps/{map_name}.nav")
    return extras


def write_origin(dest_root: Path, payload: dict) -> None:
    (dest_root / ORIGIN_NAME).write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")


def read_origin(dest_root: Path) -> dict | None:
    path = dest_root / ORIGIN_NAME
    if not path.is_file():
        return None
    return json.loads(path.read_text(encoding="utf-8"))


def do_import(args: argparse.Namespace) -> int:
    manifest = load_manifest()
    dest_root = gamedata_dir(args.dest)
    cs = find_cs16()
    source = Path(cs["source"])
    dest_root.mkdir(parents=True, exist_ok=True)

    origin = read_origin(dest_root)
    refresh = bool(args.refresh)
    if origin and not refresh:
        same = origin.get("acf_sha256") == cs["acf_sha256"]
        if same and (dest_root / "cstrike" / "maps" / "de_dust.bsp").is_file():
            print(f"Game-Data aktuell ({dest_root}), Steam-ACF unverändert.")
            if not args.skip_modules:
                owned = deploy_csretro_modules(
                    dest_root,
                    Path(args.client) if args.client else None,
                    Path(args.gamedll) if args.gamedll else None,
                )
                if origin is not None:
                    origin["csretro_owned"] = sorted(set(origin.get("csretro_owned", []) + owned))
                    write_origin(dest_root, origin)
            if not args.skip_extras:
                install_zbot_extras(dest_root, args.map)
            ensure_platform_resource(source, dest_root, manifest)
            # prune vor Overrides: sonst löscht prune cstrike/resource/background
            # inklusive csretro.png, das der Override in denselben Ordner legt.
            prune_stale(dest_root, manifest)
            apply_ui_overrides(dest_root)
            patch_tracker_scheme_menu_item_height(dest_root)
            print(f"XASH3D_RODIR={dest_root}")
            return 0
        if not same:
            print("Steam AppID 10 hat sich geändert — nur Steam-sourced Daten werden erneuert.")
            refresh = True

    copied: list[str] = []
    skipped_ignore = 0
    for rel in collect_copy_rels(source, manifest):
        src = source / rel
        if not src.is_file():
            continue
        if is_ignored(rel, manifest):
            skipped_ignore += 1
            continue
        copied.append(copy_file(source, dest_root, src))

    extras = []
    if not args.skip_extras:
        extras = install_zbot_extras(dest_root, args.map)
    deployed = []
    if not args.skip_modules:
        root = repo_root()
        client = Path(args.client) if args.client else root / "build/client-cmake/client" / library_names()[0]
        gamedll = Path(args.gamedll) if args.gamedll else root / "build/gamedll-cmake" / library_names()[1]
        deployed = deploy_csretro_modules(
            dest_root,
            client if client.is_file() else None,
            gamedll if gamedll.is_file() else None,
        )

    copied.extend(ensure_platform_resource(source, dest_root, manifest))
    pruned = prune_stale(dest_root, manifest)
    copied.extend(apply_ui_overrides(dest_root))
    copied.extend(patch_tracker_scheme_menu_item_height(dest_root))

    write_origin(
        dest_root,
        {
            "manifest_version": manifest["version"],
            "imported_at": now_iso(),
            "refresh": refresh,
            "steam_root": cs["steam_root"],
            "library": cs["library"],
            "source": cs["source"],
            "acf": cs["acf"],
            "acf_sha256": cs["acf_sha256"],
            "acf_mtime": cs["acf_mtime"],
            "acf_meta": cs["acf_meta"],
            "copied": len(copied),
            "ignored_skipped": skipped_ignore,
            "pruned": pruned,
            "steam_sourced": copied,
            "csretro_owned": extras + deployed,
        },
    )
    print(f"Import {len(copied)} Dateien → {dest_root}")
    if pruned:
        print(f"Entfernt (Steam-Reste): {', '.join(pruned)}")
    print(f"Steam-Quelle (read-only): {source}")
    print(f"XASH3D_RODIR={dest_root}")
    return 0


def do_status(args: argparse.Namespace) -> int:
    dest_root = gamedata_dir(args.dest)
    origin = read_origin(dest_root)
    print(f"Game-Data: {dest_root}")
    print(f"vorhanden: {(dest_root / 'cstrike' / 'maps' / 'de_dust.bsp').is_file()}")
    if origin:
        print(f"Quelle: {origin.get('source')}")
        print(f"ACF:    {origin.get('acf')} sha256={origin.get('acf_sha256', '')[:12]}…")
        print(f"Import: {origin.get('imported_at')}  Dateien={origin.get('copied')}")
    try:
        cs = find_cs16()
        print(f"Steam jetzt: {cs['source']}")
        if origin and origin.get("acf_sha256") != cs["acf_sha256"]:
            print("Steam-Stand hat sich geändert. ./scripts/bootstrap-gamedata.py --refresh")
        elif origin:
            print("Steam-Stand unverändert.")
    except SystemExit as exc:
        print(exc, file=sys.stderr)
        return 1
    return 0


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description="CS-Retro Game-Data aus Steam CS 1.6 erzeugen.")
    p.add_argument("--dest", help="Zielbaum (Default: <repo>/gamedata bzw. plattformüblicher User-Pfad)")
    p.add_argument("--map", default="de_dust", help="ZBot-Nav für diese Map holen")
    p.add_argument("--client", help="CS-Retro-Client-Lib zum Einsetzen")
    p.add_argument("--gamedll", help="CS-Retro-GameDLL zum Einsetzen")
    p.add_argument("--refresh", action="store_true", help="Steam-sourced Dateien erneut kopieren")
    p.add_argument("--skip-extras", action="store_true", help="Kein ZBot-Profil/.nav")
    p.add_argument("--skip-modules", action="store_true", help="Keine Client/GameDLL-Kopie")
    p.add_argument("--print-dir", action="store_true", help="Nur den Game-Data-Pfad ausgeben")
    p.add_argument("--print-steam", action="store_true", help="Nur die erkannte Steam-CS-1.6-Quelle ausgeben")
    p.add_argument("--status", action="store_true", help="Stand anzeigen")
    return p


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    if args.print_dir:
        print(gamedata_dir(args.dest))
        return 0
    if args.print_steam:
        cs = find_cs16()
        print(cs["source"])
        return 0
    if args.status:
        return do_status(args)
    return do_import(args)


if __name__ == "__main__":
    sys.exit(main())
