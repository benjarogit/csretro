#!/usr/bin/env python3
"""Repository verification for CSRETRO.

Checks the invariants that must hold on every commit and in CI:

* required top-level files exist and are non-empty
* VERSION is a valid semantic version
* versions.lock.json parses and has the expected shape
* German and English documentation mirror each other
* relative Markdown links resolve
* no secrets are tracked
* no generated or maintainer-local paths are tracked

Exit code 0 means everything passed. Exit code 1 means at least one check
failed and the failures were printed.

The tool intentionally has no third-party dependencies so that it runs on a
bare Python 3.12+ installation and in CI without a setup step.
"""

from __future__ import annotations

import hashlib
import json
import re
import subprocess
import sys
from pathlib import Path
from urllib.parse import unquote

ROOT = Path(__file__).resolve().parents[1]

REQUIRED_FILES = [
    "AGENTS.md",
    "CHANGELOG.md",
    "CREDITS.md",
    "HANDOFF.md",
    "README.md",
    "README.de.md",
    "SECURITY.md",
    "VERSION",
    "versions.lock.json",
    "docs/plan/csretro-plan.md",
    "docs/de/index.md",
    "docs/en/index.md",
    "zensical.toml",
]

VERSION_RE = re.compile(r"^\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?$")

FORBIDDEN_TRACKED_PREFIXES = (
    ".local/",
    "build/",
    "out/",
    "install/",
    "dist/",
    ".cache/",
    "__pycache__/",
    "vcpkg_installed/",
    "node_modules/",
    ".pytest_cache/",
)

FORBIDDEN_TRACKED_NAMES = {
    "CMakeUserPresets.json",
    ".env",
    "compile_commands.json",
}

FORBIDDEN_TRACKED_SUFFIXES = (
    ".log",
    ".pyc",
    ".pyo",
    ".tmp",
    ".swp",
)

# Three files under the vendored cpr test data are intentionally named *.key.
# They are public test fixtures, not credentials. They are allowlisted by path
# rather than disabling the scan for the whole vendored tree, so a real secret
# added anywhere else is still caught.
SECRET_ALLOWLIST = (
    "client/dep/cpr/test/data/keys/",
)

SECRET_PATTERNS = [
    ("private key block", re.compile(r"-----BEGIN (?:RSA |EC |OPENSSH |DSA )?PRIVATE KEY-----")),
    ("GitHub token", re.compile(r"\bgh[pousr]_[A-Za-z0-9]{20,}\b")),
    ("GitHub fine-grained token", re.compile(r"\bgithub_pat_[A-Za-z0-9_]{20,}\b")),
    ("AWS access key", re.compile(r"\bAKIA[0-9A-Z]{16}\b")),
    ("OpenAI-style key", re.compile(r"\bsk-[A-Za-z0-9]{32,}\b")),
    ("Slack token", re.compile(r"\bxox[baprs]-[A-Za-z0-9-]{10,}\b")),
    ("Discord webhook", re.compile(r"https://(?:ptb\.|canary\.)?discord(?:app)?\.com/api/webhooks/\d+/[\w-]+")),
    ("Sentry DSN", re.compile(r"https://[0-9a-f]{32}@[\w.-]+/\d+")),
]

# Values that look like a secret assignment but are documented placeholders.
PLACEHOLDER_HINTS = (
    "<",
    "example",
    "placeholder",
    "changeme",
    "your-",
    "your_",
    "xxx",
    "redacted",
    "dummy",
)

LINK_RE = re.compile(r"(?<!!)\[[^\]]*\]\(\s*(<[^>]*>|[^)\s]+)(?:\s+[\"'][^\"']*[\"'])?\s*\)")
FENCE_RE = re.compile(r"^\s*(```|~~~)")


class Report:
    def __init__(self) -> None:
        self.failures: list[str] = []
        self.checks = 0

    def check(self, condition: bool, message: str) -> None:
        self.checks += 1
        if not condition:
            self.failures.append(message)

    def fail(self, message: str) -> None:
        self.checks += 1
        self.failures.append(message)


def tracked_files() -> list[str]:
    """Return repository-relative tracked files, or all files outside git."""
    try:
        result = subprocess.run(
            ["git", "ls-files", "-z"],
            cwd=ROOT,
            check=True,
            capture_output=True,
        )
        names = [n for n in result.stdout.decode("utf-8", "replace").split("\0") if n]
        if names:
            return sorted(names)
    except (OSError, subprocess.CalledProcessError):
        pass

    names = []
    for path in ROOT.rglob("*"):
        if not path.is_file():
            continue
        rel = path.relative_to(ROOT).as_posix()
        if rel.startswith(".git/"):
            continue
        names.append(rel)
    return sorted(names)


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8", errors="replace")


def check_required_files(report: Report) -> None:
    for rel in REQUIRED_FILES:
        path = ROOT / rel
        report.check(path.is_file(), f"missing required file: {rel}")
        if path.is_file():
            report.check(path.stat().st_size > 0, f"required file is empty: {rel}")


def check_version(report: Report) -> None:
    path = ROOT / "VERSION"
    if not path.is_file():
        return
    value = read_text(path).strip()
    report.check(bool(VERSION_RE.match(value)), f"VERSION is not a semantic version: {value!r}")


def check_lockfile(report: Report) -> None:
    path = ROOT / "versions.lock.json"
    if not path.is_file():
        return
    try:
        data = json.loads(read_text(path))
    except json.JSONDecodeError as exc:
        report.fail(f"versions.lock.json is not valid JSON: {exc}")
        return

    report.check(isinstance(data, dict), "versions.lock.json must contain an object")
    if not isinstance(data, dict):
        return

    for key in ("schema_version", "project", "upstreams", "toolchain", "game_builds"):
        report.check(key in data, f"versions.lock.json is missing key: {key}")

    upstreams = data.get("upstreams", {})
    if isinstance(upstreams, dict):
        for name, entry in upstreams.items():
            report.check(isinstance(entry, dict), f"upstream {name} must be an object")
            if not isinstance(entry, dict):
                continue
            for key in ("repository", "prefix", "method", "gate"):
                report.check(key in entry, f"upstream {name} is missing key: {key}")


def check_docs_parity(report: Report) -> None:
    de = ROOT / "docs" / "de"
    en = ROOT / "docs" / "en"
    if not de.is_dir() or not en.is_dir():
        report.fail("docs/de and docs/en must both exist")
        return

    de_files = {p.relative_to(de).as_posix() for p in de.rglob("*.md")}
    en_files = {p.relative_to(en).as_posix() for p in en.rglob("*.md")}

    for rel in sorted(de_files - en_files):
        report.fail(f"German document has no English counterpart: docs/de/{rel}")
    for rel in sorted(en_files - de_files):
        report.fail(f"English document has no German counterpart: docs/en/{rel}")


def markdown_links(text: str) -> list[str]:
    links = []
    in_fence = False
    for line in text.splitlines():
        if FENCE_RE.match(line):
            in_fence = not in_fence
            continue
        if in_fence:
            continue
        for match in LINK_RE.finditer(line):
            target = match.group(1).strip()
            if target.startswith("<") and target.endswith(">"):
                target = target[1:-1].strip()
            links.append(target)
    return links


def check_links(report: Report, files: list[str]) -> None:
    for rel in files:
        if not rel.endswith(".md"):
            continue
        path = ROOT / rel
        if not path.is_file():
            continue
        for target in markdown_links(read_text(path)):
            if not target:
                continue
            if target.startswith(("http://", "https://", "mailto:", "#")):
                continue
            if "{{" in target or target.startswith("/"):
                continue

            clean = unquote(target.split("#", 1)[0].split("?", 1)[0])
            if not clean:
                continue

            resolved = (path.parent / clean).resolve()
            try:
                resolved.relative_to(ROOT.resolve())
            except ValueError:
                report.fail(f"{rel}: link escapes the repository: {target}")
                continue
            if not resolved.exists():
                report.fail(f"{rel}: broken link: {target}")


def is_placeholder(value: str) -> bool:
    lowered = value.lower()
    return any(hint in lowered for hint in PLACEHOLDER_HINTS)


def check_secrets(report: Report, files: list[str]) -> None:
    for rel in files:
        if rel.startswith(SECRET_ALLOWLIST):
            continue
        path = ROOT / rel
        if not path.is_file() or path.stat().st_size > 2_000_000:
            continue
        try:
            text = path.read_text(encoding="utf-8")
        except (UnicodeDecodeError, OSError):
            continue
        for name, pattern in SECRET_PATTERNS:
            for match in pattern.finditer(text):
                if is_placeholder(match.group(0)):
                    continue
                line = text.count("\n", 0, match.start()) + 1
                report.fail(f"{rel}:{line}: possible {name}")


def check_forbidden_paths(report: Report, files: list[str]) -> None:
    for rel in files:
        if rel.startswith(FORBIDDEN_TRACKED_PREFIXES):
            report.fail(f"generated or local path is tracked: {rel}")
            continue
        name = rel.rsplit("/", 1)[-1]
        if name in FORBIDDEN_TRACKED_NAMES:
            report.fail(f"forbidden file is tracked: {rel}")
            continue
        if rel.endswith(FORBIDDEN_TRACKED_SUFFIXES):
            report.fail(f"generated or temporary file is tracked: {rel}")


def check_pins(report: Report) -> None:
    """Compare recorded pins with what is actually checked in.

    Uses `git ls-tree HEAD`, so it works in CI without submodule checkout.
    """
    path = ROOT / "versions.lock.json"
    if not path.is_file():
        return
    try:
        data = json.loads(read_text(path))
    except json.JSONDecodeError:
        return

    upstreams = data.get("upstreams", {})
    if not isinstance(upstreams, dict):
        return

    for name, entry in upstreams.items():
        if not isinstance(entry, dict) or not entry.get("vendored"):
            continue
        prefix = str(entry.get("prefix", "")).rstrip("/")
        expected = entry.get("commit")
        if not prefix:
            report.fail(f"upstream {name}: vendored entry has no prefix")
            continue

        try:
            result = subprocess.run(
                ["git", "ls-tree", "HEAD", prefix],
                cwd=ROOT,
                check=True,
                capture_output=True,
                text=True,
            )
        except (OSError, subprocess.CalledProcessError):
            report.fail(f"upstream {name}: cannot read git tree for {prefix}")
            continue

        line = result.stdout.strip()
        if not line:
            report.fail(f"upstream {name}: vendored path is missing: {prefix}")
            continue

        fields = line.split()
        if entry.get("method") == "git-submodule":
            if len(fields) < 3 or fields[0] != "160000":
                report.fail(f"upstream {name}: {prefix} is not a gitlink")
            elif expected and fields[2] != expected:
                report.fail(
                    f"upstream {name}: gitlink is {fields[2]}, lock records {expected}"
                )
        elif expected and entry.get("method") == "git-subtree":
            # Subtree content is not a single object, so only presence is checked.
            report.check(fields[0] == "040000", f"upstream {name}: {prefix} is not a tree")


def check_spent_plans(report: Report, files: list[str]) -> None:
    for rel in files:
        if rel.endswith(".plan.md"):
            report.fail(f"spent work plan is tracked: {rel}")


def check_docs_template(report: Report) -> None:
    path = ROOT / "zensical.toml"
    if not path.is_file():
        return
    text = read_text(path)
    report.check("csretro-vgui" not in text, "zensical.toml still loads the custom vgui skin")
    report.check("vgui-skin.js" not in text, "zensical.toml still loads the vgui skin script")


def check_server_vendor(report: Report) -> None:
    path = ROOT / "versions.lock.json"
    if not path.is_file():
        return
    try:
        data = json.loads(read_text(path))
    except json.JSONDecodeError:
        return

    for section in ("server_toolchain", "server_runtime"):
        entries = data.get(section, {})
        if not isinstance(entries, dict):
            continue
        for name, entry in entries.items():
            if not isinstance(entry, dict) or not entry.get("vendored"):
                continue
            archive = entry.get("archive")
            if not archive:
                report.fail(f"{section}.{name}: vendored entry has no archive")
            else:
                archive_path = ROOT / str(archive)
                if not archive_path.is_file():
                    report.fail(f"{section}.{name}: vendored archive missing: {archive}")
                else:
                    digest = hashlib.sha256(archive_path.read_bytes()).hexdigest()
                    expected = entry.get("sha256")
                    if expected and digest != expected:
                        report.fail(
                            f"{section}.{name}: archive sha256 is {digest}, lock records {expected}"
                        )
            prefix = entry.get("source_prefix")
            if prefix and not (ROOT / str(prefix)).is_dir():
                report.fail(f"{section}.{name}: vendored source missing: {prefix}")

    for rel in ("docs/de/build/server.md", "docs/en/build/server.md"):
        doc = ROOT / rel
        if not doc.is_file():
            report.fail(f"missing server build doc: {rel}")
            continue
        text = read_text(doc)
        report.check(
            "server/vendor/amxmodx" in text,
            f"{rel} does not name the vendored AMXX source",
        )
        report.check(
            "nicht vendort" not in text and "not vendored" not in text,
            f"{rel} still says the Pawn toolchain is not vendored",
        )


def check_sdk_patch(report: Report) -> None:
    patch = ROOT / "client/dep/ncl-hl1-source-sdk.patch"
    sdk = ROOT / "client/dep/ncl-hl1-source-sdk"
    if not patch.is_file() or patch.stat().st_size == 0:
        report.fail("client/dep/ncl-hl1-source-sdk.patch is missing")
        return
    report.check(True, "sdk patch is present")
    if not (sdk / ".git").exists():
        return
    reverse = subprocess.run(
        ["git", "apply", "--reverse", "--check", str(patch)],
        cwd=sdk,
        capture_output=True,
        text=True,
        check=False,
    )
    if reverse.returncode == 0:
        return
    forward = subprocess.run(
        ["git", "apply", "--check", str(patch)],
        cwd=sdk,
        capture_output=True,
        text=True,
        check=False,
    )
    report.check(
        forward.returncode == 0,
        "ncl-hl1-source-sdk patch does not match the checked-out tree",
    )


def check_changelog(report: Report) -> None:
    path = ROOT / "CHANGELOG.md"
    if not path.is_file():
        return
    text = read_text(path)
    report.check("## [Unreleased]" in text, "CHANGELOG.md is missing an [Unreleased] section")


def main() -> int:
    report = Report()
    files = tracked_files()

    check_required_files(report)
    check_version(report)
    check_lockfile(report)
    check_pins(report)
    check_docs_parity(report)
    check_links(report, files)
    check_secrets(report, files)
    check_forbidden_paths(report, files)
    check_spent_plans(report, files)
    check_docs_template(report)
    check_server_vendor(report)
    check_sdk_patch(report)
    check_changelog(report)

    if report.failures:
        print(f"FAIL: {len(report.failures)} problem(s) in {report.checks} checks", file=sys.stderr)
        for failure in report.failures:
            print(f"  - {failure}", file=sys.stderr)
        return 1

    print(f"OK: {report.checks} checks passed on {len(files)} tracked files")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
