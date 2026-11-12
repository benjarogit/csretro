#!/usr/bin/env bash
# Build the public documentation site with the pinned Zensical wheel.
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

python3 - <<'PY'
import hashlib
import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path

root = Path(".").resolve()
lock = json.loads((root / "versions.lock.json").read_text(encoding="utf-8"))
site = lock["docs_site"]["zensical"]
version = site["version"]
expected = site["sha256_manylinux_x86_64"]
url = site["url_manylinux_x86_64"]

with tempfile.TemporaryDirectory(prefix="zensical-wheel-") as tmp:
    tmp_path = Path(tmp)
    wheel = tmp_path / Path(url).name
    subprocess.check_call(["curl", "-fsSL", url, "-o", str(wheel)])
    digest = hashlib.sha256(wheel.read_bytes()).hexdigest()
    if digest != expected:
        raise SystemExit(
            f"zensical wheel sha256 mismatch: got {digest}, expected {expected}"
        )
    venv = tmp_path / "venv"
    subprocess.check_call([sys.executable, "-m", "venv", str(venv)])
    pip = venv / "bin" / "pip"
    zensical = venv / "bin" / "zensical"
    # Install the verified wheel; pip resolves zensical-ui from PyPI.
    subprocess.check_call([str(pip), "install", "--quiet", str(wheel)])
    env = os.environ.copy()
    env["UV_LINK_MODE"] = "copy"
    subprocess.check_call([str(zensical), "build", "--strict"], env=env)

print("docs site built into site/")
PY
