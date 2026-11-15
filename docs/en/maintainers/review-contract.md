# Provider-neutral independent review

This is a staged contract, not an activated alternative merge gate. The effective
CodeRabbit-specific acceptance workflow, native review rules, security checks
and host merge audit remain in force. `.github/review-contract.json` is disabled
and contains no enrolled executors. Changing this security boundary requires
separate independent review and explicit scoped maintainer approval.

## Actual execution and source evidence

`tools/review_adapters.py` captures authenticated repository ID/name, PR number,
exact head/base, complete changed/renamed paths, before/after Git blob identities
and SHA-256 fingerprints, complete acceptance manifest and canonical digest.
It reads source bytes only and detects a moving PR; it never executes PR code.
The host supplies the authoritative contributing actors and approved trust pin.
These cannot come from a receipt or instructions embedded in repository text.

`tools/review_contract.py` verifies a completed executor receipt containing the
actual input/output hashes, pinned instructions and runtime/model identity,
separate reviewer actor, path coverage, decisions for every manifest criterion,
source excerpts matching actual blob bytes, and findings. Generic summaries and
incomplete evidence are insufficient. A cryptographic signature authenticates
origin and integrity; it does not establish the quality of an assessment.

An independently approved executor profile must come from the immutable protected
target base and match an externally pinned digest. Keys embedded in a receipt or
added by its own PR cannot enroll an executor. No signing service, key enrollment,
new App, cloud model, package installation or paid execution is provided here.
Actual native-agent and installed local-model runs can produce draft evidence;
unsigned drafts never acquire live merge authority. A self-signed implementing
agent is not an independent reviewer. Test keys are ephemeral fixtures only.

## Selection and formal approval

A trusted negative review blocks selection even when another provider passes.
An unavailable provider is distinct from a substantive failure. The CodeRabbit
adapter accepts quota unavailability only from an authenticated exact-head check
with an explicit quota title, or records absence of an executed check. Ambiguous
custom results, withdrawn reviews and malformed evidence never become a quota
exception. The existing genuine substantive CodeRabbit validator remains reused.
Every selected result must match the same externally expected source digest.

Content acceptance never replaces a native GitHub approval. The required current
commit approval must come from another authorized GitHub identity; unresolved
blocking votes remain blockers after a push. The existing host still checks all
mandatory CI/security handles, actual protection, fresh base, resolved threads,
sensitive-change authorization and human-only runtime acceptance. The staged
selector and CLI alone cannot authorize a merge or certify gameplay.

## Validation and rollout

Run `python3 -m unittest discover -s tests/review_contract -v` for real Ed25519
fixture verification and positive/negative source, identity, tampering,
unavailable-versus-failed, stale review and self-enrollment cases. Existing merge
tests and repository verification also remain mandatory. Targeted included
CodeRabbit reviews replace automatic incremental requests in the candidate
configuration; no billing fallback is enabled.

Next: independently approve the contract and executor trust model, enroll only
approved existing execution identities, then connect typed results to the
read-only acceptance job and provider-neutral host audit on a separately reviewed
change. Repeat the protected sandbox test for that route before production use.
The successful PR #18 sandbox test validates the existing CodeRabbit route only.
Evidence and current blockers live in `HANDOFF.md`; this guide
does not maintain a second status journal. Reusable receipt, adapter and trust
boundaries are candidates for Orqen; no Orqen repository is modified.

[CodeRabbit configuration reference](https://docs.coderabbit.ai/reference/configuration)
