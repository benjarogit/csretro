#!/usr/bin/env bash
# Build the CSRETRO client.
#
# The default builds x64 components. linux-port builds architecture-matched
# i386 components for the first Xash port. Neither profile starts the game.
#
# Paths with spaces break OpenSSL's Configure and several other port builds.
# When the repository lives under such a path, the script builds through a
# space-free symlink in ${TMPDIR:-/tmp}; the files stay where they are.
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"

PRESET="linux"
CONFIG="Release"
JOBS="$(nproc)"
TARGETS=(client_mini engine_mini nitro_api gameui cstrike.exe BUILD_ALL)
TARGETS_SELECTED=0
RECONFIGURE=0
CLEAN=0
INSTALL_DIR=""
MANIFEST=""

usage() {
    cat <<'EOF'
usage: scripts/build-client.sh [options]

  --preset <name>      linux (default) or linux-port; separate output trees
  --config <name>      CMake configuration (default: Release)
  --jobs <n>           parallel jobs (default: nproc)
  --target <name>      build only this target; repeatable
  --reconfigure        re-run the configure preset
  --clean              clean the selected preset build before configuring
  --install-dir <dir>  set NEXTCLIENT_INSTALL_DIR and reconfigure
  --manifest <file>    manifest path (default: selected build/manifest-<config>.txt)
  -h, --help           show this help
EOF
}

while (($# > 0)); do
    case "$1" in
        --preset) PRESET="${2:?missing value}"; shift ;;
        --config) CONFIG="${2:?missing value}"; shift ;;
        --jobs) JOBS="${2:?missing value}"; shift ;;
        --target)
            if ((! TARGETS_SELECTED)); then TARGETS=(); TARGETS_SELECTED=1; fi
            TARGETS+=("${2:?missing value}"); shift ;;
        --reconfigure) RECONFIGURE=1 ;;
        --clean) CLEAN=1 ;;
        --install-dir) INSTALL_DIR="${2:?missing value}"; shift ;;
        --manifest) MANIFEST="${2:?missing value}"; shift ;;
        -h|--help) usage; exit 0 ;;
        *) printf 'unknown option: %s\n\n' "$1" >&2; usage >&2; exit 2 ;;
    esac
    shift
done

case "$PRESET" in
    linux|linux-port) ;;
    *) printf 'unsupported client preset: %s\n' "$PRESET" >&2; exit 2 ;;
esac

if [[ "$ROOT" == *" "* ]]; then
    SRC="${TMPDIR:-/tmp}/csretro-src"
    ln -sfn "$ROOT" "$SRC"
    printf 'note: repository path contains spaces, building through %s\n' "$SRC"
else
    SRC="$ROOT"
fi

CLIENT="$SRC/client"
BUILD="$CLIENT/build/$PRESET"

if ((CLEAN)); then
    "$ROOT/scripts/clean.sh" --client-build "$PRESET"
fi

configure_args=()
if [[ -n "$INSTALL_DIR" ]]; then
    configure_args+=("-DNEXTCLIENT_INSTALL_DIR=$INSTALL_DIR")
fi

if [[ ! -f "$BUILD/CMakeCache.txt" || ! -f "$BUILD/artifact-elf-class.txt" ]] || ((RECONFIGURE)) || [[ -n "$INSTALL_DIR" ]]; then
    printf '==> configure (preset %s)\n' "$PRESET"
    (cd "$CLIENT" && cmake --preset "$PRESET" "${configure_args[@]}")
fi

started="$(date +%s)"
for target in "${TARGETS[@]}"; do
    printf '\n==> build %s (%s)\n' "$target" "$CONFIG"
    (cd "$CLIENT" && cmake --build --preset "$PRESET-release" --config "$CONFIG" --target "$target" -j "$JOBS")
done
finished="$(date +%s)"

out="$BUILD/out/bin/$CONFIG"
manifest="${MANIFEST:-$BUILD/manifest-$CONFIG.txt}"
version="$(cat "$ROOT/VERSION" 2>/dev/null || echo unknown)"
commit="$(git -C "$ROOT" rev-parse HEAD 2>/dev/null || echo unknown)"

{
    printf '# CSRETRO client build manifest\n'
    printf '# version: %s\n' "$version"
    printf '# commit: %s\n' "$commit"
    printf '# config: %s\n' "$CONFIG"
    printf '# preset: %s\n' "$PRESET"
    printf '# date: %s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
    printf '# build_seconds: %s\n' "$((finished - started))"
    printf '# size_bytes\tsha256\telf\tpath\n'
    find "$out" -type f \( -name '*.so' -o -name '*.dll' -o -name '*.exe' \) -print0 |
        sort -z |
        while IFS= read -r -d '' file; do
            sha="$(sha256sum "$file" | awk '{print $1}')"
            size="$(stat -c '%s' "$file")"
            elf="$(readelf -h "$file" 2>/dev/null | awk -F: '/Class/{gsub(/ /,"",$2); print $2}')"
            printf '%s\t%s\t%s\t%s\n' "$size" "$sha" "$elf" "${file#"$out"/}"
        done
} >"$manifest"

# The compiler ABI is authoritative for this build. An i386 port component
# cannot silently land in an x64 component set (or vice versa).
expected_elf="$(cat "$BUILD/artifact-elf-class.txt")"
case "$expected_elf" in
    ELF32|ELF64) ;;
    *) printf 'ERROR: invalid configured artifact class\n' >&2; exit 1 ;;
esac
if ! awk -F'\t' '!/^#/ { count++ } END { exit count == 0 }' "$manifest"; then
    printf 'ERROR: no shared artifacts were produced\n' >&2
    exit 1
fi
bad="$(awk -F'\t' -v expected="$expected_elf" '!/^#/ && $3 != expected { print $4 }' "$manifest")"
if [[ -n "$bad" ]]; then
    printf '\nERROR: artifacts do not match configured %s ABI:\n%s\n' "$expected_elf" "$bad" >&2
    exit 1
fi

printf '\n==> artifacts\n'
awk -F'\t' '!/^#/ { printf "  %10d  %-24s %s\n", $1, $3, $4 }' "$manifest"
printf '\nmanifest: %s\n' "${manifest/#$SRC/$ROOT}"
printf 'build time: %s s\n' "$((finished - started))"
