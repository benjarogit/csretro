#!/usr/bin/env bash
# Weekly upstream radar: open or refresh at most one Issue. Never changes pins.
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

if ! command -v gh >/dev/null 2>&1; then
  echo "gh is required" >&2
  exit 1
fi

report="$(mktemp)"
trap 'rm -f "$report"' EXIT

set +e
./scripts/sync-upstream.sh >"$report" 2>&1
set -e

python3 - "$report" <<'PY'
import json
import re
import subprocess
import sys
from pathlib import Path

report = Path(sys.argv[1]).read_text(encoding="utf-8", errors="replace")
lock = json.loads(Path("versions.lock.json").read_text(encoding="utf-8"))

behind: list[str] = []
section: str | None = None
has_commits = False
for line in report.splitlines():
    match = re.match(r"^== (\S+)", line)
    if match:
        if has_commits and section and section not in behind:
            behind.append(section)
        section = match.group(1)
        has_commits = False
        continue
    if "up to date" in line:
        has_commits = False
        continue
    if section and re.match(r"^\s+[0-9a-f]{7,}\s+\S", line):
        has_commits = True
if has_commits and section and section not in behind:
    behind.append(section)

if not behind:
    print("all tracked upstreams up to date; no issue")
    raise SystemExit(0)

title = "Upstream watch: pins behind remote"
body_lines = [
    "Automated weekly check. **Do not merge from this issue.**",
    "",
    "A human chooses per find: leave it, refresh the pin, or rebuild as `patches/`.",
    "See `docs/plan/csretro-plan.md` section 11.4.",
    "",
    "Behind:",
]
for name in behind:
    entry = lock.get("upstreams", {}).get(name, {})
    body_lines.append(f"- `{name}` pin `{entry.get('commit', '?')}`")
body_lines.extend(["", "```text", report.strip(), "```", ""])
body = "\n".join(body_lines)

listed = subprocess.check_output(
    [
        "gh",
        "issue",
        "list",
        "--state",
        "open",
        "--search",
        f'in:title "{title}"',
        "--json",
        "number,title",
    ],
    text=True,
)
issues = json.loads(listed)
if issues:
    number = issues[0]["number"]
    subprocess.check_call(["gh", "issue", "comment", str(number), "--body", body])
    print(f"updated issue #{number}")
else:
    subprocess.check_call(["gh", "issue", "create", "--title", title, "--body", body])
    print("opened one upstream watch issue")
PY
