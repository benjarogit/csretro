# CSRETRO GitHub quality baseline

GitHub quality checks support the existing `AGENTS.md` contract and the authoritative `docs/plan/csretro-plan.md`. They never replace gate acceptance or authorise a release.

## Existing safeguards, preserve

- `.github/workflows/ci.yml` verifies repository rules and multiple Linux client-fixture profiles; green CI is not a playable game or G07 runtime PASS.
- `.github/workflows/pages.yml` publishes the documentation site only after successful main push CI or explicit dispatch. Keep its publication boundary.
- `.github/dependabot.yml` already groups GitHub Actions updates once weekly, capped at one PR. Do not introduce duplicate package/vendor updates or unpinned submodule changes.
- `SECURITY.md` is the canonical security-reporting policy.
- PR #15 is **stacked on PR #14**. Integrate in that order only after explicit review and permission.

## Proposed additions

- Issue and PR templates that retain NextClient, Xash3D, existing pins and DE/EN gates.
- Scoped Python CodeQL for repository tooling, scheduled weekly and on PR/main; verify actual availability and the first scan before requiring the check. This does not cover C++ engine/client code; assess a separate C++ scan after a resource/relevance review.

## GitHub website settings requiring owner/admin controls

1. Rules → Rulesets: protect `main` (no force push/delete; require PR), and choose **only verified stable checks** from CI after checking exact names. Do not prevent a solo maintainer from approving merges.
2. Security: enable dependency graph, Dependabot alerts/security updates, secret scanning, push protection and private vulnerability reporting where supported.
3. Actions → General: default token read-only; restrict untrusted fork PRs and avoid unnecessary write permissions; review which actions can run.
4. Code scanning: verify this CodeQL workflow succeeds before making it mandatory; avoid duplicate CodeQL default setup.
5. CodeRabbit: confirm integration, review content, free public-repository eligibility and check names. AI review is advisory, not an automatic authority.
6. Project work: retain current project manager, source registry and `HANDOFF.md`; do not invent a second progress tracker.

No merge, release, runtime test or Orqen integration is authorised by this setup.
