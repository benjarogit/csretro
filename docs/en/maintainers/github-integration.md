# Protected GitHub integration

Normal development uses native GitHub squash auto-merge after the central
[policy](../../../.github/merge-policy.json) passes. The existing NextClient to
Xash3D port remains the development objective. No game starts automatically.

The gate reads live protection, exact-head CI/security checks, CodeRabbit approval,
all review conversations and a current-head acceptance declaration. Skipped,
neutral, missing or stale evidence does not pass. Protection applies to admins,
requires an up-to-date branch, independent approval and resolved conversations,
and disallows force pushes/deletion. Existing tests remain in the existing CI;
the coordinator runs trusted main code only, never privileged PR code.

Include this JSON comment in the PR description after its final push, replacing
the placeholders with the full head SHA and actual criteria/evidence:

```text
<!-- chatdev-acceptance {"head":"FULL_HEAD_SHA","criteria":[{"criterion":"Acceptance criterion","evidence":"CI/test/report link","passed":true}],"human_runtime_required":false} -->
```

The independent approving review by CodeRabbit or an authorized maintainer must
also contain a separate `CSRETRO-ACCEPTANCE DIGEST` line. DIGEST is SHA-256 of the
declaration JSON encoded with Python `json.dumps(sort_keys=True, separators=(',', ':'))`.
Changing the declaration invalidates the attestation even when the head stays
unchanged. Public stranger reviews cannot attest acceptance.

CodeRabbit must review these claims. If a human gameplay acceptance is needed,
keep the PR held until that evidence exists; never declare it unnecessary merely
to pass. A text declaration alone is not independent verification.

Releases, production deployments, paid operations, permission changes and other
security-sensitive changes require explicit current-head maintainer approval.
Sensitive paths/labels are centrally listed. Besides an independent maintainer
review, an authorized maintainer may explicitly comment
`CSRETRO-APPROVE-SENSITIVE FULL_HEAD_SHA`. This is scoped to that immutable head;
labels or an agent's inferred approval do not count. Native independent review
requirements still apply, including to maintainer-authored PRs. Do not manufacture
this approval as the developing agent.

The controller is event-driven by existing CI, PR and review events. It publishes
`protected merge acceptance` and arms native auto-merge only when every hold is
cleared; it never uses admin bypass, force push or an unprotected merge. API
errors fail closed. It does not fix code by itself: the active development agent
uses the existing test/fix/recheck cycle. No extra model subscription or runner
class is installed. Initial bootstrap must itself pass security review before the
trusted main coordinator exists; do not temporarily remove its required check.

Current deployment evidence belongs in HANDOFF.md. Repository administration
activated strict protection, auto-merge availability, dependency alerts/security
updates, secret scanning and push protection. Dependabot retains its existing
weekly grouped Actions configuration. CodeQL reuses PR #16's Python workflow;
this is not C++ engine analysis. CodeRabbit's repository-specific service limits
can still prevent reviews; configuration does not override vendor eligibility.

See [GitHub branch protection](https://docs.github.com/en/repositories/configuring-branches-and-merges-in-your-repository/managing-protected-branches/about-protected-branches)
and [CodeRabbit configuration](https://docs.coderabbit.ai/reference/configuration).

## Native-first assessment and optional check publisher

Native GitHub Auto-Merge is available for this public, personally owned repository;
it does not require a custom App. Active ruleset `27257972` now enforces strict
source-bound checks, fresh independent review, resolved conversations, squash/linear
history, no force push/deletion and CodeQL result protection without bypass actors.
The native scanning rule blocks configured findings, incomplete analysis and a
missing tool. It complements the existing CI instead of adding another scanner.
Legacy branch protection stays in place during validation.

Native checks do not express arbitrary project acceptance criteria or identify a
particular trusted Actions workflow merely from its check name. GitHub accepts
skipped/neutral required checks; these do not prove a CodeRabbit review occurred.
GitHub documents required trusted workflows at organization/enterprise level;
team-specific path reviewers are unavailable to user-owned repositories. Two
non-enforcing API workflow-rule probes returned HTTP 422, including a reference
to the existing CI workflow. That error alone is not proof of a plan restriction.
No probe ruleset was created and no organization migration is authorized.

Without an App, the existing administrator host can audit native rules, validate
independent current-head acceptance/sensitive approvals and arm native Auto-Merge.
That needs no new credentials, but the extra project acceptance decision remains
host-controlled rather than independently represented by a dedicated server check.
The fully automatic independently authenticated acceptance variant therefore remains
an optional, unapproved design; the existing custom required check is not removed
just to make a partial native test mergeable.

If that variant is approved, the App is a **check publisher only**: Checks write
plus GitHub's mandatory Metadata read, installed only on CSRetro. The existing host
reads PR/review evidence, audits rules and enables native Auto-Merge using existing
authority. The App needs no Administration, Contents, Pull requests or security-alert
permissions. Its token can neither merge nor edit code/rules. Native CodeQL rules
supply the server-side finding gate. Bypass actors are hidden without ruleset-write
access: the existing administrator host must audit them; a default Actions token
may fail this audit. Missing audit visibility blocks the candidate controller and
must not be solved by silently increasing App permissions.

The key would exist only as `CSRETRO_MERGE_APP_PRIVATE_KEY` in the
`csretro-merge-controller` environment, whose server-side branch policy allows
only branch `main`, no tags or other branches. App ID is a non-secret environment
variable `CSRETRO_MERGE_APP_ID`. A pinned official action mints an installation
check token for the job and revokes it afterward; `GH_ACCEPTANCE_TOKEN` is used
only for check creation/update. Existing host authority uses a separate token.
Never place the private key in repository-wide secrets, source, logs, caches or
PR jobs. Limit installation to this repository; rotate/revoke the key on exposure,
replace the environment secret and rerun negative tests. No key or App grant has
been created. Provisioning still requires explicit approval.

Before activation, prove real-token access, main-only secret isolation, no-bypass
audit, failure/head-change blocking and CodeRabbit reviews. Repository Auto-Merge
stays off until those gates pass. Bootstrap order must retain #15's dependency on
#14; a reviewed local host can bridge initial App-bound acceptance without removing
checks, while #16 supplies the existing Python scanner. No second persistent daemon.

Sources: [native Auto-Merge](https://docs.github.com/en/pull-requests/how-tos/merge-and-close-pull-requests/automatically-merging-a-pull-request),
[rules and trusted workflow scope](https://docs.github.com/en/enterprise-cloud%40latest/repositories/configuring-branches-and-merges-in-your-repository/managing-rulesets/available-rules-for-rulesets),
[scanning merge protection](https://docs.github.com/en/code-security/concepts/code-scanning/merge-protection),
[Rules API visibility](https://docs.github.com/en/rest/repos/rules#get-a-repository-ruleset).
