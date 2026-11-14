"""Bounded GitHub coordination. Reads use gh; mutations are explicit and guarded.

The upstream register remains versions.lock.json and scripts/upstream-watch.sh.
This module neither cherry-picks code nor executes arbitrary repair commands.
"""
from __future__ import annotations

from datetime import datetime, timezone
import hashlib
from difflib import SequenceMatcher
import json
from pathlib import Path
import re
import subprocess
from urllib.parse import quote, urlsplit

CATEGORIES = ('bug', 'feature', 'enhancement', 'upstream', 'maintenance',
              'documentation', 'testing', 'security', 'question')
STATUSES = ('new', 'analyzed', 'planned', 'in-progress', 'blocked', 'in-review', 'done')
RISKY = {'force-push', 'history-rewrite', 'delete', 'credentials', 'release',
         'branch-protection', 'paid-service', 'unclear-license', 'main-merge', 'game'}


def guard_action(action: str) -> dict:
    """Risky operations are deliberately unavailable from this entry point."""
    allowed = action in {'snapshot', 'upstreams', 'issue-create', 'issue-close', 'reconcile'}
    return {'action': action, 'allowed': allowed,
            'reason': '' if allowed else 'Requires a separate explicit maintainer-controlled action'}


def _title(value):
    return ' '.join(re.findall(r'\w+', value.casefold()))


def classify(title, body='', labels=()):
    names = {str(x.get('name', '')) if isinstance(x, dict) else str(x) for x in labels}
    names = {n.casefold() for n in names}
    text = f'{title} {body}'.casefold()
    category = next((c for c in CATEGORIES if c in names or f'type:{c}' in names), None)
    if not category:
        category = next((c for c, pattern in (
            ('security', r'\b(security|vulnerability|cve-\d+)\b'),
            ('upstream', r'\bupstream\b'), ('bug', r'\b(bug|crash|regression|broken)\b'),
            ('documentation', r'\b(docs?|documentation|translation)\b'),
            ('testing', r'\b(tests?|coverage)\b'), ('feature', r'\bfeature\b'),
            ('enhancement', r'\b(improve|enhancement)\b'), ('question', r'\bquestion\b'))
            if re.search(pattern, text)), 'maintenance')
    priority = next((p for p in ('P0', 'P1', 'P2', 'P3')
                     if p.casefold() in names or f'priority:{p.casefold()}' in names), 'P2')
    status = next((s for s in reversed(STATUSES) if s in names or f'status:{s}' in names), 'new')
    return {'category': category, 'priority': priority, 'status': status}


def reconcile(snapshot, attempts=0, now=None):
    """Report issues needing attention without silently changing their status."""
    now = now or datetime.now(timezone.utc)
    duplicates, seen, blocked, stale = [], {}, [], []
    related, dependencies, orphaned = [], [], []
    issues = snapshot.get('issues', [])
    for issue in issues:
        key = _title(issue['title'])
        number = issue['number']
        if key in seen:
            duplicates.append([seen[key], number])
        else:
            seen[key] = number
        state = classify(issue['title'], issue.get('body') or '', issue.get('labels', []))
        if state['status'] == 'blocked':
            blocked.append(number)
        refs = re.findall(r'(?:blocked by|depends on)\s+#(\d+)', issue.get('body') or '', re.I)
        if refs:
            dependencies.append({'issue': number, 'depends_on': [int(x) for x in refs]})
        if state['status'] == 'in-progress' and not any(
                f'#{number}' in (pr.get('body') or '') for pr in snapshot.get('pulls', [])):
            orphaned.append(number)
        stamp = issue.get('updated_at', issue.get('updatedAt'))
        if stamp:
            try:
                if (now - datetime.fromisoformat(stamp.replace('Z', '+00:00'))).days >= 30:
                    stale.append(number)
            except (ValueError, TypeError):
                pass
    for i, left in enumerate(issues):
        for right in issues[i + 1:]:
            a, b = _title(left['title']), _title(right['title'])
            if a != b and len(set(a.split()) & set(b.split())) >= 3 and SequenceMatcher(None, a, b).ratio() >= .8:
                related.append([left['number'], right['number']])
    failed, reviews, pending = [], [], []
    errors = list(snapshot.get('errors', []))
    open_pulls = [pr for pr in snapshot.get('pulls', []) if str(pr.get('state', 'open')).lower() == 'open']
    for pr in open_pulls:
        for check in pr.get('checks', []):
            if not check.get('conclusion'): pending.append({'pr':pr['number'],'check':check.get('name'),'status':check.get('status','unknown')})
            if (check.get('conclusion') or '').lower() in {'failure', 'timed_out', 'cancelled', 'action_required'}:
                failed.append({'pr': pr['number'], 'check': check.get('name'), 'url': check.get('html_url')})
        if pr.get('reviews'):
            history = pr['reviews']
            # Draft review IDs can predate submission. Prefer submission time
            # for decisions, then numeric IDs, then the REST history sequence.
            formal = [r for r in history if str(r.get('state', '')).upper()
                      in {'APPROVED', 'CHANGES_REQUESTED', 'DISMISSED'}]
            timed = False
            if formal and all(r.get('submitted_at') for r in formal):
                try:
                    history = sorted(history, key=lambda r: datetime.fromisoformat(
                        r['submitted_at'].replace('Z', '+00:00')).timestamp()
                        if r.get('submitted_at') else float('-inf'))
                    timed = True
                except (TypeError, ValueError):
                    pass
            if not timed and all(type(r.get('id')) is int and r['id'] > 0 for r in history):
                history = sorted(history, key=lambda r: r['id'])
            effective, unidentified, comments = {}, [], []
            for review in history:
                state = str(review.get('state', '')).upper()
                user = review.get('user') or {}
                identity = ('id', user['id']) if type(user.get('id')) is int and user['id'] > 0 else (
                    ('login', user['login'].casefold()) if isinstance(user.get('login'), str) and user['login'] else None)
                normalized = {**review, 'state': state}
                if identity is None:
                    errors.append(f'PR {pr["number"]}: review {review.get("id", "unknown")} reviewer identity unavailable; manual review required')
                    # Never let an anonymous approval dismiss another review.
                    unidentified.append({**normalized, 'identity_unavailable': True})
                    continue
                if state in {'APPROVED', 'CHANGES_REQUESTED', 'DISMISSED'}:
                    effective[identity] = normalized
                else:
                    # COMMENTED/PENDING are useful context, not decisions.
                    comments.append(normalized)
            reviews.append({'pr': pr['number'], 'reviews': [*effective.values(), *unidentified],
                            'comments': comments, 'history_count': len(history),
                            'manual_review_required': bool(unidentified)})
    default_branch = snapshot.get('default_branch')
    runs = snapshot.get('runs', [])
    if runs and not default_branch:
        errors.append('CI: default branch identity unavailable; main-branch status is unknown')
        pending.append({'status': 'unknown', 'reason': 'default branch identity unavailable'})
    active_heads = {(pr.get('head', {}).get('ref'), pr.get('head', {}).get('sha'))
                    for pr in open_pulls if pr.get('head', {}).get('ref') and pr.get('head', {}).get('sha')}
    for pr in open_pulls:
        if runs and not (pr.get('head', {}).get('ref') and pr.get('head', {}).get('sha')):
            errors.append(f'PR {pr["number"]}: CI branch/head identity unavailable')
            pending.append({'pr': pr['number'], 'status': 'unknown', 'reason': 'branch/head identity unavailable'})
    seen_workflows = set()
    for run in runs:  # GitHub REST returns newest runs first.
        branch = run.get('head_branch')
        workflow = run.get('workflow_id') or run.get('path') or run.get('name')
        if not branch or not workflow:
            errors.append(f'CI run {run.get("id", "unknown")}: workflow or branch identity unavailable')
            pending.append({'run': run.get('id'), 'status': 'unknown', 'url': run.get('html_url')})
            continue
        if branch != default_branch and (branch, run.get('head_sha')) not in active_heads:
            continue
        # Two open fork PRs can use the same branch name with different heads.
        key = (workflow, branch, None if branch == default_branch else run.get('head_sha'))
        if key in seen_workflows:
            continue
        seen_workflows.add(key)
        conclusion = run.get('conclusion')
        if run.get('status') not in (None, 'completed') or not conclusion:
            pending.append({'run': run.get('id'), 'status': run.get('status') or 'unknown',
                            'url': run.get('html_url')})
        elif conclusion in {'failure', 'timed_out', 'action_required', 'cancelled'}:
            failed.append({'run': run.get('id'), 'url': run.get('html_url')})
    return {'duplicates': duplicates, 'related': related, 'dependencies': dependencies,
            'orphaned': orphaned, 'blocked': blocked, 'stale': stale,
            'reviews': reviews, 'failed_ci': failed, 'pending_ci':pending, 'errors': errors,
            'repair': 'escalate' if failed and attempts >= 2 else 'assign-test-and-repository-review' if failed else 'none',
            'retry_limit': 2, 'remaining_attempts': max(0, 2 - attempts)}


def upstream_candidate(existing, *, source, reference, commit, components, relevance,
                       compatibility, license_review, priority='P2', status='analyzed',
                       dependencies=(), strategy='review'):
    """Deduplicate a reviewed observation; never import or maintain another pin list."""
    if priority not in {'P0', 'P1', 'P2', 'P3'} or status not in STATUSES:
        raise ValueError('Invalid priority or status')
    if not all((source, reference, commit, relevance, compatibility, license_review)):
        raise ValueError('An upstream assessment requires provenance and reviews')
    for candidate in existing:
        if candidate.get('reference') == reference or (candidate.get('source'), candidate.get('commit')) == (source, commit):
            return {'existing': True, 'candidate': candidate}
    return {'existing': False, 'candidate': dict(source=source, reference=reference, commit=commit,
            components=list(components), relevance=relevance, compatibility=compatibility,
            license_review=license_review, priority=priority, status=status,
            dependencies=list(dependencies), strategy=strategy,
            import_allowed=compatibility == 'compatible' and license_review == 'approved')}


class GitHub:
    def __init__(self, root, runner=None):
        self.root = Path(root).resolve()
        self.runner = runner or subprocess.run
        self._repo = None
        self._default_branch = None

    def _call(self, args):
        try:
            result = self.runner(['gh', *args], cwd=self.root, input=None, text=True,
                                 capture_output=True, check=False, timeout=30)
        except subprocess.TimeoutExpired as error:
            raise RuntimeError('GitHub request timed out; re-read remote state before retrying') from error
        if result.returncode:
            # Do not persist stderr: authentication errors can contain local data.
            raise RuntimeError(f'GitHub command unavailable (exit {result.returncode}): {args[0]}')
        return json.loads(result.stdout or 'null')

    @property
    def repo(self):
        if self._repo is None:
            metadata = self._call(['repo', 'view', '--json', 'nameWithOwner,defaultBranchRef'])
            self._repo = metadata['nameWithOwner']
            self._default_branch = (metadata.get('defaultBranchRef') or {}).get('name')
            if not re.fullmatch(r'[\w.-]+/[\w.-]+', self._repo):
                raise ValueError('Invalid GitHub repository identity')
        return self._repo

    def _pages(self, endpoint):
        pages = self._call(['api', endpoint, '--paginate', '--slurp'])
        return [item for page in pages for item in page]

    def upstreams(self):
        """Read bounded upstream evidence from the existing lock, never import it.

        Each collection is capped at three 100-entry pages per source. Explicit
        truncation/errors mean incomplete coverage, never "up to date". Existing
        sync-upstream/upstream-watch remain the pin and issue workflows.
        """
        result = {'register': 'versions.lock.json', 'sources': [], 'candidates': [],
                  'errors': [], 'mutations': False}
        lock = json.loads((self.root / 'versions.lock.json').read_text())
        sources = {}
        for section in ('upstreams', 'server_toolchain', 'server_runtime', 'references'):
            for name, entry in lock.get(section, {}).items():
                location = entry.get('repository') or entry.get('url', '')
                url = urlsplit(location)
                parts = url.path.strip('/').split('/')
                if url.hostname != 'github.com' or len(parts) < 2:
                    continue
                repo = '/'.join(parts[:2]).removesuffix('.git')
                if not re.fullmatch(r'[\w.-]+/[\w.-]+', repo):
                    continue
                source = sources.setdefault(repo.casefold(), {
                    'repository': repo, 'pins': [], 'versions': [], 'components': [],
                    'provenance': [], 'reference_only': True})
                source['reference_only'] &= section == 'references'
                source['provenance'].append(f'versions.lock.json#{section}.{name}')
                component = entry.get('prefix') or entry.get('source_prefix') or name
                if component not in source['components']:
                    source['components'].append(component)
                pin = entry.get('commit') or entry.get('source_commit')
                if pin and pin not in source['pins']:
                    source['pins'].append(pin)
                if entry.get('version') and entry['version'] not in source['versions']:
                    source['versions'].append(entry['version'])
        result['sources'] = list(sources.values())

        def error(source, endpoint, message):
            result['errors'].append({'source': source['repository'], 'endpoint': endpoint,
                                     'error': message})

        def pages(source, endpoint, key=None):
            items = []
            for page in range(1, 4):
                separator = '&' if '?' in endpoint else '?'
                try:
                    payload = self._call(['api', f'{endpoint}{separator}per_page=100&page={page}'])
                    batch = payload[key] if key else payload
                    if not isinstance(batch, list):
                        raise ValueError('Expected a GitHub collection')
                    items.extend(batch)
                    if len(batch) < 100:
                        break
                except (RuntimeError, OSError, ValueError, KeyError, TypeError) as exc:
                    error(source, endpoint, str(exc))
                    break
            else:
                error(source, endpoint, 'Truncated after 300 entries; review remaining upstream pages manually')
            return items

        def add(source, kind, reference, revision, title, security=False):
            if not reference or not revision or revision in source['pins']:
                return
            existing = next((x for x in result['candidates'] if x['source'] == source['repository'] and
                             (x['reference'] == reference or x['commit'] == revision)), None)
            if existing:
                if reference not in existing['references']:
                    existing['references'].append(reference)
                if kind not in existing['observations']:
                    existing['observations'].append(kind)
                if security:
                    existing['priority'] = 'P1'
                    existing['classification'] = 'security'
                return
            assessment = upstream_candidate([x for x in result['candidates'] if x['source'] == source['repository']],
                source=source['repository'],
                reference=reference, commit=revision, components=source['components'],
                relevance=title or kind, compatibility='pending', license_review='pending',
                priority='P1' if security else 'P2', status='new', strategy='review')['candidate']
            assessment.update(classification='security' if security else 'upstream', references=[reference],
                              observations=[kind], provenance=source['provenance'],
                              reference_only=source['reference_only'], import_allowed=False)
            result['candidates'].append(assessment)

        for source in result['sources']:
            repo = source['repository']
            endpoint = f'repos/{repo}'
            try:
                metadata = self._call(['api', endpoint])
                branch = metadata['default_branch']
                if not isinstance(branch, str) or not branch:
                    raise ValueError('Default branch unavailable')
                source['default_branch'] = branch
            except (RuntimeError, OSError, ValueError, KeyError, TypeError) as exc:
                error(source, endpoint, str(exc))
                continue
            source['commit_scope'] = 'since recorded pins' if source['pins'] else 'latest 20 commits, no recorded baseline'
            if not source['pins']:
                endpoint = f'repos/{repo}/commits?sha={quote(branch, safe="")}&per_page=20'
                try:
                    recent = self._call(['api', endpoint])
                    if not isinstance(recent, list):
                        raise ValueError('Expected a GitHub commit snapshot')
                    for item in recent[:20]:
                        title = item.get('commit', {}).get('message', '').split('\n', 1)[0]
                        add(source, 'commit', item.get('html_url'), item.get('sha'), title,
                            classify(title)['category'] == 'security')
                except (RuntimeError, OSError, ValueError, KeyError, TypeError) as exc:
                    error(source, endpoint, str(exc))
            compared = []
            commit_items = {}
            for pin in source['pins']:
                endpoint = f'repos/{repo}/compare/{quote(pin, safe="")}...{quote(branch, safe="")}'
                commits = pages(source, endpoint, 'commits')
                compared.append({x['sha'] for x in commits})
                commit_items.update({x['sha']: x for x in commits})
            # A commit must be beyond every recorded pin, not merely an older
            # component pin sharing the same upstream repository.
            unseen = set.intersection(*compared) if compared else set()
            for sha in sorted(unseen):
                item = commit_items[sha]
                message = item.get('commit', {}).get('message', '').split('\n', 1)[0]
                add(source, 'commit', item.get('html_url'), sha, message,
                    classify(message)['category'] == 'security')
            for item in pages(source, f'repos/{repo}/pulls?state=open'):
                sha = item.get('head', {}).get('sha')
                if sha in source['pins']:
                    continue
                title = item.get('title', '')
                labels = item.get('labels', [])
                add(source, 'pull-request', item.get('html_url'), sha, title,
                    classify(title, labels=labels)['category'] == 'security')
            for item in pages(source, f'repos/{repo}/releases'):
                tag = item.get('tag_name')
                target = item.get('target_commitish', '')
                if item.get('draft') or tag in source['versions'] or target in source['pins']:
                    continue
                revision = target if re.fullmatch(r'[0-9a-fA-F]{40}', target) else f'tag:{tag}' if tag else None
                add(source, 'release', item.get('html_url'), revision,
                    item.get('name') or tag, classify(item.get('body') or '')['category'] == 'security')
            for item in pages(source, f'repos/{repo}/security-advisories?state=published'):
                add(source, 'security-advisory', item.get('html_url'),
                    f'advisory:{item["ghsa_id"]}' if item.get('ghsa_id') else None,
                    item.get('summary'), True)
        return result

    def snapshot(self):
        data = {'issues': [], 'pulls': [], 'runs': [], 'labels': [], 'projects': [], 'errors': []}
        try:
            repo = self.repo
            data['default_branch'] = self._default_branch
            if not self._default_branch:
                data['errors'].append('Repository default branch identity unavailable')
        except (RuntimeError, KeyError, ValueError, OSError) as error:
            data['errors'].append(str(error))
            return data
        for key, endpoint in [('issues', f'repos/{repo}/issues?state=open&per_page=100'),
                              ('pulls', f'repos/{repo}/pulls?state=open&per_page=100'),
                              ('labels', f'repos/{repo}/labels?per_page=100')]:
            try:
                data[key] = self._pages(endpoint)
            except (RuntimeError, OSError, ValueError) as error:
                data['errors'].append(f'{key}: {error}')
        data['issues'] = [x for x in data['issues'] if 'pull_request' not in x]
        for pr in data['pulls']:
            try:
                pr['reviews'] = self._pages(f'repos/{repo}/pulls/{int(pr["number"])}/reviews?per_page=100')
                pages = self._call(['api', f'repos/{repo}/commits/{pr["head"]["sha"]}/check-runs?per_page=100', '--paginate', '--slurp'])
                pr['checks'] = [check for page in pages for check in page['check_runs']]
            except (RuntimeError, OSError, ValueError, KeyError) as error:
                data['errors'].append(f'PR {pr["number"]}: {error}')
        try:
            data['runs'] = self._call(['api', f'repos/{repo}/actions/runs?per_page=100'])['workflow_runs']
        except (RuntimeError, OSError, ValueError, KeyError) as error:
            data['errors'].append(f'CI: {error}')
        try:
            projects = self._call(['api', 'graphql', '-f',
                f'query=query {{ repository(owner:"{repo.split("/")[0]}",name:"{repo.split("/")[1]}") {{ projectsV2(first:100) {{ nodes {{ number title url closed }} pageInfo {{ hasNextPage }} }} }} }}'])
            result = projects['data']['repository']['projectsV2']
            data['projects'] = result['nodes']
            if result['pageInfo']['hasNextPage']:
                data['errors'].append('Projects: more than 100 projects; complete reconciliation unavailable')
        except (RuntimeError, OSError, ValueError, KeyError, TypeError) as error:
            data['errors'].append(f'Projects unavailable: {error}')
        return data

    def ensure_issue(self, title, body, category='maintenance', priority='P2', apply=False):
        if category not in CATEGORIES or priority not in {'P0', 'P1', 'P2', 'P3'} or not title.strip():
            raise ValueError('Invalid issue classification')
        # Include closed issues: closed does not establish that implementation exists.
        issues = self._pages(f'repos/{self.repo}/issues?state=all&per_page=100')
        key = _title(title)
        existing = [x for x in issues if 'pull_request' not in x and _title(x['title']) == key]
        if existing:
            return {'action': 'existing', 'issues': existing, 'needs_verification': True}
        available = self._pages(f'repos/{self.repo}/labels?per_page=100')
        names = {x['name'].casefold(): x['name'] for x in available}
        labels = []
        for variants in ((f'type:{category}', category), (f'priority:{priority}', priority), ('status:new', 'new')):
            found = next((names[v.casefold()] for v in variants if v.casefold() in names), None)
            if found:
                labels.append(found)
        result = {'action': 'create' if apply else 'preview', 'title': title, 'body': body, 'labels': labels}
        if apply:
            payload = {'title': title, 'body': body, 'labels': labels}
            # --input - keeps shell syntax and body text out of command evaluation.
            try:
                response = self.runner(['gh', 'api', f'repos/{self.repo}/issues', '--method', 'POST', '--input', '-'],
                    cwd=self.root, input=json.dumps(payload), text=True, capture_output=True, check=False, timeout=30)
            except subprocess.TimeoutExpired as error:
                raise RuntimeError('Issue creation outcome unknown; re-read remote state before retrying') from error
            if response.returncode:
                raise RuntimeError('Issue creation failed; re-read remote state before retrying')
            result['issue'] = json.loads(response.stdout)
        return result

    def _proof(self, evidence):
        if not isinstance(evidence, dict) or not evidence.get('path') or not evidence.get('sha256'):
            return False
        path = Path(evidence['path'])
        if path.is_absolute() or '..' in path.parts:
            return False
        path = (self.root / path).resolve()
        try:
            path.relative_to(self.root)
            return path.is_file() and hashlib.sha256(path.read_bytes()).hexdigest() == evidence['sha256']
        except (ValueError, OSError):
            return False

    def close_issue(self, number, acceptance, quality_review, checks, apply=False):
        """Close only with hashed local evidence, independent review and live PR checks.

        checks names a linked pull request: {'pr': 12}. Claimed check results are
        never accepted from the caller. Required reviews/tests remain human work.
        """
        if not acceptance or not all(x.get('passed') is True and self._proof(x.get('evidence')) for x in acceptance):
            raise ValueError('Verified acceptance evidence required')
        if not quality_review.get('approved') or not quality_review.get('independent') or not self._proof(quality_review.get('evidence')):
            raise ValueError('Independent quality review evidence required')
        pr_number = int(checks['pr'])
        pr = self._call(['pr', 'view', str(pr_number), '--json', 'state,statusCheckRollup,closingIssuesReferences'])
        linked = any(int(x['number']) == int(number) for x in pr.get('closingIssuesReferences', []))
        green = pr.get('statusCheckRollup') and all(x.get('conclusion') == 'SUCCESS' or x.get('state') == 'SUCCESS' for x in pr['statusCheckRollup'])
        if pr.get('state') != 'MERGED' or not linked or not green:
            raise ValueError('A merged linked PR with current green checks is required')
        if apply:
            self._call(['api', f'repos/{self.repo}/issues/{int(number)}', '--method', 'PATCH', '-f', 'state=closed'])
        return {'action': 'closed' if apply else 'preview-close', 'number': int(number), 'pr': pr_number}
