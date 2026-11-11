#!/usr/bin/env bash
# CSRETRO bootstrap.
#
# Prepares a fresh clone: hooks, Git LFS, submodules, vcpkg. Idempotent, safe to
# run repeatedly.
#
# By default it does not install system packages and never touches the retail
# game. Pass --install-deps to install the Arch/CachyOS packages listed in
# docs/*/build/prerequisites.md.
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

install_deps=0
run_verify=1

usage() {
    cat <<'EOF'
usage: scripts/bootstrap.sh [options]

  --install-deps   install missing Arch/CachyOS packages (needs sudo)
  --no-verify      skip the final verification run
  -h, --help       show this help
EOF
}

while (($# > 0)); do
    case "$1" in
        --install-deps) install_deps=1 ;;
        --no-verify) run_verify=0 ;;
        -h|--help) usage; exit 0 ;;
        *) printf 'unknown option: %s\n\n' "$1" >&2; usage >&2; exit 2 ;;
    esac
    shift
done

note() { printf '==> %s\n' "$*"; }

if ((install_deps)); then
    note "system packages"
    if ! command -v pacman >/dev/null 2>&1; then
        printf 'automatic installation is only implemented for Arch/CachyOS.\n' >&2
        printf 'install the packages listed in docs/en/build/prerequisites.md manually.\n' >&2
        exit 2
    fi

    packages=(
        base-devel
        cmake
        ninja
        python
        uv
        git
        git-lfs
        shellcheck
        curl
        zip
        unzip
        tar
        patch
        pkgconf
        perl
        nasm
        lib32-glibc
        lib32-gcc-libs
        lib32-mesa
        lib32-libglvnd
        lib32-libx11
        lib32-libxext
        lib32-libxcursor
        lib32-libxi
        lib32-libxrandr
        lib32-libxinerama
        lib32-libxss
        lib32-libxfixes
        lib32-libxrender
        mesa
        libglvnd
        libx11
        libxext
        libxcursor
        libxi
        libxrandr
        libxinerama
        libxss
        libxfixes
        libxrender
    )

    sudo pacman -S --needed --noconfirm "${packages[@]}"
fi

note "git hooks"
git config core.hooksPath .githooks
chmod +x .githooks/pre-push scripts/*.sh tools/*.py

note "git lfs"
if command -v git-lfs >/dev/null 2>&1; then
    # Do not run `git lfs install`: it would replace the tracked pre-push hook in
    # .githooks/. Only the filter configuration is needed; the hook forwards to
    # `git lfs pre-push` itself.
    git config --local filter.lfs.clean "git-lfs clean -- %f"
    git config --local filter.lfs.smudge "git-lfs smudge -- %f"
    git config --local filter.lfs.process "git-lfs filter-process"
    git config --local filter.lfs.required true
else
    printf 'git-lfs is not installed; binary assets will stay as pointers\n' >&2
fi

note "submodules"
git submodule update --init --recursive

note "vcpkg"
if [[ -x client/vcpkg/bootstrap-vcpkg.sh ]]; then
    client/vcpkg/bootstrap-vcpkg.sh -disableMetrics
else
    printf 'vcpkg submodule is missing; skipping bootstrap\n' >&2
fi

if command -v git-lfs >/dev/null 2>&1; then
    note "lfs objects"
    git lfs pull
fi

if ((run_verify)); then
    note "verification"
    ./scripts/verify.sh
fi
