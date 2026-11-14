#!/usr/bin/env python3
"""Inspect live GitHub evidence and enable native auto-merge without bypasses.

The Actions caller checks out main, never PR code. No game/model is launched.
Acceptance is an auditable, current-head PR-body evidence declaration reviewed by
CodeRabbit; it cannot replace human-only gameplay acceptance or security approval.
"""
import argparse
import fnmatch
import hashlib
import json
from pathlib import Path
import re
import subprocess


class GitHub:
    def __init__(self, repo):
        self.repo = repo

    def api(self, path, method='GET', data=None):
        args = ['gh', 'api', f'repos/{self.repo}' + (f'/{path}' if path else ''), '--method', method]
        if data is not None:
            args += ['--input', '-']
        result = subprocess.run(args, input=json.dumps(data) if data is not None else None,
                                text=True, capture_output=True, check=True)
        return json.loads(result.stdout) if result.stdout.strip() else None

    def pages(self, path):
        result = []
        for page in range(1, 101):
            batch = self.api(f'{path}{"&" if "?" in path else "?"}per_page=100&page={page}')
            result.extend(batch)
            if len(batch) < 100:
                return result
        raise RuntimeError('Pagination limit reached; refusing incomplete evidence')

    def graph(self, query, variables):
        result = subprocess.run(['gh', 'api', 'graphql', '--input', '-'],
                                input=json.dumps({'query': query, 'variables': variables}),
                                capture_output=True, text=True, check=True)
        data = json.loads(result.stdout)
        if data.get('errors'):
            raise RuntimeError('GraphQL evidence unavailable')
        return data['data']


def protection_holds(protection, policy):
    holds = []
    checks = protection.get('required_status_checks', {})
    required = set(policy['required_checks'])
    trusted_app = policy.get('acceptance_app_id')
    if not isinstance(trusted_app, int) or trusted_app in (15368, 57789):
        holds.append('Dedicated acceptance GitHub App is not configured')
    if not any(c.get('context') == policy['acceptance_check'] and c.get('app_id') == trusted_app
               for c in checks.get('checks', [])):
        holds.append('Acceptance check is not bound to the dedicated trusted App')
    bound = {c['context'] for c in checks.get('checks', []) if c.get('app_id') == 15368}
    security = policy.get('security_checks', [])
    security_bound = all(any(c.get('context') == item['context'] and c.get('app_id') == item['app_id']
                             for c in checks.get('checks', [])) for item in security)
    if not checks.get('strict') or not required <= bound or not security_bound:
        holds.append('Required checks must be strict and bound to GitHub Actions')
    for flag in ('enforce_admins', 'required_conversation_resolution', 'required_linear_history'):
        if not protection.get(flag, {}).get('enabled'):
            holds.append(f'Protection not enforced: {flag}')
    reviews = protection.get('required_pull_request_reviews', {})
    if (reviews.get('required_approving_review_count', 0) < 1
            or not reviews.get('dismiss_stale_reviews') or not reviews.get('require_last_push_approval')):
        holds.append('Fresh independent approval protection is missing')
    for flag in ('allow_force_pushes', 'allow_deletions'):
        if protection.get(flag, {}).get('enabled'):
            holds.append(f'Unsafe protection setting: {flag}')
    return holds


def acceptance_holds(body, head):
    match = re.search(r'<!-- chatdev-acceptance\s+(\{.*?\})\s*-->', body or '', re.S)
    if not match:
        return ['Missing current-head acceptance evidence declaration']
    try:
        evidence = json.loads(match[1])
        criteria = evidence['criteria']
        if evidence['head'] != head or not criteria:
            raise ValueError('stale or empty')
        for item in criteria:
            if not item['criterion'].strip() or not item['evidence'].strip() or item['passed'] is not True:
                raise ValueError('incomplete')
        if evidence.get('human_runtime_required') is not False:
            raise ValueError('human runtime acceptance pending')
    except (KeyError, TypeError, ValueError, AttributeError):
        return ['Acceptance declaration is stale, incomplete or requires human runtime evidence']
    return []


def evaluate(pr, files, reviews, checks, protection, repository, threads, policy, authorizations=(), security_alerts=()):
    holds = protection_holds(protection, policy)
    head = pr['head']['sha']
    if not repository.get('allow_auto_merge'):
        holds.append('Repository auto-merge is disabled')
    if pr['state'] != 'open' or pr.get('draft') or pr['base']['ref'] != policy['base']:
        holds.append('PR must be open, ready and based on main')
    if pr.get('mergeable') is not True or pr.get('mergeable_state') not in ('clean', 'blocked', 'unstable'):
        holds.append('Merge conflict, stale base or mergeability unknown')
    if pr['head']['repo']['full_name'] != repository['full_name']:
        holds.append('Fork changes require a separate trusted review')
    latest = {}
    for review in reviews:
        if review['state'] in ('APPROVED', 'CHANGES_REQUESTED', 'DISMISSED'):
            latest[review['user']['login']] = review
    if any(r['state'] == 'CHANGES_REQUESTED' for r in latest.values()):
        holds.append('Blocking review remains')
    robot = latest.get(policy['reviewer'], {})
    if robot.get('state') != 'APPROVED' or robot.get('commit_id') != head:
        holds.append('CodeRabbit current-head approval is missing (skipped is not reviewed)')
    if threads:
        holds.append('Unresolved review conversations')
    current = {}
    for check in checks:
        if check.get('app', {}).get('id') == 15368 and check.get('head_sha') == head:
            old = current.get(check['name'])
            if old is None or check['id'] > old['id']:
                current[check['name']] = check
    for name in policy['required_checks']:
        check = current.get(name, {})
        if check.get('status') != 'completed' or check.get('conclusion') != 'success':
            holds.append(f'Required check not successful: {name}')
    for item in policy.get('security_checks', []):
        matching = [c for c in checks if c.get('name') == item['context']
                    and c.get('app', {}).get('id') == item['app_id'] and c.get('head_sha') == head]
        newest = max(matching, key=lambda c: c['id'], default={})
        if newest.get('status') != 'completed' or newest.get('conclusion') != 'success':
            holds.append(f'Security results not successful: {item["context"]}')
    holds += acceptance_holds(pr.get('body'), head)
    match = re.search(r'<!-- chatdev-acceptance\s+(\{.*?\})\s*-->', pr.get('body') or '', re.S)
    try:
        declaration = json.loads(match[1]) if match else None
        digest = hashlib.sha256(json.dumps(declaration, sort_keys=True, separators=(',', ':')).encode()).hexdigest()
        marker = f'CSRETRO-ACCEPTANCE {digest}'
        attested = any(r['state'] == 'APPROVED' and r.get('commit_id') == head
                       and login in [policy['reviewer'], *policy['maintainers']]
                       and login != pr['user']['login'] and marker in (r.get('body') or '').splitlines()
                       for login, r in latest.items())
        if not attested:
            holds.append('Independent review must attest the exact acceptance declaration digest')
    except (TypeError, ValueError):
        holds.append('Acceptance digest unavailable')
    if security_alerts:
        holds.append('Open CodeQL/security scanning findings require resolution')
    sensitive = any(fnmatch.fnmatch(f, pattern) for f in files for pattern in policy['sensitive_paths'])
    sensitive |= bool(set(policy['sensitive_labels']) & {label['name'] for label in pr.get('labels', [])})
    explicit = any(c.get('user', {}).get('login') in policy['maintainers']
                   and c.get('body', '').strip() == f'CSRETRO-APPROVE-SENSITIVE {head}'
                   for c in authorizations)
    if sensitive and not explicit and not any(r['state'] == 'APPROVED' and r.get('commit_id') == head
                             and login in policy['maintainers'] and login != pr['user']['login']
                             for login, r in latest.items()):
        holds.append('Sensitive change requires explicit current-head maintainer review')
    return {'head': head, 'holds': holds, 'eligible': not holds, 'sensitive': sensitive}


def changed_paths(files):
    return [name for item in files
            for name in (item['filename'], item.get('previous_filename')) if name]


def inspect(github, number, policy):
    pr = github.api(f'pulls/{number}')
    protection = github.api('branches/main/protection')
    repository = github.api('')
    files = changed_paths(github.pages(f'pulls/{number}/files'))
    reviews = github.pages(f'pulls/{number}/reviews')
    checks = []
    for page in range(1, 101):
        batch = github.api(f'commits/{pr["head"]["sha"]}/check-runs?per_page=100&page={page}')['check_runs']
        checks.extend(batch)
        if len(batch) < 100:
            break
    else:
        raise RuntimeError('Check pagination incomplete')
    owner, repo = github.repo.split('/')
    threads = []
    cursor = None
    while True:
        data = github.graph('''query($owner:String!,$repo:String!,$number:Int!,$cursor:String) {
          repository(owner:$owner,name:$repo) { pullRequest(number:$number) {
            reviewThreads(first:100,after:$cursor) { nodes { isResolved }
              pageInfo { hasNextPage endCursor } } } } }''',
                            dict(owner=owner, repo=repo, number=number, cursor=cursor))
        connection = data['repository']['pullRequest']['reviewThreads']
        threads.extend(t for t in connection['nodes'] if not t['isResolved'])
        if not connection['pageInfo']['hasNextPage']:
            break
        cursor = connection['pageInfo']['endCursor']
    authorizations = github.pages(f'issues/{number}/comments')
    security_alerts = github.pages('code-scanning/alerts?state=open')
    security_alerts += github.pages(f'code-scanning/alerts?state=open&ref=refs/pull/{number}/merge')
    return evaluate(pr, files, reviews, checks, protection, repository, threads, policy, authorizations, security_alerts)


def publish(github, number, policy):
    """Invalidate old success before inspecting mutable evidence; never bypass protection."""
    expected_app = policy.get('acceptance_app_id')
    if not isinstance(expected_app, int) or expected_app in (15368, 57789):
        raise RuntimeError('Dedicated trusted acceptance App must be configured first')
    pr = github.api(f'pulls/{number}')
    head = pr['head']['sha']
    pending = github.api('check-runs', 'POST', {
        'name': policy['acceptance_check'], 'head_sha': head, 'status': 'in_progress',
        'output': {'title': 'Rechecking live evidence', 'summary': 'Auto-merge remains held until all evidence is refreshed.'}})
    if pending.get('app', {}).get('id') != expected_app:
        raise RuntimeError('Publishing token does not belong to the trusted acceptance App')
    try:
        result = inspect(github, number, policy)
        fresh = inspect(github, number, policy)
        if result['head'] != head or fresh['head'] != head:
            raise RuntimeError('Head changed during inspection')
        result = fresh
        github.api(f'check-runs/{pending["id"]}', 'PATCH', {
            'status': 'completed', 'conclusion': 'success' if result['eligible'] else 'failure',
            'output': {'title': 'Acceptance satisfied' if result['eligible'] else 'Merge held',
                       'summary': '\n'.join(result['holds']) or 'Current-head evidence and live protections verified.'}})
        if result['eligible']:
            subprocess.run(['gh', 'pr', 'merge', str(number), '--repo', github.repo,
                            '--auto', '--squash', '--match-head-commit', head], check=True)
        else:
            disarm(github, number)
        return result
    except Exception:
        # The pending check is already blocking. Keep failure visible when possible.
        try:
            github.api(f'check-runs/{pending["id"]}', 'PATCH', {
                'status': 'completed', 'conclusion': 'failure',
                'output': {'title': 'Evidence unavailable', 'summary': 'Live refresh failed; do not merge.'}})
            disarm(github, number)
        except Exception:
            pass
        raise


def disarm(github, number):
    owner, repo = github.repo.split('/')
    data = github.graph('''query($owner:String!,$repo:String!,$number:Int!) {
      repository(owner:$owner,name:$repo) { pullRequest(number:$number) {
        id autoMergeRequest { enabledAt } } } }''', dict(owner=owner, repo=repo, number=number))
    pr = data['repository']['pullRequest']
    if pr['autoMergeRequest']:
        github.graph('''mutation($id:ID!) { disablePullRequestAutoMerge(input:{pullRequestId:$id}) {
          clientMutationId } }''', {'id': pr['id']})


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', required=True)
    parser.add_argument('--pr', type=int, required=True)
    parser.add_argument('--publish', action='store_true', help='Publish gate and enable native auto-merge')
    args = parser.parse_args()
    policy = json.loads((Path(__file__).resolve().parents[1] / '.github/merge-policy.json').read_text())
    github = GitHub(args.repo)
    result = publish(github, args.pr, policy) if args.publish else inspect(github, args.pr, policy)
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
