"""Manual original-content selection works without a local Steam installation."""
from pathlib import Path
import subprocess
import tempfile
import unittest

import test_install_content as fixtures

install_content = fixtures.install_content


class ManualContentSource(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="csretro-manual-content-")
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.game = self.root / "Original game with spaces"
        fixtures._write_files(self.game, fixtures.CONTENT)
        fixtures._write_files(self.game, fixtures.LEFT_BEHIND)

    def test_root_and_leaf_selections_copy_the_same_allowlist_without_steam(self):
        for selection in (self.game, self.game / "valve", self.game / "cstrike"):
            with self.subTest(selection=selection.name):
                destination = self.root / ("copy-" + selection.name)
                result = install_content.install_content([], destination, source=selection)
                self.assertTrue(result["ok"], result)
                self.assertEqual(result["source"], str(self.game))
                self.assertIsNone(result["library"])
                expected = sorted(fixtures.CONTENT)
                self.assertEqual(fixtures._files_under(destination), expected)
                self.assertEqual(result["copied"], expected)
                self.assertEqual((self.game / "hw.so").read_bytes(), b"engine")

    def test_manual_selection_overrides_another_detectable_game(self):
        steam = self.root / "Steam"
        other = fixtures._plant_game(steam)
        fixtures._write_files(other, {"cstrike/maps/other.bsp": b"other"})
        result = install_content.install_content(
            [steam], self.root / "copy", source=self.game / "valve")
        self.assertTrue(result["ok"], result)
        self.assertNotIn("cstrike/maps/other.bsp", result["copied"])
        (steam / "steamapps/libraryfolders.vdf").write_text('"broken" {')
        result = install_content.install_content(
            [steam], self.root / "copy-broken-metadata", source=self.game)
        self.assertTrue(result["ok"], result)

    def test_invalid_manual_selection_does_not_fall_back_to_steam(self):
        steam = self.root / "Steam"
        fixtures._plant_game(steam)
        for selection, error in ((self.root / "missing", "source_missing"),
                                 (self.root, "source_layout_invalid")):
            with self.subTest(error=error):
                destination = self.root / "copy"
                result = install_content.install_content([steam], destination, source=selection)
                self.assertFalse(result["ok"])
                self.assertEqual(result["error"], error)
                self.assertFalse(destination.exists())

    def test_incomplete_leaf_requires_siblings_and_required_assets(self):
        (self.game / "valve/liblist.gam").unlink()
        destination = self.root / "copy"
        result = install_content.install_content([], destination, source=self.game / "cstrike")
        self.assertEqual(result["error"], "game_missing")
        self.assertIn("valve/liblist.gam", result["missing"])
        self.assertFalse(destination.exists())
        lone = self.root / "lone" / "cstrike"
        lone.mkdir(parents=True)
        result = install_content.install_content([], destination, source=lone)
        self.assertEqual(result["error"], "source_layout_invalid")
        self.assertIn("valve/", result["missing"])

    def test_manual_source_in_steam_preserves_library_write_protection(self):
        library = self.root / "SteamLibrary"
        game = fixtures._plant_game(library)
        destination = library / "new-copy"
        result = install_content.install_content([], destination, source=game / "valve")
        self.assertEqual(result["error"], "destination_inside_source")
        self.assertFalse(destination.exists())

    def test_known_steam_root_and_source_symlinks_are_protected(self):
        steam = self.root / "Steam"
        (steam / "steamapps").mkdir(parents=True)
        result = install_content.install_content(
            [steam], steam / "copy", source=self.game)
        self.assertEqual(result["error"], "destination_inside_source")
        alias = self.root / "alias"
        alias.symlink_to(self.game, target_is_directory=True)
        result = install_content.install_content([], alias / "copy", source=self.game)
        self.assertEqual(result["error"], "destination_inside_source")

    def test_overlay_rules_are_unchanged_for_manual_sources(self):
        overlay = self.root / "shipped"
        fixtures._write_files(overlay, {"cstrike/maps/de_dust2.bsp": b"replacement"})
        destination = self.root / "copy"
        result = install_content.install_content([], destination, overlay, source=self.game)
        self.assertTrue(result["ok"], result)
        self.assertEqual((destination / "cstrike/maps/de_dust2.bsp").read_bytes(), b"replacement")
        self.assertEqual((self.game / "cstrike/maps/de_dust2.bsp").read_bytes(), b"bsp")
        result = install_content.install_content(
            [], self.root / "other-copy", self.game / "cstrike", source=self.game)
        self.assertEqual(result["error"], "overlay_inside_source")

    def test_cli_accepts_leaf_source_when_steam_metadata_is_unavailable(self):
        result = subprocess.run(
            [str(fixtures.SCRIPT), "--source", str(self.game / "cstrike"),
             "--steam-root", str(self.root / "missing-steam"),
             "--overlay", str(self.root / "overlay"),
             "--destination", str(self.root / "copy")],
            capture_output=True, text=True, check=False)
        self.assertEqual(result.returncode, 1)
        self.assertIn("overlay_missing", result.stderr)
        (self.root / "overlay").mkdir()
        result = subprocess.run(
            [str(fixtures.SCRIPT), "--source", str(self.game / "cstrike"),
             "--steam-root", str(self.root / "missing-steam"),
             "--overlay", str(self.root / "overlay"),
             "--destination", str(self.root / "copy")],
            capture_output=True, text=True, check=False)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("ok copied=", result.stdout)


if __name__ == "__main__":
    unittest.main()
