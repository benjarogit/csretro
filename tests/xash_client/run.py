#!/usr/bin/env python3
"""Run the real client bridge against synthetic libraries, never a game."""

import argparse
import json
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--profile', choices=('linux', 'linux-port'), default='linux')
    parser.add_argument('--gtest-source', type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    presets = json.loads((root / 'client/CMakePresets.json').read_text())
    profile = next(p for p in presets['configurePresets'] if p['name'] == args.profile)
    with tempfile.TemporaryDirectory(prefix='csretro-callback-tests-') as tmp:
        configure = ['cmake', '-S', str(Path(__file__).parent), '-B', tmp, '-G', 'Ninja',
                     '-DCMAKE_BUILD_TYPE=Release']
        # Use the product profile's compiler flags, not a second ABI definition.
        configure.extend(f'-D{key}={value}' for key, value in profile['cacheVariables'].items()
                         if key == 'CMAKE_CXX_FLAGS')
        if args.gtest_source:
            configure.append(f'-DCSRETRO_GTEST_SOURCE={args.gtest_source.resolve()}')
        subprocess.run(configure, check=True)
        subprocess.run(['cmake', '--build', tmp, '--target', 'xash_client-tests', '-j', '4'], check=True)
        subprocess.run(['ctest', '--test-dir', tmp, '--output-on-failure'], check=True)


if __name__ == '__main__':
    main()
