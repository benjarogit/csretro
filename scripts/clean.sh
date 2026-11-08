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
if [[ "${1:-}" == "--dry-run" ]]; then
    dry_run=1
elif [[ $# -gt 0 ]]; then
    printf 'usage: %s [--dry-run]\n' "$(basename "$0")" >&2
    exit 2
fi

# Fixed list of generated paths, relative to the repository root.
targets=(
    build
    out
    install
    dist
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
