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
    "VERSION",
    "versions.lock.json",
    "docs/plan/csretro-plan.md",
    "docs/de/index.md",
    "docs/en/index.md",
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
    check_docs_parity(report)
    check_links(report, files)
    check_secrets(report, files)
    check_forbidden_paths(report, files)
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
