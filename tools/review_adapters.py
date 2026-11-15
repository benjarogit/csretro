"""Read-only adapters for the staged review contract; no trust enrollment.

The host supplies an independently authenticated PR snapshot and implementers.
PR text, model output and candidate-head policy cannot establish those facts.
"""
from pathlib import Path
import json
import base64
import hashlib
from urllib.parse import quote
import subprocess

from review_contract import InvalidReview, Result, digest, hex_digest, require, validate_snapshot


def base_trust(repository, base, expected_pin):
    """Read ONLY the immutable target-base policy; absence never falls back to head."""
    require(hex_digest(base, 40) and hex_digest(expected_pin), 'Exact base and external policy pin required')
    result = subprocess.run(['git', '-C', str(Path(repository)), 'show',
                             base + ':.github/review-contract.json'], capture_output=True)
    require(result.returncode == 0, 'Target base has no approved alternative review profile')
    profile = json.loads(result.stdout)
    require(digest(profile) == expected_pin, 'Base profile differs from independent trust pin')
    return profile


def source_request(github, number, policy, instructions):
    """Capture complete authenticated source bytes without executing PR code."""
    from github_acceptance import read_artifact
    repository = github.api('')
    pr = github.api(f'pulls/{number}')
    manifest = read_artifact(github, pr['head']['sha'], policy)
    files, blobs = [], {}
    statuses = {'added':'A', 'modified':'M', 'removed':'D', 'renamed':'R', 'changed':'T'}
    for changed in github.pages(f'pulls/{number}/files'):
        require(changed.get('status') in statuses, 'Unsupported PR file status')
        path, old = changed['filename'], changed.get('previous_filename')
        file = {'path':path, 'status':statuses[changed['status']]}
        if old: file['previous_path'] = old
        for side, ref, name, absent in (
            ('before', pr['base']['sha'], old or path, changed['status']=='added'),
            ('after', pr['head']['sha'], path, changed['status']=='removed')):
            if absent:
                file[side+'_blob'] = file[side+'_sha256'] = None
                continue
            value = github.api('contents/' + quote(name, safe='/') + '?ref=' + ref)
            require(value.get('type') == 'file' and value.get('encoding') == 'base64', 'Complete source blob unavailable')
            content = base64.b64decode(value['content'])
            blob = hashlib.sha1(b'blob ' + str(len(content)).encode() + b'\0' + content).hexdigest()
            require(value.get('sha') == blob, 'GitHub source/blob mismatch')
            file[side+'_blob'], file[side+'_sha256'] = blob, hashlib.sha256(content).hexdigest()
            blobs[blob] = base64.b64encode(content).decode()
        files.append(file)
    snapshot = {'repository_id':repository['id'], 'repository':repository['full_name'], 'pr':number,
                'head':pr['head']['sha'], 'base':pr['base']['sha'], 'manifest_sha256':digest(manifest),
                'criterion_ids':[c['id'] for c in manifest['criteria']], 'files':files}
    validate_snapshot(snapshot)
    fresh = github.api(f'pulls/{number}')
    require(fresh['head']['sha'] == snapshot['head'] and fresh['base']['sha'] == snapshot['base'], 'PR moved during source capture')
    return {'snapshot':snapshot, 'instructions':instructions, 'acceptance_manifest':manifest, 'source_blobs':blobs}


def coderabbit(github, number, policy, snapshot):
    """Reuse the existing authenticated substantive review validator, never labels.

    Failed substantive checks remain negative, unavailable service is not a code
    failure. All ambiguous/invalid evidence raises instead of inventing a pass.
    This adapter is not wired into the live acceptance workflow yet.
    """
    from github_acceptance import inspect
    # The existing reader validates genuine service identity, exact source,
    # manifest, path coverage and substantive criterion results.
    live = source_request(github, number, policy, '')['snapshot']
    require(snapshot == live, 'CodeRabbit adapter snapshot is not the authenticated PR source')
    result = inspect(github, number, policy, require_approval=False)
    require(result.get('head') == snapshot['head'] and result.get('acceptance_sha256') == snapshot['manifest_sha256'], 'Provider reviewed a different head')
    if result['eligible']:
        return Result('coderabbit', 'available', 'pass', True, policy['reviewer'], (), digest(snapshot))
    state = coderabbit_state(github, number, policy, snapshot)
    if state is not None:
        return state
    # Ambiguous/incomplete output is never downgraded to quota unavailability.
    raise InvalidReview('CodeRabbit has unresolved holds: ' + '; '.join(result['holds']))


def coderabbit_state(github, number, policy, snapshot):
    """Classify only authenticated exact-head service evidence, never holds text."""
    from github_acceptance import reviewed_coverage, custom_check_status
    head = snapshot['head']
    reviews = [r for r in github.pages(f'pulls/{number}/reviews')
               if r.get('user', {}).get('id') == policy['reviewer_user_id']
               and r['user'].get('login') == policy['reviewer']]
    votes = [r for r in reviews if r.get('state') in ('APPROVED', 'CHANGES_REQUESTED', 'DISMISSED')]
    latest = max(votes, key=lambda r:r['id'], default={})
    if latest.get('state') in ('CHANGES_REQUESTED', 'DISMISSED'):
        return Result('coderabbit', 'available', 'fail', True, policy['reviewer'], ('Blocking or withdrawn actual review',), digest(snapshot))
    for comment in github.pages(f'issues/{number}/comments'):
        if (comment.get('user', {}).get('id') == policy['reviewer_user_id']
                and comment['user'].get('login') == policy['reviewer']
                and reviewed_coverage(comment.get('body', ''), head)
                and custom_check_status(comment.get('body', ''), policy) == 'not-passed'):
            # Existing provider publication does not distinguish inconclusive
            # and negative custom results reliably; block both conservatively.
            raise InvalidReview('Actual custom review has unresolved non-passing evidence')
    checks = []
    for page in range(1, 101):
        batch = github.api(f'commits/{head}/check-runs?per_page=100&page={page}')['check_runs']
        checks.extend(batch)
        if len(batch) < 100: break
    else: raise InvalidReview('Incomplete provider check pagination')
    checks = [c for c in checks if c.get('name') == 'CodeRabbit'
              and c.get('app', {}).get('id') == policy['reviewer_app_id'] and c.get('head_sha') == head]
    def quota(item):
        return (item.get('output') or {}).get('title', '').strip().casefold() in ('review rate limited', 'review rate-limited')
    # A quota rerun cannot erase an actual earlier negative assessment. A later
    # successful check alone is insufficient to resolve it: inspect() above must
    # also validate the genuine substantive source/criterion attestation.
    failures = [c for c in checks if c.get('status') == 'completed'
                and c.get('conclusion') in ('failure', 'action_required') and not quota(c)]
    if failures:
        return Result('coderabbit', 'available', 'fail', True, policy['reviewer'], ('Executed service review failed; no validated substantive replacement',), digest(snapshot))
    check = max(checks, key=lambda c:c['id'], default={})
    if not check:
        return Result('coderabbit', 'unavailable', 'not-run', False, '', ('No executed exact-head service check',), digest(snapshot))
    title = (check.get('output') or {}).get('title', '').strip().casefold()
    if check.get('status') == 'completed' and title in ('review rate limited', 'review rate-limited'):
        return Result('coderabbit', 'unavailable', 'not-run', False, '', ('Authenticated included-review quota exhausted',), digest(snapshot))
    if check.get('status') == 'completed' and check.get('conclusion') in ('failure', 'action_required'):
        return Result('coderabbit', 'available', 'fail', True, policy['reviewer'], ('Executed service review failed',), digest(snapshot))
    return None
