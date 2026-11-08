#!/usr/bin/env bash
# CSRETRO bootstrap.
#
# Prepares a fresh clone: hooks, Git LFS, submodules. Idempotent, safe to run
# repeatedly. It never installs packages and never touches the retail game.
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

note() { printf '==> %s\n' "$*"; }

note "git hooks"
git config core.hooksPath .githooks
chmod +x .githooks/pre-push scripts/*.sh tools/*.py

note "git lfs"
if command -v git-lfs >/dev/null 2>&1; then
    git lfs install --local
else
    printf 'git-lfs is not installed; binary assets will stay as pointers\n' >&2
fi

note "submodules"
git submodule update --init --recursive

if command -v git-lfs >/dev/null 2>&1; then
    note "lfs objects"
    git lfs pull
fi

note "verification"
./scripts/verify.sh
