#!/usr/bin/env bash
# CSRETRO repository verification.
#
# Runs the checks that must pass locally and in CI before a commit is accepted.
# It never builds the game and never starts it.
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

fail=0
note() { printf '%s\n' "$*"; }
fail_msg() { printf 'FAIL: %s\n' "$*" >&2; fail=1; }

note "==> repository structure, docs, links, secrets"
if ! python3 tools/verify_repo.py; then
    fail=1
fi

note "==> python syntax"
if ! python3 -m compileall -q tools; then
    fail_msg "python compilation failed"
fi

note "==> shell scripts"
if command -v shellcheck >/dev/null 2>&1; then
    mapfile -t scripts < <(git ls-files '*.sh' 2>/dev/null || find scripts -name '*.sh' -type f)
    if ((${#scripts[@]} > 0)); then
        if ! shellcheck --severity=warning "${scripts[@]}"; then
            fail_msg "shellcheck reported problems"
        fi
    fi
else
    note "    shellcheck not installed, skipping (CI enforces it)"
fi

note "==> line endings"
if git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    if git ls-files --eol | grep -q 'i/crlf'; then
        fail_msg "tracked files contain CRLF line endings"
        git ls-files --eol | grep 'i/crlf' | head -20 >&2
    fi
fi

if ((fail)); then
    note ""
    note "Verification FAILED"
    exit 1
fi

note ""
note "Verification passed"
