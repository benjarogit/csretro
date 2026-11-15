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

## Dedicated reviewer App: proposed setup, not authorization

GitHub's [create-review API](https://docs.github.com/en/rest/pulls/reviews#create-a-review-for-a-pull-request)
explicitly supports installation access tokens, `APPROVE` and an explicit
`commit_id`. The minimum publisher permission is **Pull requests: write**, plus
mandatory Metadata read. This is an App installation identity, not a user token
acting as the PR author. No second personal GitHub account is proposed.

Separate the capabilities:

- The reviewer reads source and executes the approved independent review. For
  private source it needs Contents read and Pull requests read; it gets no App
  private key or write token. Existing installed local models remain optional
  providers, with pinned runtime and genuine output requirements.
- The isolated publisher needs Pull requests write only to publish the verified
  review. Contents read is needed only if it also fetches source; Checks read
  only if it checks run evidence itself. Keep the existing host's protection,
  CodeQL and CI audit separate. No Contents write, Checks write, Administration
  write, Actions write, bypass entry or broader installation is required.
- Install only on CSRETRO. Narrow each installation token to this repository and
  the permissions needed by that operation. Tokens expire after one hour;
  revoke them early when finished. Never store tokens in review artifacts.

See GitHub's [installation-token documentation](https://docs.github.com/en/apps/creating-github-apps/authenticating-with-a-github-app/generating-an-installation-access-token-for-a-github-app).
No registration, installation, key generation, secret upload or permission grant
has been performed by this investigation.

### Independence is a deployment boundary

A bot name alone provides no independence. The implementing agent must not be
able to read the App private key, mint installation tokens, change the enrolled
executor profile, access the executor signing key, or invoke an unrestricted
approval/signature endpoint. A key in its own checkout, accessible local account
or ordinary repository secret is insufficient. GitHub warns that repository
writers can obtain repository secrets through workflows; see its
[secure-use guidance](https://docs.github.com/en/actions/reference/security/secure-use).

Prefer a separately administered reviewer/publisher process outside the author's
filesystem and credentials. A request may identify a repository/PR, never supply
a trusted decision. The reviewer independently fetches the immutable source,
executes the pinned review, and returns authenticated execution evidence. The
publisher verifies that evidence against the approved immutable trust profile,
checks every manifest criterion/path and unresolved negative finding, then
re-fetches head/base immediately before publishing. It records App/bot IDs,
review ID, exact commit, manifest/source/input/output digests, executor/runtime,
verifier revision, decision and evidence reference. Replay or movement aborts.
Repository text and model output are untrusted data, never operational commands.

An Actions-hosted design would additionally need proof that only an independently
approved immutable workflow can access a restricted environment and credentials;
PR/head code must never execute there with secrets. Merely adding a secret to an
existing environment is not that proof. Changes to reviewer configuration,
workflow, keys or security policy retain explicit maintainer approval. A sole
administrator who also owns the App can override the control plane; no App can
cryptographically prevent that administrator from issuing new keys or changing
rules. Document that trust boundary rather than claiming protection against it.

### Approval and protected test sequence

1. Maintainer separately approves the repository-scoped permission set, hosting
   isolation, trust enrollment, key custodian and rotation/revocation procedure.
   Store the App private key only in the approved isolated secret store. Use
   short-lived JWTs to mint narrowly scoped installation tokens; rotate/revoke
   keys under [GitHub's private-key procedure](https://docs.github.com/en/apps/creating-github-apps/authenticating-with-a-github-app/managing-private-keys-for-github-apps).
2. Implement and independently review the trusted execution/publishing boundary;
   enroll immutable App ID, bot user ID and executor public keys only after
   approval. Keep the current live route and protections until migration passes.
3. In an equally protected sandbox, require real independent execution and a
   current-head App `APPROVED` review. Read back the review's identity/commit and
   native `reviewDecision`; prove last-push approval, stale dismissal and all
   seven existing CI/security checks. An accepted REST call alone is insufficient.
4. Exercise negative cases: self/author-authored receipts, fake or unavailable
   execution, failed review followed by quota exhaustion, stale head/base,
   changed manifest/path, unknown/revoked key, replay, withdrawn vote, unresolved
   discussion and failed CI/CodeQL. Every one must block native Auto-Merge.
5. Only after separate migration authorization and a genuine positive sandbox
   result may the provider-neutral route participate in normal protected merges.
   The publisher does not merge; the existing host/native rules retain authority.

GitHub documents [last-push and stale-review protection](https://docs.github.com/en/repositories/configuring-branches-and-merges-in-your-repository/managing-protected-branches/about-protected-branches).
The earlier CodeRabbit App vote worked in the existing sandbox under equivalent
rules. Acceptance of this not-yet-installed App under those same rules remains
unproven. Do not change required approval count, allow App bypass or enable
Actions approval to make that test pass. This investigation grants no permissions
and promises no unlimited free compute; no paid provider or hosting is enabled.

The approval-pending App setup is parked independently of ordinary G07 port work.
It does not create a project-wide development dependency or authorize merging an
otherwise blocked PR. NextClient/Xash3D development continues in its existing
branch/task sequence, with all current review and security gates preserved.
