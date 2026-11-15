#!/usr/bin/env python3
"""Read-only independent acceptance: immutable criteria and substantive bot evidence."""
import argparse
import base64
import hashlib
import json
from pathlib import Path
import re
import time

import github_merge_gate as gate


MARKER = r'<!-- csretro-acceptance-review\s+(\{.*?\})\s*-->'
NEGATIVE = re.compile(r'\b(review skipped|rate limited|rate-limited|not reviewed|no (?:code|model-driven) review)\b', re.I)


def validate(pr, files, reviews, checks, artifact, policy):
    holds = []
    head = pr['head']['sha']
    paths = set(gate.changed_paths(files))
    if not isinstance(artifact, dict):
        return ['Acceptance artifact is not an object']
    criteria = artifact.get('criteria')
    if (type(artifact.get('schema')) is not int or artifact['schema'] != 1
            or artifact.get('base_sha') != pr['base']['sha']
            or artifact.get('human_runtime_required') is not False
            or not isinstance(criteria, list) or not criteria):
        return ['Acceptance artifact schema, base, criteria or runtime evidence is invalid']
    declared = artifact.get('changed_paths')
    if not isinstance(declared, list) or set(declared) != paths or len(declared) != len(paths):
        holds.append('Acceptance manifest must cover every current and renamed changed path')
    criterion_ids = []
    cited_checks = set()
    try:
        for criterion in criteria:
            identifier = criterion['id']
            if not isinstance(identifier, str) or not identifier or identifier in criterion_ids:
                raise ValueError('invalid id')
            if not isinstance(criterion['requirement'], str) or len(criterion['requirement'].strip()) < 20:
                raise ValueError('empty requirement')
            evidence = criterion['checks']
            if not isinstance(evidence, list) or not evidence or not all(isinstance(item, str) for item in evidence):
                raise ValueError('missing check evidence')
            criterion_ids.append(identifier)
            cited_checks.update(evidence)
    except (KeyError, TypeError, ValueError):
        return holds + ['Acceptance criteria require distinct IDs, requirements and check evidence']
    mandatory = set(policy['required_checks']) | {item['context'] for item in policy['security_checks']}
    if not mandatory <= cited_checks or cited_checks - mandatory:
        holds.append('Criteria must reference all mandatory checks and no invented checks')
    robot_checks = [check for check in checks if check.get('name') == 'CodeRabbit'
                    and check.get('app', {}).get('id') == policy['reviewer_app_id']
                    and check.get('head_sha') == head]
    newest = max(robot_checks, key=lambda check: check['id'], default={})
    output = newest.get('output') or {}
    if (newest.get('status') != 'completed' or newest.get('conclusion') != 'success'
            or NEGATIVE.search(' '.join(str(output.get(key) or '') for key in ('title', 'summary', 'text')))):
        holds.append('CodeRabbit did not complete a successful substantive review for this head')
    digest = hashlib.sha256(json.dumps(artifact, sort_keys=True, separators=(',', ':')).encode()).hexdigest()
    authored = [review for review in sorted(reviews, key=lambda item: item.get('id', 0))
                if review.get('user', {}).get('login') == policy['reviewer']
                and review['user'].get('id') == policy['reviewer_user_id']
                and review.get('commit_id') == head]
    votes = [review for review in authored if review.get('state') in ('APPROVED', 'CHANGES_REQUESTED', 'DISMISSED')]
    if not votes or votes[-1]['state'] != 'APPROVED':
        holds.append('CodeRabbit current-head approval is absent or withdrawn')
    attested = False
    # Latest current-head review with an attestation wins; a later failure revokes it.
    for review in reversed(authored):
        matches = re.findall(MARKER, review.get('body') or '', re.S)
        if not matches:
            continue
        if len(matches) != 1 or review.get('state') not in ('APPROVED', 'COMMENTED'):
            break
        try:
            proof = json.loads(matches[0])
            decisions = proof['criteria']
            if (proof['schema'] != 1 or proof['head'] != head
                    or proof['acceptance_sha256'] != digest or proof['decision'] != 'pass'
                    or set(proof['reviewed_paths']) != paths
                    or len(proof['reviewed_paths']) != len(paths)
                    or len(proof['summary'].strip()) < 40
                    or {item['id'] for item in decisions} != set(criterion_ids)
                    or len(decisions) != len(criterion_ids)):
                break
            if all(item['passed'] is True and len(item['reason'].strip()) >= 20 for item in decisions):
                attested = True
        except (KeyError, TypeError, ValueError, AttributeError):
            pass
        break
    if not attested:
        holds.append('Missing substantive current-head CodeRabbit acceptance attestation with complete path/criterion evidence')
    return holds


def read_artifact(github, head, policy):
    value = github.api(f'contents/{policy["acceptance_path"]}?ref={head}')
    if value.get('encoding') != 'base64' or value.get('type') != 'file':
        raise RuntimeError('Acceptance artifact unavailable at the exact PR head')
    return json.loads(base64.b64decode(value['content']))


def inspect(github, number, policy):
    pr = github.api(f'pulls/{number}')
    artifact = read_artifact(github, pr['head']['sha'], policy)
    files = github.pages(f'pulls/{number}/files')
    reviews = github.pages(f'pulls/{number}/reviews')
    checks = []
    for page in range(1, 101):
        batch = github.api(f'commits/{pr["head"]["sha"]}/check-runs?per_page=100&page={page}')['check_runs']
        checks.extend(batch)
        if len(batch) < 100:
            break
    else:
        raise RuntimeError('Incomplete review check evidence')
    holds = validate(pr, files, reviews, checks, artifact, policy)
    fresh = github.api(f'pulls/{number}')
    if fresh['head']['sha'] != pr['head']['sha'] or fresh['base']['sha'] != pr['base']['sha']:
        holds.append('PR head/base changed during independent acceptance')
    return {'head': pr['head']['sha'], 'holds': holds, 'eligible': not holds,
            'acceptance_sha256': hashlib.sha256(json.dumps(artifact, sort_keys=True, separators=(',', ':')).encode()).hexdigest()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', required=True)
    parser.add_argument('--pr', type=int, required=True)
    parser.add_argument('--expected-head', required=True)
    parser.add_argument('--wait-seconds', type=int, default=0)
    args = parser.parse_args()
    policy = json.loads((Path(__file__).resolve().parents[1] / '.github/merge-policy.json').read_text())
    github = gate.GitHub(args.repo)
    if not 0 <= args.wait_seconds <= 90:
        raise ValueError('Read-only wait must be between zero and 90 seconds')
    deadline = time.monotonic() + args.wait_seconds
    while True:
        result = inspect(github, args.pr, policy)
        if result['eligible'] or result['head'] != args.expected_head or time.monotonic() >= deadline:
            break
        # Bot check completion can follow the review event; bounded reads only.
        time.sleep(min(10, max(0, deadline - time.monotonic())))
    if result['head'] != args.expected_head:
        result['holds'].append('Actions event belongs to an outdated head')
        result['eligible'] = False
    print(json.dumps(result, indent=2))
    raise SystemExit(0 if result['eligible'] else 1)


if __name__ == '__main__':
    main()
