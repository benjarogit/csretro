"""Native fixtures prove that module loading and named game exports differ."""
from __future__ import annotations

import importlib.util
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("check_game_client", ROOT / "tools/check_game_client.py")
checker = importlib.util.module_from_spec(spec)
spec.loader.exec_module(checker)


@unittest.skipUnless(shutil.which("cc") and shutil.which("nm"), "requires C compiler and binutils")
class GameClientExports(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="csretro-game-client-")
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)

    def build(self, source):
        path = self.root / "client with spaces.so"
        subprocess.run(["cc", "-shared", "-fPIC", "-x", "c", "-o", str(path), "-"],
                       input=source, text=True, check=True, capture_output=True)
        return path

    def test_complete_exports_are_only_an_export_screen(self):
        # Loading this fixture would terminate the checker. Inspection must not.
        source = "__attribute__((constructor)) static void stop(void) { __builtin_trap(); }\n"
        source += "\n".join(f"void {name}(void) {{}}" for name in checker.REQUIRED_EXPORTS)
        result = checker.inspect_client(self.build(source))
        self.assertEqual(result["export_screen"], "pass")
        self.assertEqual(result["playability"], "unverified")

    def test_hook_factory_does_not_make_a_game_client(self):
        result = checker.inspect_client(self.build("void CreateInterface(void) {}"))
        self.assertEqual(result["export_screen"], "fail")
        self.assertIn("Initialize", result["errors"][0])

    def test_hidden_functions_and_data_are_not_callable_exports(self):
        source = "\n".join(f"int {name};" for name in checker.REQUIRED_EXPORTS)
        result = checker.inspect_client(self.build(source))
        self.assertEqual(result["export_screen"], "fail")
        source = "\n".join(f'__attribute__((visibility("hidden"))) void {name}(void) {{}}'
                           for name in checker.REQUIRED_EXPORTS)
        self.assertEqual(checker.inspect_client(self.build(source))["export_screen"], "fail")

    def test_imported_function_is_not_an_export(self):
        path = self.build("extern void Initialize(void); void caller(void) { Initialize(); }")
        self.assertEqual(checker.inspect_client(path)["export_screen"], "fail")

    def test_wrong_architecture_truncated_and_missing_files_fail(self):
        path = self.build("void CreateInterface(void) {}")
        header = bytearray(path.read_bytes())
        original = bytes(header)
        header[4] = 1
        path.write_bytes(header)
        self.assertIn("ELF64", checker.inspect_client(path)["errors"][0])
        header = bytearray(original)
        header[18:20] = b"\xb7\x00"  # AArch64, not our product architecture.
        path.write_bytes(header)
        self.assertIn("x86_64", checker.inspect_client(path)["errors"][0])
        path.write_bytes(b"\x7fELF")
        self.assertEqual(checker.inspect_client(path)["export_screen"], "fail")
        self.assertEqual(checker.inspect_client(self.root / "missing.so")["export_screen"], "fail")


if __name__ == "__main__":
    unittest.main()
