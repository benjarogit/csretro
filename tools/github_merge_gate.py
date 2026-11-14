#!/usr/bin/env python3
"""Inspect live GitHub evidence and enable native auto-merge without bypasses.

The Actions caller checks out main, never PR code. No game/model is launched.
Acceptance is an immutable current-head evidence manifest substantively reviewed by
CodeRabbit; it cannot replace human-only gameplay acceptance or security approval.
"""
import argparse
import fnmatch
import json
import os
import re
from pathlib import Path
import subprocess


class GitHub:
    def __init__(self, repo, token_env=None):
        self.repo = repo
        self.token_env = token_env

    def auth_env(self):
        if self.token_env is None:
            return None
        token = os.environ.get(self.token_env)
        if not token:
            raise RuntimeError('Dedicated check-publisher token is unavailable')
        return {**os.environ, 'GH_TOKEN': token}

    def api(self, path, method='GET', data=None):
        args = ['gh', 'api', f'repos/{self.repo}' + (f'/{path}' if path else ''), '--method', method]
        if data is not None:
            args += ['--input', '-']
        result = subprocess.run(args, input=json.dumps(data) if data is not None else None,
                                text=True, capture_output=True, check=True, env=self.auth_env())
        return json.loads(result.stdout) if result.stdout.strip() else None

    def pages(self, path):
        result = []
        for page in range(1, 101):
            batch = self.api(f'{path}{"&" if "?" in path else "?"}per_page=100&page={page}')
            result.extend(batch)
            if len(batch) < 100:
                return result
        raise RuntimeError('Pagination limit reached; refusing incomplete evidence')

    def graph(self, query, variables, merge_auth=False):
        result = subprocess.run(['gh', 'api', 'graphql', '--input', '-'],
                                input=json.dumps({'query': query, 'variables': variables}),
                                capture_output=True, text=True, check=True,
                                env=merge_env() if merge_auth else self.auth_env())
        data = json.loads(result.stdout)
        if data.get('errors'):
            raise RuntimeError('GraphQL evidence unavailable')
        return data['data']


def protection_holds(protection, policy):
    holds = []
    checks = protection.get('required_status_checks', {})
    required = set(policy['required_checks'])
    trusted_app = policy.get('acceptance_app_id')
    if trusted_app != 15368 or policy.get('acceptance_producer') != 'reviewed-read-only-actions':
        holds.append('Read-only independently reviewed native acceptance producer is not configured')
    if not any(c.get('context') == policy['acceptance_check'] and c.get('app_id') == trusted_app
               for c in checks.get('checks', [])):
        holds.append('Acceptance check is not bound to the existing Actions service')
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
    if security_alerts:
        holds.append('Open CodeQL/security scanning findings require resolution')
    sensitive = any(fnmatch.fnmatch(f, pattern) for f in files for pattern in policy['sensitive_paths'])
    sensitive |= bool({label.casefold() for label in policy['sensitive_labels']}
                      & {label['name'].casefold() for label in pr.get('labels', [])})
    explicit = any(c.get('user', {}).get('login') in policy['maintainers']
                   and c.get('body', '').strip() == f'CSRETRO-APPROVE-SENSITIVE {head}'
                   for c in authorizations)
    if sensitive and not explicit and not any(r['state'] == 'APPROVED' and r.get('commit_id') == head
                             and login in policy['maintainers'] and login != pr['user']['login']
                             for login, r in latest.items()):
        holds.append('Sensitive change requires explicit current-head maintainer review')
    return {'head': head, 'holds': holds, 'eligible': not holds, 'sensitive': sensitive}


def native_protection(github, policy):
    """Read native rules; existing host must audit bypasses without expanding App access."""
    effective = github.pages(f'rules/branches/{policy["base"]}')
    rule_id = policy['native_ruleset_id']
    if not any(rule.get('ruleset_id') == rule_id for rule in effective):
        raise RuntimeError('Required native ruleset is not effective on main')
    ruleset = github.api(f'rulesets/{rule_id}')
    if 'bypass_actors' not in ruleset:
        raise RuntimeError('Existing host cannot audit ruleset bypass actors; do not grant extra App rights')
    if ruleset.get('enforcement') != 'active' or ruleset.get('bypass_actors'):
        raise RuntimeError('Required native ruleset is inactive or allows bypass')
    rules = {rule['type']: rule.get('parameters', {}) for rule in ruleset['rules']}
    scan = rules.get('code_scanning', {}).get('code_scanning_tools', [])
    if not any(tool.get('tool') == 'CodeQL' and tool.get('alerts_threshold') == 'all'
               and tool.get('security_alerts_threshold') == 'all' for tool in scan):
        raise RuntimeError('Native CodeQL finding/pending-analysis protection is missing')
    status = rules.get('required_status_checks', {})
    review = rules.get('pull_request', {})
    return {
        'enforce_admins': {'enabled': not ruleset['bypass_actors']},
        'required_conversation_resolution': {'enabled': review.get('required_review_thread_resolution', False)},
        'required_linear_history': {'enabled': 'required_linear_history' in rules},
        'allow_force_pushes': {'enabled': 'non_fast_forward' not in rules},
        'allow_deletions': {'enabled': 'deletion' not in rules},
        'required_status_checks': {'strict': status.get('strict_required_status_checks_policy', False),
            'checks': [{'context': item['context'], 'app_id': item.get('integration_id')}
                       for item in status.get('required_status_checks', [])]},
        'required_pull_request_reviews': {
            'required_approving_review_count': review.get('required_approving_review_count', 0),
            'dismiss_stale_reviews': review.get('dismiss_stale_reviews_on_push', False),
            'require_last_push_approval': review.get('require_last_push_approval', False)}}


def merge_env():
    """Existing host/Actions authority arms native auto-merge, never the check-only App."""
    token = os.environ.get('GH_MERGE_TOKEN')
    return {**os.environ, 'GH_TOKEN': token} if token else None


def changed_paths(files):
    return [name for item in files
            for name in (item['filename'], item.get('previous_filename')) if name]


def inspect(github, number, policy):
    pr = github.api(f'pulls/{number}')
    protection = native_protection(github, policy)
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
    # Native code-scanning rules enforce findings and pending results server-side.
    result = evaluate(pr, files, reviews, checks, protection, repository, threads, policy, authorizations)
    import github_acceptance
    artifact = github_acceptance.read_artifact(github, pr['head']['sha'], policy)
    changed_files = github.pages(f'pulls/{number}/files')
    result['holds'] += github_acceptance.validate(pr, changed_files, reviews, checks, artifact, policy)
    acceptance = [check for check in checks if check.get('name') == policy['acceptance_check']
                  and check.get('app', {}).get('id') == policy['acceptance_app_id']
                  and check.get('head_sha') == pr['head']['sha']]
    latest = max(acceptance, key=lambda check: check['id'], default={})
    if latest.get('status') != 'completed' or latest.get('conclusion') != 'success':
        result['holds'].append('Required independent acceptance job has not succeeded')
    result['eligible'] = not result['holds']
    return result


def publish(github, number, policy):
    """Audit all live native gates and arm existing Auto-Merge; no check mutation."""
    try:
        result = inspect(github, number, policy)
        fresh = inspect(github, number, policy)
        if fresh['head'] != result['head']:
            raise RuntimeError('Head changed during inspection')
        if fresh['eligible']:
            subprocess.run(['gh', 'pr', 'merge', str(number), '--repo', github.repo,
                            '--auto', '--squash', '--match-head-commit', fresh['head']],
                           check=True, env=merge_env())
        else:
            disarm(github, number)
        return fresh
    except Exception:
        try:
            disarm(github, number)
        except Exception:
            pass
        raise


def disarm(github, number):
    owner, repo = github.repo.split('/')
    data = github.graph('''query($owner:String!,$repo:String!,$number:Int!) {
      repository(owner:$owner,name:$repo) { pullRequest(number:$number) {
        id autoMergeRequest { enabledAt } } } }''', dict(owner=owner, repo=repo, number=number), merge_auth=True)
    pr = data['repository']['pullRequest']
    if pr['autoMergeRequest']:
        github.graph('''mutation($id:ID!) { disablePullRequestAutoMerge(input:{pullRequestId:$id}) {
          clientMutationId } }''', {'id': pr['id']}, merge_auth=True)


def sandbox_policy(github, number, policy, branch, rule_id):
    """Permit an isolated, equally protected three-file documentation canary only."""
    if not isinstance(branch, str) or not re.fullmatch(r'codex/auto-merge-sandbox(?:-[a-z0-9]+)?', branch):
        raise ValueError('Only an isolated auto-merge sandbox target is allowed')
    if not isinstance(rule_id, int) or rule_id == policy['native_ruleset_id']:
        raise ValueError('A distinct test-only ruleset is required')
    candidate = {**policy, 'base': branch, 'native_ruleset_id': rule_id}
    if native_protection(github, candidate) != native_protection(github, policy):
        raise RuntimeError('Sandbox native protections differ from production requirements')
    expected = {'docs/de/maintainers/merge-canary.md', 'docs/en/maintainers/merge-canary.md',
                policy['acceptance_path']}
    paths = set(changed_paths(github.pages(f'pulls/{number}/files')))
    if paths != expected:
        raise RuntimeError('Canary must change exactly paired documentation and its acceptance manifest')
    return candidate


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', required=True)
    parser.add_argument('--pr', type=int, required=True)
    parser.add_argument('--sandbox-base', help='Isolated documentation-only test target')
    parser.add_argument('--sandbox-ruleset', type=int)
    parser.add_argument('--publish', action='store_true', help='Audit existing acceptance and enable native auto-merge')
    args = parser.parse_args()
    policy = json.loads((Path(__file__).resolve().parents[1] / '.github/merge-policy.json').read_text())
    github = GitHub(args.repo)
    if args.sandbox_base or args.sandbox_ruleset:
        policy = sandbox_policy(github, args.pr, policy, args.sandbox_base, args.sandbox_ruleset)
    result = publish(github, args.pr, policy) if args.publish else inspect(github, args.pr, policy)
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
