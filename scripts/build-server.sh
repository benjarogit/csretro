#!/usr/bin/env bash
# Build the CSRETRO server API (AMX Mod X module).
#
# The module is a 32-bit i386 shared object that ReHLDS loads through Metamod.
# Its dependencies (kangaru, sigslot, magic_enum) are pinned and fetched by
# CMake; the Pawn toolchain is downloaded and hash-checked by
# scripts/fetch-server-toolchain.sh.
#
# Paths with spaces are handled the same way as for the client: the build runs
# through a space-free symlink in ${TMPDIR:-/tmp}.
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"

CONFIG="Release"
JOBS="$(nproc)"
TARGETS=()
RECONFIGURE=0
CLEAN=0
RUN_TESTS=1
MANIFEST=""
STAGE=""

usage() {
    cat <<'EOF'
usage: scripts/build-server.sh [options]

  --config <name>    CMake configuration (default: Release)
  --jobs <n>         parallel jobs (default: nproc)
  --target <name>    build only this target; repeatable
  --reconfigure      re-run the configure step
  --clean            remove server/api/build/linux before configuring
  --no-tests         skip configure and run of the CTest suite
  --manifest <file>  manifest path (default: server/api/build/linux/manifest-<config>.txt)
  --stage <dir>      assemble a Docker build stage: runtime archives, the module
                     and the compiled test plugin, plus a staging manifest
  -h, --help         show this help

Environment:
  nextclientapi_amxx_COPY_TO_PATH  copy the built module there after the build
EOF
}

while (($# > 0)); do
    case "$1" in
        --config) CONFIG="${2:?missing value}"; shift ;;
        --jobs) JOBS="${2:?missing value}"; shift ;;
        --target) TARGETS+=("${2:?missing value}"); shift ;;
        --reconfigure) RECONFIGURE=1 ;;
        --clean) CLEAN=1 ;;
        --no-tests) RUN_TESTS=0 ;;
        --manifest) MANIFEST="${2:?missing value}"; shift ;;
        --stage) STAGE="${2:?missing value}"; shift ;;
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

API="$SRC/server/api"
BUILD="$API/build/linux"

if ((CLEAN)); then
    printf '==> removing %s\n' "$BUILD"
    rm -rf "$BUILD"
fi

configure_args=(
    -S "$API"
    -B "$BUILD"
    -G Ninja
    "-DCMAKE_BUILD_TYPE=$CONFIG"
)

if ((RUN_TESTS)) || [[ -n "$STAGE" ]]; then
    printf '==> fetching Pawn toolchain\n'
    eval "$("$ROOT/scripts/fetch-server-toolchain.sh")"
fi

if ((RUN_TESTS)); then
    configure_args+=(
        -DNEXTCLIENTAPI_BUILD_TESTS=ON
        "-DNEXTCLIENTAPI_PAWN_COMPILER=$AMXXPC"
        "-DNEXTCLIENTAPI_PAWN_INCLUDE_DIR=$PAWN_INCLUDE_DIR"
    )
else
    configure_args+=(-DNEXTCLIENTAPI_BUILD_TESTS=OFF)
fi

if [[ ! -f "$BUILD/CMakeCache.txt" ]] || ((RECONFIGURE)); then
    printf '==> configure\n'
    cmake "${configure_args[@]}"
fi

started="$(date +%s)"
if ((${#TARGETS[@]} > 0)); then
    for target in "${TARGETS[@]}"; do
        printf '\n==> build %s (%s)\n' "$target" "$CONFIG"
        cmake --build "$BUILD" --config "$CONFIG" --target "$target" -j "$JOBS"
    done
else
    printf '\n==> build (%s)\n' "$CONFIG"
    cmake --build "$BUILD" --config "$CONFIG" -j "$JOBS"
fi
finished="$(date +%s)"

if ((RUN_TESTS)) && ((${#TARGETS[@]} == 0)); then
    printf '\n==> ctest\n'
    ctest --test-dir "$BUILD" --output-on-failure
fi

module="$(find "$BUILD" -maxdepth 1 -type f -name '*_i386.so' -print -quit)"
if [[ -z "$module" ]]; then
    printf 'ERROR: no *_i386.so found in %s\n' "$BUILD" >&2
    exit 1
fi

elf="$(readelf -h "$module" 2>/dev/null | awk -F: '/Class/{gsub(/ /,"",$2); print $2}')"
if [[ "$elf" != "ELF32" ]]; then
    printf 'ERROR: %s is %s, expected ELF32\n' "$module" "${elf:-unknown}" >&2
    exit 1
fi

# Module metadata is generated from server/api/CMakeLists.txt. Read the expected
# values there instead of repeating them here.
module_name="$(sed -n 's/^set(AMXX_MODULE_NAME[[:space:]]*"\(.*\)").*/\1/p' "$ROOT/server/api/CMakeLists.txt")"
module_version="$(sed -n 's/^project([^)]*VERSION[[:space:]]*\([0-9.]*\).*/\1/p' "$ROOT/server/api/CMakeLists.txt")"
module_tag="$(sed -n 's/^set(AMXX_MODULE_LOG_TAG[[:space:]]*"\(.*\)").*/\1/p' "$ROOT/server/api/CMakeLists.txt")"

missing_metadata=()
module_strings="$(strings "$module")"
for expected in "$module_name" "$module_version" "$module_tag"; do
    [[ -z "$expected" ]] && continue
    if ! grep -Fxq "$expected" <<<"$module_strings"; then
        missing_metadata+=("$expected")
    fi
done
if ((${#missing_metadata[@]} > 0)); then
    printf 'ERROR: module metadata missing from %s: %s\n' "$module" "${missing_metadata[*]}" >&2
    exit 1
fi

version="$(cat "$ROOT/VERSION" 2>/dev/null || echo unknown)"
commit="$(git -C "$ROOT" rev-parse HEAD 2>/dev/null || echo unknown)"
manifest="${MANIFEST:-$BUILD/manifest-$CONFIG.txt}"

{
    printf '# CSRETRO server API build manifest\n'
    printf '# version: %s\n' "$version"
    printf '# commit: %s\n' "$commit"
    printf '# config: %s\n' "$CONFIG"
    printf '# date: %s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
    printf '# build_seconds: %s\n' "$((finished - started))"
    printf '# module_name: %s\n' "$module_name"
    printf '# module_version: %s\n' "$module_version"
    printf '# module_log_tag: %s\n' "$module_tag"
    printf '# needed: %s\n' "$(readelf -d "$module" | awk -F'[][]' '/NEEDED/{printf "%s ", $2}')"
    printf '# size_bytes\tsha256\telf\tpath\n'
    find "$BUILD" -maxdepth 1 -type f \( -name '*.so' -o -name '*-tests' \) -print0 |
        sort -z |
        while IFS= read -r -d '' file; do
            sha="$(sha256sum "$file" | awk '{print $1}')"
            size="$(stat -c '%s' "$file")"
            class="$(readelf -h "$file" 2>/dev/null | awk -F: '/Class/{gsub(/ /,"",$2); print $2}')"
            printf '%s\t%s\t%s\t%s\n' "$size" "$sha" "$class" "${file#"$BUILD"/}"
        done
} >"$manifest"

printf '\n==> artifacts\n'
awk -F'\t' '!/^#/ { printf "  %10d  %-7s %s\n", $1, $3, $4 }' "$manifest"
printf '\nmodule: %s %s (%s)\n' "$module_name" "$module_version" "$module_tag"
printf 'manifest: %s\n' "${manifest/#$SRC/$ROOT}"
printf 'build time: %s s\n' "$((finished - started))"

if [[ -n "$STAGE" ]]; then
    printf '\n==> staging test server context in %s\n' "$STAGE"
    rm -rf "$STAGE"
    mkdir -p "$STAGE/archives" "$STAGE/module" "$STAGE/plugins" "$STAGE/assets"

    runtime_env="$("$ROOT/scripts/fetch-server-runtime.sh" --stage "$STAGE/archives")"
    steamcmd_image="$(sed -n 's/^RUNTIME_STEAMCMD_IMAGE=//p' <<<"$runtime_env")"
    cp -f "$module" "$STAGE/module/"

    # The test plugin precaches custom models, sounds and sprites; a missing
    # asset aborts the map load with a fatal engine error.
    cp -a "$ROOT/server/api/amxx_test/models" "$ROOT/server/api/amxx_test/sound" \
        "$ROOT/server/api/amxx_test/sprites" "$ROOT/server/api/amxx_test/materials" "$STAGE/assets/"

    printf '==> compiling test plugin\n'
    plugin="$STAGE/plugins/test_nextclient_api.amxx"
    pawn_source="$ROOT/server/api/amxx_test/addons/amxmodx/scripting/test_nextclient_api.sma"
    api_include="$ROOT/server/api/addons/amxmodx/scripting/include"
    if ! pawn_log="$("$AMXXPC" "$pawn_source" "-i$api_include" "-i$PAWN_INCLUDE_DIR" "-o$plugin" 2>&1)"; then
        printf '%s\n' "$pawn_log" >&2
        printf 'ERROR: test plugin compilation failed\n' >&2
        exit 1
    fi
    if grep -q 'warning [0-9]*:' <<<"$pawn_log"; then
        printf '%s\n' "$pawn_log" >&2
        printf 'ERROR: test plugin compilation produced warnings\n' >&2
        exit 1
    fi
    if [[ ! -f "$plugin" ]]; then
        printf 'ERROR: test plugin not found at %s\n' "$plugin" >&2
        exit 1
    fi

    staging_manifest="$STAGE/staging-manifest.txt"
    {
        printf '# CSRETRO test server staging manifest\n'
        printf '# version: %s\n' "$version"
        printf '# commit: %s\n' "$commit"
        printf '# date: %s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
        printf '# steamcmd_image: %s\n' "$steamcmd_image"
        printf '# size_bytes\tsha256\tpath\n'
        find "$STAGE" -type f ! -name 'staging-manifest.txt' -print0 |
            sort -z |
            while IFS= read -r -d '' file; do
                printf '%s\t%s\t%s\n' \
                    "$(stat -c '%s' "$file")" \
                    "$(sha256sum "$file" | awk '{print $1}')" \
                    "${file#"$STAGE"/}"
            done
    } >"$staging_manifest"

    printf '\nstaging manifest: %s\n' "$staging_manifest"
    awk -F'\t' '!/^#/ { printf "  %10d  %s\n", $1, $3 }' "$staging_manifest"
fi
