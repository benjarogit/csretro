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

if command -v git-lfs >/dev/null 2>&1; then
    note "lfs objects"
    git lfs pull
fi

note "verification"
./scripts/verify.sh
