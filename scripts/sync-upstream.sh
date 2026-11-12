#!/usr/bin/env bash
# Report upstream changes since the pinned commits.
#
# Read-only: fetches objects, prints what is new, never merges or cherry-picks.
# Use the output to decide whether a pin bump is worth a gate.
#
# Usage:
#   scripts/sync-upstream.sh              # all upstreams
#   scripts/sync-upstream.sh nextclient   # one upstream by lockfile name
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

only="${1:-all}"
limit="${SYNC_LOG_LIMIT:-20}"

mapfile -t rows < <(python3 - <<'PY'
import json
from pathlib import Path

data = json.loads(Path("versions.lock.json").read_text())
for name, entry in data.get("upstreams", {}).items():
    print("\t".join([
        name,
        entry.get("repository", ""),
        entry.get("prefix", "").rstrip("/"),
        entry.get("commit") or "-",
        entry.get("method", ""),
        entry.get("branch", "main"),
    ]))
PY
)

status=0
for row in "${rows[@]}"; do
    IFS=$'\t' read -r name repo prefix pin method branch <<<"$row"
    [[ "$only" == "all" || "$only" == "$name" ]] || continue

    printf '\n== %s (%s, %s)\n' "$name" "$method" "$prefix"

    if [[ -z "$repo" ]]; then
        printf '   no repository recorded\n' >&2
        status=1
        continue
    fi

    if [[ "$method" == "git-subtree" ]]; then
        if [[ "$pin" == "-" ]]; then
            printf '   no pin recorded\n' >&2
            status=1
            continue
        fi
        if ! git fetch --no-tags --quiet "$repo" "$branch"; then
            printf '   fetch failed\n' >&2
            status=1
            continue
        fi
        printf '   pinned: %s\n' "$pin"
        printf '   head:   %s\n' "$(git rev-parse --short FETCH_HEAD)"
        commits="$(git log --oneline "$pin"..FETCH_HEAD | head -n "$limit" || true)"
        if [[ -z "$commits" ]]; then
            printf '   up to date\n'
        else
            printf '%s\n' "$commits" | sed 's/^/   /'
            printf '   ... see `git log %s..FETCH_HEAD` for the full list\n' "$pin"
        fi
    elif [[ "$method" == "git-submodule" || "$method" == "vendored-tree" ]]; then
        # vendored-tree is an in-repo snapshot. The pin is still the upstream base.
        remote_head="$(git ls-remote "$repo" HEAD | awk '{print $1}')"
        printf '   pinned: %s\n' "$pin"
        printf '   head:   %s\n' "${remote_head:0:40}"
        [[ "$pin" == "$remote_head" ]] && printf '   up to date\n'
    else
        printf '   unknown method: %s\n' "$method" >&2
        status=1
    fi
done

exit "$status"
