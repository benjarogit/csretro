#!/usr/bin/env python3
"""Read-only independent acceptance: immutable criteria and substantive bot evidence."""
import argparse
import base64
import hashlib
import html
import json
from pathlib import Path
import re
import time

import github_merge_gate as gate


MARKER = r'<!-- csretro-acceptance-review\s+(\{.*?\})\s*-->|```csretro-acceptance-review\s+(\{.*?\})\s*```'
NEGATIVE = re.compile(r'\b(review skipped|rate limited|rate-limited|not reviewed|no (?:code|model-driven) review)\b', re.I)


def provider_check_section(body):
    header = '<!-- This is an auto-generated comment: summarize by coderabbit.ai -->'
    start = '<!-- pre_merge_checks_walkthrough_start -->'
    end = '<!-- pre_merge_checks_walkthrough_end -->'
    if (not body.lstrip().startswith('<!-- coderabbit-ai-generated:v1 -->')
            or body.count(header) != 1 or 'auto-generated reply by CodeRabbit' in body
            or body.count(start) != 1 or body.count(end) != 1):
        return None
    first, last = body.index(start), body.index(end)
    return body[first + len(start):last] if first < last else None


def custom_check_status(body, policy):
    """Read one actual result in the official walkthrough, never a chat/template."""
    section = provider_check_section(body)
    if section is None:
        official = (body.lstrip().startswith('<!-- coderabbit-ai-generated:v1 -->')
                    and body.count('<!-- This is an auto-generated comment: summarize by coderabbit.ai -->') == 1
                    and 'auto-generated reply by CodeRabbit' not in body)
        return 'not-passed' if official else None
    name = policy.get('independent_check_name', 'CSRETRO independent acceptance').casefold()
    statuses = []
    for line in section.splitlines():
        cells = [cell.strip().strip('*') for cell in line.strip().split('|')]
        if len(cells) >= 3 and cells[1].casefold() == name:
            passed = re.search(r'!\[Passed\]\(https://storage\.googleapis\.com/coderabbit_public_assets/pre-merge-checks-passed\.svg\)', cells[2])
            statuses.append('Passed' if passed else 'not-passed')
    return statuses[0] if len(statuses) == 1 else 'not-passed'


def custom_explanation(body, policy):
    section = provider_check_section(body)
    if section is None:
        return ''
    name = re.escape(policy.get('independent_check_name', 'CSRETRO independent acceptance'))
    matches = list(re.finditer(r'<summary><strong>Full details:\s*' + name
                             + r'</strong></summary>(.*?)</details>', section, re.I | re.S))
    return matches[0][1] if len(matches) == 1 else ''


def reviewed_coverage(body, head):
    matches = re.findall(r'<!-- final_review_risk_coverage:(\{.*?\})\s*-->', body)
    if len(matches) != 1:
        return False
    try:
        value = json.loads(matches[0])
        return (value['sourceCommitId'] == head and value['coveredCommitId'] == head
                and value['kind'] == 'reviewed')
    except (ValueError, KeyError, TypeError):
        return False


def validate(pr, files, reviews, checks, artifact, policy, comments=(), *, require_approval=True):
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
    if (votes and votes[-1]['state'] != 'APPROVED') or (require_approval and not votes):
        holds.append('CodeRabbit current-head approval is absent or withdrawn')
    attested = False
    sources = [(review, False) for review in authored]
    sources += [(comment, True) for comment in comments
                if comment.get('user', {}).get('login') == policy['reviewer']
                and comment['user'].get('id') == policy['reviewer_user_id']]
    sources.sort(key=lambda item: (item[0].get('updated_at') or item[0].get('submitted_at')
                                   or item[0].get('created_at') or '', item[0].get('id', 0)))
    for source, is_comment in reversed(sources):
        body = html.unescape(source.get('body') or '')
        row = custom_check_status(body, policy)
        publication = custom_explanation(body, policy) if is_comment else body
        matches = [left or right for left, right in re.findall(MARKER, publication, re.S)]
        if not matches and row is None:
            continue
        if len(matches) != 1:
            break
        if is_comment:
            if row != 'Passed' or not reviewed_coverage(body, head):
                break
        elif source.get('state') not in ('APPROVED', 'COMMENTED'):
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


def inspect(github, number, policy, *, require_approval=True):
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
    comments = github.pages(f'issues/{number}/comments')
    holds = validate(pr, files, reviews, checks, artifact, policy, comments, require_approval=require_approval)
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
    parser.add_argument('--content-only', action='store_true', help='Read-only content job; native/host approval remains mandatory')
    parser.add_argument('--wait-seconds', type=int, default=0)
    args = parser.parse_args()
    policy = json.loads((Path(__file__).resolve().parents[1] / '.github/merge-policy.json').read_text())
    github = gate.GitHub(args.repo)
    if not 0 <= args.wait_seconds <= 90:
        raise ValueError('Read-only wait must be between zero and 90 seconds')
    deadline = time.monotonic() + args.wait_seconds
    while True:
        result = inspect(github, args.pr, policy, require_approval=not args.content_only)
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
