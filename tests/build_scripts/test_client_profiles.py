"""Exercise profile isolation and ELF rejection without building/running a game."""
import json
import os
import platform
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


def elf_header(bits):
    ident = b"\x7fELF" + bytes([1 if bits == 32 else 2, 1, 1]) + bytes(9)
    if bits == 32:
        return struct.pack("<16sHHIIIIIHHHHHH", ident, 3, 3, 1, 0, 0, 0, 0, 52, 0, 0, 0, 0, 0)
    return struct.pack("<16sHHIQQQIHHHHHH", ident, 3, 62, 1, 0, 0, 0, 0, 64, 0, 0, 0, 0, 0)


@unittest.skipUnless(shutil.which("readelf"), "requires binutils")
class ClientBuildProfiles(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="csretro-profile-test-")
        self.addCleanup(self.tmp.cleanup)
        self.base = Path(self.tmp.name)
        self.repo = self.base / "repo with spaces"
        (self.repo / "scripts").mkdir(parents=True)
        (self.repo / "client").mkdir()
        for name in ("build-client.sh", "clean.sh"):
            shutil.copy2(ROOT / "scripts" / name, self.repo / "scripts" / name)
        shutil.copyfile(ROOT / "VERSION", self.repo / "VERSION")
        self.bin = self.base / "bin"
        self.bin.mkdir()
        self.log = self.base / "cmake-calls.jsonl"
        self.artifact = self.base / "fixture.so"
        self.artifact.write_bytes(elf_header(32))
        fake = self.bin / "cmake"
        fake.write_text("#!" + sys.executable + "\n" + '''
import json, os, pathlib, shutil, sys
args = sys.argv[1:]
with open(os.environ["PROFILE_CALLS"], "a") as stream:
    stream.write(json.dumps(args) + "\\n")
preset = args[args.index("--preset") + 1]
if "--build" in args:
    profile = preset.removesuffix("-release")
    config = args[args.index("--config") + 1]
    out = pathlib.Path("build") / profile / "out/bin" / config
    out.mkdir(parents=True, exist_ok=True)
    if not os.environ.get("PROFILE_EMPTY"):
        shutil.copyfile(os.environ["PROFILE_ARTIFACT"], out / "surface.so")
else:
    build = pathlib.Path("build") / preset
    build.mkdir(parents=True, exist_ok=True)
    (build / "CMakeCache.txt").write_text("fixture configuration")
    (build / "artifact-elf-class.txt").write_text(os.environ["PROFILE_CLASS"] + "\\n")
''')
        fake.chmod(0o755)
        scratch = self.base / "scratch"
        scratch.mkdir()
        self.env = dict(os.environ, PATH=str(self.bin) + os.pathsep + os.environ["PATH"],
                        TMPDIR=str(scratch), PROFILE_CALLS=str(self.log),
                        PROFILE_ARTIFACT=str(self.artifact), PROFILE_CLASS="ELF32")

    def build(self, *args):
        return subprocess.run([str(self.repo / "scripts/build-client.sh"), *args],
                              env=self.env, capture_output=True, text=True, check=False)

    def test_port_manifest_uses_configured_abi_and_presets(self):
        result = self.build("--preset", "linux-port", "--target", "xash_surface")
        self.assertEqual(result.returncode, 0, result.stderr)
        manifest = (self.repo / "client/build/linux-port/manifest-Release.txt").read_text()
        self.assertIn("# preset: linux-port", manifest)
        self.assertIn("\tELF32\tsurface.so", manifest)
        calls = [json.loads(line) for line in self.log.read_text().splitlines()]
        self.assertEqual(calls[0], ["--preset", "linux-port"])
        self.assertIn("linux-port-release", calls[1])

    def test_default_remains_x64(self):
        self.env["PROFILE_CLASS"] = "ELF64"
        self.artifact.write_bytes(elf_header(64))
        result = self.build("--target", "xash_surface")
        self.assertEqual(result.returncode, 0, result.stderr)
        manifest = (self.repo / "client/build/linux/manifest-Release.txt").read_text()
        self.assertIn("\tELF64\tsurface.so", manifest)
        self.assertFalse((self.repo / "client/build/linux-port").exists())

    def test_wrong_architecture_is_rejected(self):
        self.artifact.write_bytes(elf_header(64))
        result = self.build("--preset", "linux-port", "--target", "xash_surface")
        self.assertEqual(result.returncode, 1)
        self.assertIn("do not match configured ELF32 ABI", result.stderr)

    def test_empty_output_is_not_a_successful_build(self):
        self.env["PROFILE_EMPTY"] = "1"
        result = self.build("--preset", "linux-port", "--target", "xash_surface")
        self.assertEqual(result.returncode, 1)
        self.assertIn("no shared artifacts", result.stderr)

    def test_repeated_targets_are_built_sequentially(self):
        result = self.build("--preset", "linux-port", "--target", "surface", "--target", "bridge")
        self.assertEqual(result.returncode, 0, result.stderr)
        calls = [json.loads(line) for line in self.log.read_text().splitlines()]
        targets = [call[call.index("--target") + 1] for call in calls if "--target" in call]
        self.assertEqual(targets, ["surface", "bridge"])

    def test_clean_port_preserves_x64_source_and_other_state(self):
        stale = self.repo / "client/build/linux-port/stale"
        stale.parent.mkdir(parents=True)
        stale.write_text("old")
        keep = self.repo / "client/build/linux/keep"
        keep.parent.mkdir(parents=True)
        keep.write_text("x64")
        source = self.repo / "client/source.cpp"
        source.write_text("source")
        result = self.build("--preset", "linux-port", "--clean", "--target", "xash_surface")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertFalse(stale.exists())
        self.assertEqual(keep.read_text(), "x64")
        self.assertEqual(source.read_text(), "source")

    def test_invalid_profile_and_cleanup_traversal_are_rejected(self):
        result = self.build("--preset", "../../client", "--clean")
        self.assertEqual(result.returncode, 2)
        self.assertFalse(self.log.exists())
        result = subprocess.run([str(self.repo / "scripts/clean.sh"), "--client-build", "../../client"],
                                capture_output=True, text=True, check=False)
        self.assertEqual(result.returncode, 2)
        self.assertTrue((self.repo / "client").exists())

    def test_selective_dry_run_preserves_build(self):
        build = self.repo / "client/build/linux-port"
        build.mkdir(parents=True)
        result = subprocess.run([str(self.repo / "scripts/clean.sh"), "--dry-run", "--client-build", "linux-port"],
                                capture_output=True, text=True, check=False)
        self.assertEqual(result.returncode, 0)
        self.assertIn("would remove client/build/linux-port", result.stdout)
        self.assertTrue(build.is_dir())


@unittest.skipUnless(shutil.which("cmake") and shutil.which("cc") and shutil.which("ninja")
                     and platform.machine() == "x86_64", "requires x86_64 compiler/CMake/Ninja")
class PortAssemblerABI(unittest.TestCase):
    def test_real_assembler_uses_port_flags_and_negative_control_fails(self):
        presets = json.loads((ROOT / "client/CMakePresets.json").read_text())
        port = next(item for item in presets["configurePresets"] if item["name"] == "linux-port")
        flags = {key: value for key, value in port["cacheVariables"].items()
                 if key in {"CMAKE_C_FLAGS", "CMAKE_ASM_FLAGS"}}
        with tempfile.TemporaryDirectory(prefix="csretro-port-asm-") as tmp:
            source = Path(tmp)
            (source / "CMakeLists.txt").write_text(
                'cmake_minimum_required(VERSION 3.21)\n'
                'project(PortAsmSmoke LANGUAGES C ASM)\n'
                'add_library(probe STATIC entry.S)\n')
            (source / "entry.S").write_text(
                '.text\n.globl csretro_asm_probe\ncsretro_asm_probe:\n'
                'pushl %ebp\npopl %ebp\nret\n')
            env = dict(os.environ)
            for name in ("ASMFLAGS", "CFLAGS", "CC", "AS"):
                env.pop(name, None)
            for control in (False, True):
                build = source / ("negative" if control else "positive")
                selected = dict(flags)
                if control:
                    selected.pop("CMAKE_ASM_FLAGS", None)
                args = ["cmake", "-S", str(source), "-B", str(build), "-G", "Ninja",
                        "-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY",
                        "-DCMAKE_C_COMPILER=" + shutil.which("cc"),
                        "-DCMAKE_ASM_COMPILER=" + shutil.which("cc")]
                args += [f"-D{key}={value}" for key, value in selected.items()]
                configure = subprocess.run(args, env=env, capture_output=True, text=True, check=False)
                self.assertEqual(configure.returncode, 0, configure.stderr)
                compile_result = subprocess.run(["cmake", "--build", str(build)], env=env,
                                                capture_output=True, text=True, check=False)
                if control:
                    self.assertNotEqual(compile_result.returncode, 0)
                else:
                    self.assertEqual(compile_result.returncode, 0, compile_result.stdout + compile_result.stderr)
                    objects = list(build.rglob("entry.S.o"))
                    self.assertEqual(len(objects), 1)
                    header = objects[0].read_bytes()[:20]
                    self.assertEqual(header[4], 1)
                    self.assertEqual(struct.unpack_from("<H", header, 18)[0], 3)


if __name__ == "__main__":
    unittest.main()
