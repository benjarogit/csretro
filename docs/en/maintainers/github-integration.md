# Independent protected acceptance

Normal development uses native GitHub squash Auto-Merge after the seven existing
source-bound checks, fresh independent review and resolved conversations pass.
The NextClient/Xash3D port remains the development objective; no game starts here.

`protected merge acceptance` is a standard read-only Actions job. It adds no App,
key, secret, `checks:write` or privileged PR execution. The existing Actions identity
authenticates the service, not a unique workflow. Workflow source is itself subject
to current-head independent review and native protection; this does not claim the
stronger anti-forgery properties of a distinct external publisher.

## Commit-bound evidence

Every PR supplies `docs/evidence/acceptance.json` at its exact head:

- `schema`: 1; `base_sha`: the actual PR base commit.
- `human_runtime_required`: false only when no human runtime acceptance is needed.
- `changed_paths`: every current and old renamed path, including this manifest.
- `criteria`: distinct `id`, a concrete `requirement` and `checks` naming the actual
  mandatory CI/CodeQL contexts from the central policy. No invented checks.

Missing human acceptance cannot be synthesized. A base update or changed manifest
requires a fresh review. The verifier reads GitHub content at the immutable head;
it does not trust PR-body claims or a branch name that may have advanced.

CodeRabbit counts only with its actual bot identity, current-head approval and a
successful current-head CodeRabbit run. Skipped, neutral, rate-limited, cancelled,
failed, summary-only or empty responses never pass. It must independently inspect
every changed path and criterion, then submit a review containing:

```text
<!-- csretro-acceptance-review {"schema":1,"head":"PR_HEAD_SHA","acceptance_sha256":"DIGEST","decision":"pass","reviewed_paths":["all changed paths"],"summary":"Concrete independent assessment of the inspected changes","criteria":[{"id":"criterion-id","passed":true,"reason":"Concrete verified evidence for this criterion"}]} -->
```

DIGEST is SHA-256 of the manifest's UTF-8 canonical Python
`json.dumps(artifact, sort_keys=True, separators=(',', ':'))`. The attestation is
not supplied by the author or manufactured by the coordinator. A later negative
assessment or withdrawn approval blocks acceptance. A status name or approval
without substantive criterion/path evidence is insufficient.

The configured CodeRabbit custom pre-merge check independently checks this evidence;
automatic approvals explicitly reject skipped reviews and author approval commands.
Those settings cannot override service eligibility, capacity or plan limits.

## Native integration and exceptions

The acceptance workflow rechecks on PR changes and review submission/edit/dismissal.
It has read permissions only and receives no secrets. Expensive CI builds are not
repeated just for a review event. The redundant review-signal workflow is removed.
All other CI and CodeQL result gates remain separate native requirements.

The existing authorized host audits live Ruleset/branch state, all current checks,
actual review evidence and sensitivity via `tools/github_merge_gate.py`. The
`--publish` option now only arms native `--auto --squash --match-head-commit` after
all evidence passes; it never writes a successful check. Hidden bypass details or
API errors block the audit. No new administrator token is installed.

Releases, production deployments, paid actions, permissions and other sensitive
changes retain explicit current-head maintainer approval. An authorized maintainer
can review or comment `CSRETRO-APPROVE-SENSITIVE FULL_HEAD_SHA`; the developing agent
must never manufacture this approval. The independent review requirements remain.

Bootstrap requires the same real reviews and checks. PR #16's scanner foundation
must be reconciled without importing the entire stacked branch or weakening gates;
#15 remains dependent on #14. A docs-only canary on main verifies negative rejection
and, only after genuine acceptance, positive native Auto-Merge. Missing scanner or
reviewer is a blocker, not permission to mark the end-to-end test passed. Auto-Merge
stays disabled until a real positive canary completes under effective safeguards.

Current results and CI links live only in HANDOFF.md. The pre-push verifier isolates
hook-local Git variables from nested test repositories while preserving LFS context.

Sources: [native Auto-Merge](https://docs.github.com/en/pull-requests/how-tos/merge-and-close-pull-requests/automatically-merging-a-pull-request),
[CodeRabbit approvals and custom checks](https://docs.coderabbit.ai/reference/configuration).
