#!/usr/bin/env bash
# Copy the Counter-Strike 1.6 files the client mounts.
# Does not start Steam and does not start the game.
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
exec python3 "$ROOT/tools/install_content.py" "$@"
