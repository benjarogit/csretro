#!/usr/bin/env bash
# Project-local orchestration entry point, independent of the caller's cwd.
set -euo pipefail
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
exec python3 "$ROOT/tools/chatdev.py" "$@"
