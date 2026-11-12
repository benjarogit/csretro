"""Install copy against a fake Steam library. Never reads a real Steam path."""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

import install_content  # noqa: E402

FIXTURES = Path(__file__).resolve().parent / "fixtures"
SCRIPT = ROOT / "scripts" / "install-content.sh"

CONTENT = {
    "cstrike/liblist.gam": b"gamedir cstrike\n",
    "cstrike/delta.lst": b"delta\n",
    "cstrike/mapcycle.txt": b"de_dust2\n",
    "cstrike/motd.txt": b"motd\n",
    "cstrike/commandmenu.txt": b"menu\n",
    "cstrike/de_dust2.wad": b"wad",
    "cstrike/maps/de_dust2.bsp": b"bsp",
    "cstrike/maps/de_dust2.txt": b"maptxt",
    "cstrike/models/player/leet/leet.mdl": b"mdl",
    "cstrike/sound/weapons/ak47.wav": b"wav",
    "cstrike/sprites/smoke.spr": b"spr",
    "cstrike/gfx/vgui/round_corner_nw.tga": b"tga",
    "cstrike/gfx/env/dustup.tga": b"sky",
    "cstrike/resource/ClientScheme.res": b"scheme",
    "cstrike/resource/GameMenu.res": b"gamemenu",
    "cstrike/events/ak47.sc": b"event",
    "cstrike/overviews/de_dust2.bmp": b"overview",
    "cstrike/classes/ak47.res": b"buy",
    "cstrike/logos/remapped.bmp": b"logo",
    "valve/liblist.gam": b"gamedir valve\n",
    "valve/default.cfg": b"default\n",
    "valve/valve.rc": b"rc\n",
    "valve/halflife.wad": b"wad",
    "valve/models/player.mdl": b"vmodel",
    "valve/sound/ambience/beep.wav": b"beep",
    "valve/sprites/voiceicon.spr": b"voice",
    "valve/gfx/env/other.tga": b"env",
    "valve/resource/marlett.ttf": b"marlett-font",
    "valve/resource/BackgroundLayout.txt": b"layout",
    "valve/resource/gameui_english.txt": b"lang",
    "valve/resource/ClientScheme.res": b"valve-scheme",
    "valve/media/Half-Life01.mp3": b"mp3",
    "valve/scripts/voicemodel.txt": b"voice-model",
    "platform/resource/platform_english.txt": b"platform",
    "platform/resource/vgui_english.txt": b"vgui",
    "platform/resource/linux_fonts/DejaVuSans.ttf": b"font",
}

LEFT_BEHIND = {
    "hw.so": b"engine",
    "libsteam.so": b"steam",
    "libsteam_api.so": b"steamapi",
    "chromehtml.so": b"cef",
    "hl_linux": b"launcher",
    "cstrike/config.cfg": b"name benny\n",
    "cstrike/userconfig.cfg": b"user\n",
    "cstrike/autoexec.cfg": b"exec\n",
    "cstrike/listenserver.cfg": b"listen\n",
    "cstrike/server.cfg": b"rcon_password secret\n",
    "cstrike/custom.hpk": b"spray",
    "cstrike/cached.wad": b"cache",
    "cstrike/tempdecal.wad": b"decal",
    "cstrike/steam_appid.txt": b"10\n",
    "cstrike/readme.txt": b"readme",
    "cstrike/halflife-cs.fgd": b"fgd",
    "cstrike/motd_temp.html": b"<html>",
    "cstrike/de_dust2.dem": b"demo",
    "cstrike/maps/extra.dem": b"demo",
    "cstrike/sound/sound.cache": b"cache",
    "cstrike/cl_dlls/client.so": b"client",
    "cstrike/dlls/cs.so": b"server",
    "cstrike/screenshots/shot.png": b"png",
    "valve/config.cfg": b"binds\n",
    "valve/autoexec.cfg": b"exec\n",
    "valve/hw/d3d.cfg": b"old-card",
    "platform/config/ServerBrowser.vdf": b"personal",
    "platform/steam/games/id.txt": b"steam-ui",
    "platform/gl_shaders/fs_world.frag": b"shader",
    "htmlcache/Cache.bin": b"html",
    "cstrike_downloads/maps/de_dust2.bsp": b"download",
    "cstrike_hd/models/player.mdl": b"hd",
    "penemy/liblist.gam": b"mod",
    "redist/setup.exe": b"setup",
    "logs/console.log": b"log",
}


def _write_files(root: Path, files: dict[str, bytes]) -> None:
    for relative, data in files.items():
        path = root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)


def _manifest(installdir: str = "Half-Life", appid: str = "10") -> str:
    text = (FIXTURES / "appmanifest_10.acf").read_text(encoding="utf-8")
    text = text.replace('"installdir"\t\t"Half-Life"', f'"installdir"\t\t"{installdir}"')
    text = text.replace('"appid"\t\t"10"', f'"appid"\t\t"{appid}"')
    return text


def _libraryfolders(steam: Path, extra: Path, legacy: bool = False) -> str:
    name = "libraryfolders-legacy.vdf" if legacy else "libraryfolders.vdf"
    text = (FIXTURES / name).read_text(encoding="utf-8")
    text = text.replace("/libraries/SteamLibrary", extra.as_posix())
    text = text.replace("/libraries/Steam", steam.as_posix())
    return text


def _plant_game(library: Path, files: dict[str, bytes] | None = None) -> Path:
    game = library / "steamapps" / "common" / "Half-Life"
    _write_files(game, CONTENT if files is None else files)
    _write_files(game, LEFT_BEHIND)
    (library / "steamapps").mkdir(parents=True, exist_ok=True)
    (library / "steamapps" / "appmanifest_10.acf").write_text(_manifest(), encoding="utf-8")
    return game


def _files_under(root: Path) -> list[str]:
    return sorted(path.relative_to(root).as_posix() for path in root.rglob("*") if path.is_file())


def test_fixture_is_the_current_libraryfolders_format():
    document = install_content.parse_vdf((FIXTURES / "libraryfolders.vdf").read_text(encoding="utf-8"))
    folders = document["libraryfolders"]
    assert folders["0"]["path"] == "/libraries/Steam"
    assert folders["0"]["apps"]["10"] == "100"
    assert folders["1"]["path"] == "/libraries/SteamLibrary"
    assert "220" in folders["0"]["apps"]


def test_fixture_appmanifest_is_app_10():
    document = install_content.parse_vdf((FIXTURES / "appmanifest_10.acf").read_text(encoding="utf-8"))
    assert document["AppState"]["appid"] == "10"
    assert document["AppState"]["installdir"] == "Half-Life"
    assert document["AppState"]["buildid"] == "12934623"


def test_parser_unescapes_windows_paths():
    text = '"libraryfolders"\n{\n"0"\n{\n"path"\t\t"C:\\\\Games\\\\Steam"\n}\n}\n'
    document = install_content.parse_vdf(text)
    assert document["libraryfolders"]["0"]["path"] == "C:\\Games\\Steam"


def test_default_roots_use_the_linux_steam_layouts():
    roots = install_content.default_steam_roots()
    assert len(roots) == 3
    assert roots[0].parts[-2:] == (".steam", "steam")
    assert roots[1].parts[-3:] == (".local", "share", "Steam")
    assert roots[2].parts[-6:] == (
        ".var",
        "app",
        "com.valvesoftware.Steam",
        ".local",
        "share",
        "Steam",
    )


def test_missing_steam_is_a_hard_failure(tmp_path: Path):
    empty = tmp_path / "empty"
    empty.mkdir()
    result = install_content.discover([empty])
    assert result["ok"] is False
    assert result["error"] == "steam_missing"
    assert result["copied"] == []


def test_steam_without_the_game_is_a_hard_failure(tmp_path: Path):
    steam = tmp_path / "Steam"
    (steam / "steamapps").mkdir(parents=True)
    result = install_content.discover([steam])
    assert result["ok"] is False
    assert result["error"] == "game_missing"


def test_malformed_libraryfolders_fails(tmp_path: Path):
    steam = tmp_path / "Steam"
    vdf = steam / "steamapps" / "libraryfolders.vdf"
    vdf.parent.mkdir(parents=True)
    vdf.write_text('"libraryfolders"\n{\n', encoding="utf-8")
    result = install_content.discover([steam])
    assert result["error"] == "steam_metadata_invalid"


def test_discover_reads_only_the_given_root(tmp_path: Path):
    steam = tmp_path / "Steam"
    extra = tmp_path / "SteamLibrary"
    (steam / "steamapps").mkdir(parents=True)
    (steam / "steamapps" / "libraryfolders.vdf").write_text(
        _libraryfolders(steam, extra), encoding="utf-8"
    )
    _plant_game(extra)
    result = install_content.discover([steam])
    assert result["ok"] is True
    assert result["source"].endswith("/SteamLibrary/steamapps/common/Half-Life")
    assert result["library"].endswith("/SteamLibrary")


def test_legacy_libraryfolders_string_path(tmp_path: Path):
    document = install_content.parse_vdf(
        (FIXTURES / "libraryfolders-legacy.vdf").read_text(encoding="utf-8")
    )
    assert document["LibraryFolders"]["1"] == "/libraries/SteamLibrary"

    steam = tmp_path / "Steam"
    extra = tmp_path / "SteamLibrary"
    (steam / "steamapps").mkdir(parents=True)
    (steam / "steamapps" / "libraryfolders.vdf").write_text(
        _libraryfolders(steam, extra, legacy=True), encoding="utf-8"
    )
    _plant_game(extra)
    result = install_content.discover([steam])
    assert result["ok"] is True
    assert result["source"].endswith("/SteamLibrary/steamapps/common/Half-Life")


def test_first_valid_library_wins(tmp_path: Path):
    steam = tmp_path / "Steam"
    extra = tmp_path / "SteamLibrary"
    (steam / "steamapps").mkdir(parents=True)
    (steam / "steamapps" / "libraryfolders.vdf").write_text(
        _libraryfolders(steam, extra), encoding="utf-8"
    )
    first = _plant_game(steam)
    second = _plant_game(extra)
    (first / "cstrike" / "maps" / "first.bsp").write_bytes(b"first")
    (second / "cstrike" / "maps" / "second.bsp").write_bytes(b"second")
    destination = tmp_path / "copy"
    result = install_content.install_content([steam], destination)
    assert result["ok"] is True
    assert "cstrike/maps/first.bsp" in result["copied"]
    assert "cstrike/maps/second.bsp" not in result["copied"]


def test_copy_is_the_allowlist_and_refuses_the_library(tmp_path: Path):
    steam = tmp_path / "Steam"
    extra = tmp_path / "SteamLibrary"
    (steam / "steamapps").mkdir(parents=True)
    (steam / "steamapps" / "libraryfolders.vdf").write_text(
        _libraryfolders(steam, extra), encoding="utf-8"
    )
    game = _plant_game(extra)
    link = game / "cstrike" / "maps" / "link.bsp"
    link.symlink_to(game / "hw.so")

    inside = extra / "steamapps" / "common" / "Half-Life" / "nested"
    inside.mkdir()
    (inside / "marker.txt").write_text("keep", encoding="utf-8")
    refused = install_content.install_content([steam], inside)
    assert refused["ok"] is False
    assert refused["error"] == "destination_inside_source"
    assert (inside / "marker.txt").read_text(encoding="utf-8") == "keep"
    assert not (inside / "cstrike").exists()
    assert (game / "hw.so").read_bytes() == b"engine"

    destination = tmp_path / "csretro-copy"
    result = install_content.install_content([steam], destination)
    assert result["ok"] is True
    assert result["error"] is None
    expected = sorted(path for path in CONTENT if install_content.decision(path) == "copy")
    assert result["copied"] == expected
    assert _files_under(destination) == expected
    assert (destination / "valve" / "resource" / "marlett.ttf").read_bytes() == b"marlett-font"
    assert (destination / "valve" / "resource" / "BackgroundLayout.txt").read_bytes() == b"layout"
    assert (destination / "cstrike" / "maps" / "de_dust2.bsp").read_bytes() == b"bsp"

    copied = set(result["copied"])
    for relative in LEFT_BEHIND:
        assert relative not in copied
    assert "cstrike/maps/link.bsp" not in copied
    reasons = {item["path"]: item["reason"] for item in result["skipped"]}
    assert reasons["cstrike/config.cfg"] == "personal"
    assert reasons["cstrike/maps/extra.dem"] == "demo"
    assert reasons["cstrike/sound/sound.cache"] == "cache"
    assert reasons["cstrike/cached.wad"] == "cache"
    assert reasons["hw.so"] == "binary"
    assert reasons["libsteam.so"] == "binary"
    assert reasons["platform/config/ServerBrowser.vdf"] == "personal"
    assert reasons["cstrike/readme.txt"] == "not_content"
    assert reasons["cstrike/maps/link.bsp"] == "symlink"
    names = {path.relative_to(destination).as_posix() for path in destination.rglob("*")}
    for banned in (
        "hw.so",
        "libsteam.so",
        "htmlcache",
        "cstrike/cl_dlls",
        "cstrike/screenshots",
        "platform/config",
        "platform/steam",
        "cstrike_downloads",
        "penemy",
        "logs",
    ):
        assert banned not in names


def test_shipped_overlay_wins_the_same_relative_path(tmp_path: Path):
    steam = tmp_path / "Steam"
    game = _plant_game(steam)
    overlay = tmp_path / "shipped"
    collisions = {
        "cstrike/maps/de_dust2.bsp": b"shipped-map",
        "cstrike/models/player/leet/leet.mdl": b"shipped-model",
        "cstrike/sound/weapons/ak47.wav": b"shipped-weapon",
        "cstrike/sprites/smoke.spr": b"shipped-sprite",
        "cstrike/resource/GameMenu.res": b"shipped-resource",
    }
    for relative, payload in collisions.items():
        path = overlay / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(payload)
    shipped_only = overlay / "cstrike/sprites/csretro_weapon.txt"
    shipped_only.parent.mkdir(parents=True, exist_ok=True)
    shipped_only.write_bytes(b"shipped-only")

    destination = tmp_path / "csretro-copy"
    result = install_content.install_content([steam], destination, overlay)
    assert result["ok"] is True
    assert result["replaced"] == sorted(collisions)
    for relative, payload in collisions.items():
        assert (destination / relative).read_bytes() == payload
        assert (game / relative).read_bytes() != payload
    assert (destination / "cstrike/maps/de_dust2.txt").read_bytes() == b"maptxt"
    assert (destination / "cstrike/events/ak47.sc").read_bytes() == b"event"
    assert (destination / "valve/sound/ambience/beep.wav").read_bytes() == b"beep"
    assert (destination / "cstrike/sprites/csretro_weapon.txt").read_bytes() == b"shipped-only"
    assert "cstrike/maps/de_dust2.txt" not in result["replaced"]
    assert (game / "hw.so").read_bytes() == b"engine"
    assert not (destination / "hw.so").exists()


def test_overlay_inside_the_library_is_refused(tmp_path: Path):
    steam = tmp_path / "Steam"
    game = _plant_game(steam)
    destination = tmp_path / "csretro-copy"
    result = install_content.install_content([steam], destination, game / "cstrike")
    assert result["ok"] is False
    assert result["error"] == "overlay_inside_source"
    assert not destination.exists()
    assert (game / "cstrike/maps/de_dust2.bsp").read_bytes() == b"bsp"


def test_incomplete_game_does_not_copy(tmp_path: Path):
    library = tmp_path / "SteamLibrary"
    game = library / "steamapps" / "common" / "Half-Life"
    _write_files(game, {"cstrike/liblist.gam": b"x", "valve/liblist.gam": b"y"})
    (library / "steamapps" / "appmanifest_10.acf").write_text(_manifest(), encoding="utf-8")
    destination = tmp_path / "copy"
    result = install_content.install_content([library], destination)
    assert result["ok"] is False
    assert result["error"] == "game_missing"
    assert "valve/resource/marlett.ttf" in result["missing"]
    assert "cstrike/maps/" in result["missing"]
    assert not destination.exists()


def test_script_reports_a_missing_game(tmp_path: Path):
    steam = tmp_path / "Steam"
    (steam / "steamapps").mkdir(parents=True)
    destination = tmp_path / "copy"
    completed = subprocess.run(
        [str(SCRIPT), "--steam-root", str(steam), "--destination", str(destination)],
        check=False,
        capture_output=True,
        text=True,
    )
    assert completed.returncode == 1
    assert "error: game_missing" in completed.stderr
    assert not destination.exists()
