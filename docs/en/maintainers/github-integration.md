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

## Trusted producer setup

Auto-merge remains disabled until a dedicated GitHub App is installed and tested.
The shared Actions App identity cannot prove which workflow minted a check.
The dedicated App requires Administration read (live protection), Checks write,
Contents write (native merge), Pull requests write and Code scanning alerts read.
This permission/credential setup needs explicit maintainer approval. Do not grant
these permissions to arbitrary PR jobs or substitute the shared Actions identity.

Store the App private key only as the `CSRETRO_MERGE_APP_PRIVATE_KEY` environment
secret and App ID as `CSRETRO_MERGE_APP_ID` in `csretro-merge-controller`.
Its server-side deployment branch policy must allow branch `main` only, with no
other branches or tags. Never store this key as a repository-wide secret: same-repo
PR workflows could access it. Set the central policy acceptance_app_id to that App
and bind the required acceptance check to it before enabling repository auto-merge.
An unprivileged review-signal job wakes the trusted controller; it does not inspect
code, publish checks or receive secrets. The controller excludes its own check
completion to avoid event loops. Test an actual App-token run and a blocked PR
before claiming deployment; API permission errors remain blocking.

Bootstrap is a separately reviewed security operation. Before the controller is
on main, the reviewed local controller can publish acceptance using the approved
App token; do not mint a shared-Actions substitute. PR #16 already contains the
Python scanner and may provide the initial workflow foundation under all the same
gates. Reconcile dependency order explicitly; #15 always follows #14. The local
bridge must not become a second persistent orchestrator or a protection bypass.
