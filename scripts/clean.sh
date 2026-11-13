#!/usr/bin/env bash
# CSRETRO cleanup.
#
# Removes generated state only. It never touches source files, documentation or
# maintainer-local data under .local/. This is the only sanctioned way to
# remove build output.
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

dry_run=0
client_preset=""
while (($# > 0)); do
    case "$1" in
        --dry-run) dry_run=1 ;;
        --client-build)
            client_preset="${2:?missing client build preset}"
            case "$client_preset" in
                linux|linux-tests|linux-port|linux-port-tests) ;;
                *) printf 'unsupported client build preset: %s\n' "$client_preset" >&2; exit 2 ;;
            esac
            shift ;;
        *) printf 'usage: %s [--dry-run] [--client-build <preset>]\n' "$(basename "$0")" >&2; exit 2 ;;
    esac
    shift
done

# Fixed list of generated paths, relative to the repository root.
targets=(
    build
    out
    install
    dist
    site
    .cache
    vcpkg_installed
    client/build
    client/out
    client/vcpkg_installed
    client/dep/ncl-hl1-source-sdk/build
    server/api/build
    tools/__pycache__
)

remove() {
    local path="$1"
    [[ -e "$path" ]] || return 0
    if ((dry_run)); then
        printf 'would remove %s\n' "$path"
    else
        printf 'removing %s\n' "$path"
        rm -rf -- "$path"
    fi
}

if [[ -n "$client_preset" ]]; then
    remove "client/build/$client_preset"
    exit 0
fi

for target in "${targets[@]}"; do
    remove "$target"
done

# Python caches anywhere in the tree, but never inside .git.
while IFS= read -r -d '' cache; do
    remove "${cache#"$ROOT"/}"
done < <(find . -path ./.git -prune -o -type d -name '__pycache__' -print0 2>/dev/null)

# CMake user presets are machine-local and must not survive a clean.
remove CMakeUserPresets.json

if ((dry_run)); then
    printf 'dry run complete, nothing was removed\n'
else
    printf 'clean complete\n'
fi
