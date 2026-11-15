"""Compile only our own fixtures and inspect them without loading any DSO."""
from __future__ import annotations

import importlib.util
import json
import os
import shlex
import shutil
import struct
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("check_elf_imports", ROOT / "tools/check_elf_imports.py")
checker = importlib.util.module_from_spec(spec)
spec.loader.exec_module(checker)


def profile_flags():
    profile = os.environ.get("CSRETRO_ELF_PROFILE", "linux")
    if profile not in {"linux", "linux-port"}:
        raise ValueError("CSRETRO_ELF_PROFILE must be linux or linux-port")
    presets = {item["name"]: item for item in json.loads((ROOT / "client/CMakePresets.json").read_text())["configurePresets"]}

    def variables(name):
        preset = presets[name]
        inherited = preset.get("inherits", [])
        result = {}
        for parent in [inherited] if isinstance(inherited, str) else inherited:
            result.update(variables(parent))
        result.update(preset.get("cacheVariables", {}))
        return result

    return shlex.split(variables(profile).get("CMAKE_C_FLAGS", ""))


@unittest.skipUnless(shutil.which("cc") and shutil.which("readelf"), "requires compiler and GNU readelf")
class ElfImports(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="csretro-elf-imports-")
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)

    def build(self, name, source, *flags):
        output = self.root / name
        subprocess.run(["cc", *profile_flags(), "-shared", "-fPIC", "-nostdlib", "-ftls-model=initial-exec",
                        "-x", "c", "-o", str(output), "-", "-x", "none", *flags], input=source,
                       text=True, capture_output=True, check=True)
        return output

    def version_script(self, name, text):
        path = self.root / name
        path.write_text(text)
        return "-Wl,--version-script=" + str(path)

    def test_same_soname_old_steam_provider_fails_modern_provider_passes(self):
        consumer = self.build("client with spaces.so", '''
            extern int SteamInternal_ContextInit(int);
            extern int SteamAPI_Init(void);
            int connect(void) { return SteamInternal_ContextInit(41) + SteamAPI_Init(); }
            __attribute__((constructor)) static void stop(void) { __builtin_trap(); }
        ''')
        old = self.build("old steam.so", "int SteamAPI_Init(void) { return 1; }", "-Wl,-soname,libsteam_api.so")
        modern = self.build("modern steam.so", '''
            int SteamInternal_ContextInit(int state) { return state + 1; }
            int SteamAPI_Init(void) { return 1; }
        ''', "-Wl,-soname,libsteam_api.so")
        rejected = checker.inspect_imports(consumer, [old])
        self.assertEqual(rejected["missing_imports"], ["SteamInternal_ContextInit"])
        accepted = checker.inspect_imports(consumer, [modern])
        self.assertEqual(accepted["symbol_screen"], "pass")
        self.assertEqual(accepted["abi"], "unverified")
        self.assertEqual(accepted["playability"], "unverified")
        self.assertIn("assumed", accepted["provider_graph"])

    def test_weak_imports_are_optional_and_private_definitions_do_not_satisfy(self):
        consumer = self.build("consumer.so", '''
            extern int optional(void) __attribute__((weak));
            extern int required(void);
            int call(void) { return required() + (optional ? optional() : 0); }
        ''')
        hidden = self.build("hidden.so", '__attribute__((visibility("hidden"))) int required(void) { return 2; }')
        self.assertEqual(checker.inspect_imports(consumer, [hidden])["missing_imports"], ["required"])
        provider = self.build("weak.so", '__attribute__((weak)) int required(void) { return 2; }')
        self.assertEqual(checker.inspect_imports(consumer, [provider])["symbol_screen"], "pass")

    def test_symbol_versions_must_match_and_only_default_version_serves_unversioned(self):
        script = self.version_script("versions.map", "V1 {}; V2 {} V1;")
        provider = self.build("versions.so", '''
            int old(void) { return 1; } int modern(void) { return 2; }
            __asm__(".symver old,required@V1");
            __asm__(".symver modern,required@@V2");
        ''', script)
        unversioned = self.build("unversioned.so", "extern int required(void); int call(void) { return required(); }")
        self.assertEqual(checker.inspect_imports(unversioned, [provider])["symbol_screen"], "pass")
        versioned = self.build("versioned.so", '''
            extern int required(void); __asm__(".symver required,required@V1");
            int call(void) { return required(); }
        ''', "-Wl,--no-as-needed", str(provider))
        self.assertEqual(checker.inspect_imports(versioned, [provider])["symbol_screen"], "pass")
        only_v2 = self.build("only-v2.so", '''
            int modern(void) { return 2; } __asm__(".symver modern,required@@V2");
        ''', script)
        self.assertEqual(checker.inspect_imports(versioned, [only_v2])["missing_imports"], ["required@V1"])
        nondefault = self.build("nondefault.so", '''
            int old(void) { return 1; } __asm__(".symver old,required@V1");
        ''', script)
        self.assertEqual(checker.inspect_imports(unversioned, [nondefault])["missing_imports"], ["required"])

    def test_data_and_tls_exports_are_supported_but_known_tls_mismatch_fails(self):
        consumer = self.build("data.so", "extern int datum; extern __thread int tls; int call(void) { return datum + tls; }")
        provider = self.build("data-provider.so", "int datum=4; __thread int tls=7;")
        self.assertEqual(checker.inspect_imports(consumer, [provider])["symbol_screen"], "pass")
        wrong = self.build("wrong-tls.so", "int datum=4; int tls=7;")
        result = checker.inspect_imports(consumer, [wrong])
        self.assertEqual(result["symbol_screen"], "fail")
        self.assertEqual(result["type_mismatches"][0]["symbol"], "tls")

    def test_incompatible_class_machine_and_missing_provider_fail_closed(self):
        consumer = self.build("consumer.so", "extern int required(void); int call(void) { return required(); }")
        provider = self.build("provider.so", "int required(void) { return 1; }")
        original = provider.read_bytes()
        for field, value in ((4, bytes([1 if original[4] == 2 else 2])),
                             (18, struct.pack("<H", 183))):
            changed = bytearray(original)
            changed[field:field + len(value)] = value
            provider.write_bytes(changed)
            self.assertEqual(checker.inspect_imports(consumer, [provider])["symbol_screen"], "fail")
        provider.write_bytes(original)
        result = checker.inspect_imports(consumer, [provider, self.root / "missing.so"])
        self.assertEqual(result["symbol_screen"], "fail")
        self.assertTrue(result["provider_errors"])
        self.assertFalse(result["missing_imports"])

    def test_missing_truncated_and_readelf_failure_are_errors(self):
        missing = self.root / "missing.so"
        self.assertTrue(checker.inspect_imports(missing, [])["errors"])
        missing.write_bytes(b"\x7fELF")
        self.assertTrue(checker.inspect_imports(missing, [])["errors"])
        consumer = self.build("consumer.so", "int datum=1;")
        with patch.object(checker.subprocess, "run", side_effect=OSError("readelf unavailable")):
            self.assertTrue(checker.inspect_imports(consumer, [])["errors"])
        failure = subprocess.CompletedProcess([], 1, "", "synthetic failure")
        with patch.object(checker.subprocess, "run", return_value=failure):
            self.assertIn("readelf failed", checker.inspect_imports(consumer, [])["errors"][0])

    def test_cli_json_failure_status_and_constructor_never_executes(self):
        marker = self.root / "constructor-executed"
        # Direct Linux syscalls for either supported profile avoid constructor imports.
        # Running/loading this synthetic artifact would create the marker and trap.
        consumer = self.build("constructor.so", '''
            __attribute__((constructor)) static void stop(void) {
#if defined(__x86_64__)
                register long number __asm__("rax") = 257;
                register long dir __asm__("rdi") = -100;
                register const char* name __asm__("rsi") = MARKER;
                register long flags __asm__("rdx") = 0101;
                register long mode __asm__("r10") = 0600;
                __asm__ volatile("syscall" : "+a"(number) : "D"(dir), "S"(name), "d"(flags), "r"(mode) : "rcx", "r11", "memory");
#else
                register long number __asm__("eax") = 8;
                register const char* name __asm__("ebx") = MARKER;
                register long mode __asm__("ecx") = 0600;
                __asm__ volatile("int $0x80" : "+a"(number) : "b"(name), "c"(mode) : "memory");
#endif
                __builtin_trap();
            }
            extern int required(void); int call(void) { return required(); }
        ''', '-DMARKER="' + str(marker) + '"')
        output = subprocess.run(["python3", str(ROOT / "tools/check_elf_imports.py"), str(consumer), "--json"],
                                capture_output=True, text=True, check=False)
        self.assertEqual(output.returncode, 1)
        self.assertEqual(json.loads(output.stdout)["missing_imports"], ["required"])
        self.assertFalse(marker.exists())
    def test_bad_header_version_and_size_fail(self):
        consumer = self.build("bad-header.so", "int datum=1;")
        original = consumer.read_bytes()
        for offset, value in ((20, b"\x02"), (52 if original[4] == 2 else 40, b"\x00\x00")):
            changed = bytearray(original)
            changed[offset:offset+len(value)] = value
            consumer.write_bytes(changed)
            self.assertTrue(checker.inspect_imports(consumer, [])["errors"])

    def test_function_import_cannot_bind_to_data_but_ifunc_is_a_function(self):
        reference = self.build("function-reference.so", "int required(void) { return 7; }")
        consumer = self.build("function.so", "extern int required(void); int call(void) { return required(); }",
                              "-Wl,--no-as-needed", str(reference))
        data = self.build("object.so", "int required=7;")
        rejected = checker.inspect_imports(consumer, [data])
        self.assertEqual(rejected["symbol_screen"], "fail")
        self.assertEqual(rejected["type_mismatches"][0]["required_type"], "FUNC")
        self.assertEqual(rejected["type_mismatches"][0]["provided_type"], "OBJECT")
        function = self.build("ifunc.so", """
            static int value(void) { return 7; }
            static void* resolve(void) { return (void*)value; }
            int required(void) __attribute__((ifunc("resolve")));
        """)
        self.assertEqual(checker.inspect_imports(consumer, [function])["symbol_screen"], "pass")


if __name__ == "__main__":
    unittest.main()
