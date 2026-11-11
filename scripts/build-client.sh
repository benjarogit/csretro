#!/usr/bin/env bash
# Build the CSRETRO client.
#
# The product client is x86_64. vcpkg uses its x64-linux triplet. The server
# module stays i386 because ReHLDS loads it that way.
#
# Paths with spaces break OpenSSL's Configure and several other port builds.
# When the repository lives under such a path, the script builds through a
# space-free symlink in ${TMPDIR:-/tmp}; the files stay where they are.
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"

CONFIG="Release"
JOBS="$(nproc)"
TARGETS=(client_mini engine_mini nitro_api gameui cstrike.exe BUILD_ALL)
RECONFIGURE=0
CLEAN=0
INSTALL_DIR=""
MANIFEST=""

usage() {
    cat <<'EOF'
usage: scripts/build-client.sh [options]

  --config <name>      CMake configuration (default: Release)
  --jobs <n>           parallel jobs (default: nproc)
  --target <name>      build only this target; repeatable
  --reconfigure        re-run the configure preset
  --clean              remove build/linux before configuring
  --install-dir <dir>  set NEXTCLIENT_INSTALL_DIR and reconfigure
  --manifest <file>    manifest path (default: build/linux/manifest-<config>.txt)
  -h, --help           show this help
EOF
}

while (($# > 0)); do
    case "$1" in
        --config) CONFIG="${2:?missing value}"; shift ;;
        --jobs) JOBS="${2:?missing value}"; shift ;;
        --target) TARGETS=("${2:?missing value}"); shift ;;
        --reconfigure) RECONFIGURE=1 ;;
        --clean) CLEAN=1 ;;
        --install-dir) INSTALL_DIR="${2:?missing value}"; shift ;;
        --manifest) MANIFEST="${2:?missing value}"; shift ;;
        -h|--help) usage; exit 0 ;;
        *) printf 'unknown option: %s\n\n' "$1" >&2; usage >&2; exit 2 ;;
    esac
    shift
done

if [[ "$ROOT" == *" "* ]]; then
    SRC="${TMPDIR:-/tmp}/csretro-src"
    ln -sfn "$ROOT" "$SRC"
    printf 'note: repository path contains spaces, building through %s\n' "$SRC"
else
    SRC="$ROOT"
fi

CLIENT="$SRC/client"
BUILD="$CLIENT/build/linux"

if ((CLEAN)); then
    printf '==> removing %s\n' "$BUILD"
    rm -rf "$BUILD"
fi

configure_args=()
if [[ -n "$INSTALL_DIR" ]]; then
    configure_args+=("-DNEXTCLIENT_INSTALL_DIR=$INSTALL_DIR")
fi

if [[ ! -f "$BUILD/CMakeCache.txt" ]] || ((RECONFIGURE)) || [[ -n "$INSTALL_DIR" ]]; then
    printf '==> configure (preset linux)\n'
    (cd "$CLIENT" && cmake --preset linux "${configure_args[@]}")
fi

started="$(date +%s)"
for target in "${TARGETS[@]}"; do
    printf '\n==> build %s (%s)\n' "$target" "$CONFIG"
    cmake --build "$BUILD" --config "$CONFIG" --target "$target" -j "$JOBS"
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

# Every artifact must be a 64-bit ELF. A 32-bit object here means the client
# was built for the old host ABI.
bad="$(awk -F'\t' '!/^#/ && $3 != "ELF64" { print $4 }' "$manifest")"
if [[ -n "$bad" ]]; then
    printf '\nERROR: not a 64-bit ELF:\n%s\n' "$bad" >&2
    exit 1
fi

printf '\n==> artifacts\n'
awk -F'\t' '!/^#/ { printf "  %10d  %-24s %s\n", $1, $3, $4 }' "$manifest"
printf '\nmanifest: %s\n' "${manifest/#$SRC/$ROOT}"
printf 'build time: %s s\n' "$((finished - started))"
