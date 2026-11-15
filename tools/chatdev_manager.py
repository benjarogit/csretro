"""Deterministic project-management reports; no agents, writes or processes.

The coordinator persists reports in its shared SQLite database. This component
recommends work from actual task/evidence inputs; it never performs that work.
"""
from __future__ import annotations

import hashlib
import json
from pathlib import Path
import re

from chatdev_github import classify, reconcile

from chatdev_contracts import validate_client as client_identity, SCHEMA_VERSION

TERMINAL = {'done', 'cancelled', 'completed', 'closed'}
SUSPENDED = {'paused', 'suspended', 'blocked'}
INFRASTRUCTURE = re.compile(r'chatdev|orchestrat|infrastructure|infrastruktur|agentensystem', re.I)


def _decoded(value, fallback):
    if isinstance(value, str):
        try:
            return json.loads(value)
        except ValueError:
            return fallback
    return value if value is not None else fallback


def _field(text, name):
    match = re.search(r'^- \*\*' + re.escape(name) + r':\*\*\s*(.+)$', text, re.M)
    return match.group(1) if match else None


class ProjectManager:
    def __init__(self, root):
        self.root = Path(root).resolve()

    def _evidence(self, value):
        evidence = _decoded(value, [])
        if not isinstance(evidence, list) or not evidence:
            return False
        for item in evidence:
            if not isinstance(item, dict) or not item.get('path') or not item.get('sha256'):
                return False
            path = Path(item['path'])
            if path.is_absolute() or '..' in path.parts:
                return False
            path = (self.root / path).resolve()
            try:
                if not path.is_relative_to(self.root) or not path.is_file():
                    return False
                if hashlib.sha256(path.read_bytes()).hexdigest() != item['sha256']:
                    return False
            except OSError:
                return False
        return True

    def _task(self, task):
        row = dict(task)
        row['roles'] = _decoded(row.get('roles'), [])
        row['results'] = _decoded(row.get('results'), [])
        row['dependencies'] = _decoded(row.get('dependencies'), [])
        sources = _decoded(row.get('sources'), {})
        for name, expected in sources.items():
            path = (self.root / name).resolve()
            try:
                actual = hashlib.sha256(path.read_bytes()).hexdigest() if path.is_relative_to(self.root) and path.is_file() else None
            except OSError:
                actual = None
            if not path.is_relative_to(self.root) or actual != expected:
                row['stale'] = True
        row['objective'] = row.get('objective', row.get('title', 'Untitled task'))
        row['state'] = row.get('state', 'new')
        row['priority'] = row.get('priority', 'P2')
        if row['priority'] not in ('P0', 'P1', 'P2', 'P3'):
            row['priority'] = 'P2'
        row['infrastructure'] = bool(row.get('component') in {'infrastructure', 'orchestration'} or INFRASTRUCTURE.search(row['objective']))
        # Local completion is distinct from proving the product milestone.
        rows = {r['role']: r for r in row['results'] if isinstance(r, dict) and r.get('role')}
        verified = [r for r in row['roles'] if rows.get(r, {}).get('status') == 'passed'
                    and self._evidence(rows[r].get('evidence'))]
        quality = rows.get('quality', {})
        independent = bool(quality.get('actor')) and all(
            r.get('actor') != quality['actor'] for name, r in rows.items() if name != 'quality')
        row['verified_roles'] = verified
        row['pending_roles'] = [r for r in row['roles'] if r not in verified]
        row['verified'] = bool(row['roles']) and not row.get('stale') and not row['pending_roles'] and 'quality' in verified and independent
        row['proof_state'] = 'verified-local-evidence' if row['verified'] else 'recorded-only'
        return row

    def report(self, tasks, snapshot, handoff_text, plan_text, client='cli'):
        client_identity(client)
        tasks = [self._task(t) for t in tasks]
        snapshot = dict(snapshot or {'issues': [], 'pulls': [], 'errors': ['GitHub snapshot unavailable']})
        if snapshot.get('unavailable'):
            snapshot['errors'] = [*snapshot.get('errors', []), 'GitHub unavailable: ' + str(snapshot['unavailable'])]
        reconciliation = reconcile(snapshot)
        gate_text = _field(handoff_text, 'Gate')
        gate = re.search(r'\bG\d+[a-z]?\b', gate_text or '')
        gate_name = gate.group(0) if gate else None
        # Link only the active gate's heading/table line, not another gate list.
        plan_match = next(((n, line) for n, line in enumerate(plan_text.splitlines(), 1)
                           if gate_name and re.search(r'(?<!\w)' + re.escape(gate_name) + r'(?!\w)', line)
                           and (line.startswith('#') or line.startswith('|'))), None)
        milestone = {'gate': gate_name, 'handoff_claim': gate_text,
                     'current_work': _field(handoff_text, 'Current work'),
                     'critical_next': _field(handoff_text, 'Critical next port work'),
                     'next_task': (re.search(r'^## Next task\s*\n(.*?)(?=^## |\Z)',handoff_text,re.M|re.S).group(1).strip() if re.search(r'^## Next task\s*\n(.*?)(?=^## |\Z)',handoff_text,re.M|re.S) else None),
                     'source': 'HANDOFF.md', 'plan_source': 'docs/plan/csretro-plan.md',
                     'plan_excerpt': {'line': plan_match[0], 'text': plan_match[1]} if plan_match else None,
                     'completion_verified': False,
                     'reason': 'Task records and menus/callbacks alone do not prove the authoritative gameplay milestone'}
        active = [t for t in tasks if t['state'] not in TERMINAL]
        completed = {str(t['id']) for t in tasks if t['state'] in {'done', 'completed'} and t['verified']}
        blockers = []
        recorded_issues = []
        for task in active:
            dependencies = [str(x) for x in task['dependencies'] if str(x) not in completed]
            if dependencies:
                blockers.append({'task': task['id'], 'kind': 'dependency', 'depends_on': dependencies})
            if task.get('stale'):
                blockers.append({'task': task['id'], 'kind': 'stale-authority', 'action': 'reconcile-authority'})
            for result in task['results']:
                if result.get('status') in {'failed', 'blocked'}:
                    blockers.append({'task': task['id'], 'kind': 'role', 'role': result['role'],
                                     'summary': result.get('summary', '')})
        if reconciliation['failed_ci']:
            blockers.append({'kind': 'ci', 'action': reconciliation['repair'],
                             'checks': reconciliation['failed_ci']})
        for review in reconciliation['reviews']:
            if any(r.get('state') == 'CHANGES_REQUESTED' for r in review['reviews']):
                blockers.append({'kind': 'review', 'pr': review['pr'], 'action': 'address-review'})
        for edge in reconciliation['dependencies']:
            blockers.append({'kind':'issue-dependency','issue':edge['issue'],'depends_on':edge['depends_on'],'action':'verify-dependency-status'})
        for issue in reconciliation['blocked']:
            blockers.append({'kind': 'issue', 'number': issue})
        # Snapshot CI/reviews are useful for triage, never evidence of effective
        # branch protection or acceptance. The trusted GitHub gate owns that
        # live verification; this report does not grant merge permissions.
        holds = []
        sensitive = {'security', 'release', 'deployment', 'production', 'paid', 'paid-service',
                     'permissions', 'manual-approval', 'requires-maintainer-approval'}
        try:
            policy = json.loads((self.root / '.github/merge-policy.json').read_text())
            labels = policy['sensitive_labels']
            if not isinstance(labels, list) or not all(isinstance(label, str) for label in labels):
                raise ValueError('Invalid sensitive labels')
            sensitive.update(label.casefold() for label in labels)
        except (OSError, ValueError, KeyError, TypeError):
            # This advisory never authorizes merging. Missing/malformed policy
            # still requires the live gate; preserve conservative human holds.
            pass
        for pr in snapshot.get('pulls', []):
            if str(pr.get('state', 'open')).lower() != 'open':
                continue
            labels = {str(label.get('name', '')) if isinstance(label, dict) else str(label)
                      for label in pr.get('labels', [])}
            needs_approval = bool({label.casefold() for label in labels} & sensitive)
            holds.append({'kind': 'sensitive-merge-approval' if needs_approval else 'protected-auto-merge-verification',
                          'pr': pr['number'], 'action': 'request-explicit-human-approval' if needs_approval
                          else 'verify-protected-auto-merge', 'automatic_merge': False,
                          'policy_source': '.github/merge-policy.json',
                          'verifier': 'tools/github_merge_gate.py',
                          'protection_verified': False,
                          'reason': 'Sensitive changes require explicit scoped authorization' if needs_approval
                          else 'Live protection, acceptance and review verification required; CI alone is insufficient'})
        backlog = [{'kind': 'task', 'id': t['id'], 'title': t['objective'], 'priority': t['priority'],
                    'state': t['state'], 'infrastructure': t['infrastructure']} for t in active]
        for issue in snapshot.get('issues', []):
            if str(issue.get('state', 'open')).lower() == 'closed':
                continue
            classification = classify(issue['title'], issue.get('body') or '', issue.get('labels', []))
            if classification['status'] == 'done':
                recorded_issues.append(issue['number'])
                continue
            backlog.append({'kind': 'issue', 'id': issue['number'], 'title': issue['title'],
                            **classification})
        backlog.sort(key=lambda x: (int(x['priority'][1]), x['kind'], str(x['id'])))
        collaboration = [{'task': t['id'], 'role': role,
                          'status': next((r['status'] for r in t['results'] if r.get('role') == role), 'pending'),
                          'evidence_verified': role in t['verified_roles'],
                          'actor': next((r.get('actor') for r in t['results'] if r.get('role') == role), None)}
                         for t in active for role in t['roles']]
        # Resume existing infrastructure before proposing any new development.
        ordered = sorted(active, key=lambda t: (not t['infrastructure'], int(t['priority'][1]),
                                               t['state'] not in SUSPENDED, str(t['id'])))
        next_step = {'action': 'review-authoritative-next', 'reason': 'No unfinished local task',
                     'source': 'HANDOFF.md', 'text': milestone['next_task'] or milestone['current_work'] or milestone['critical_next']}
        if ordered:
            task = ordered[0]
            relevant = [b for b in blockers if b.get('task') == task['id']]
            if task.get('stale'):
                action = 'reconcile-authority'
            elif any(b['kind'] == 'dependency' for b in relevant):
                action = 'resolve-dependency'
            elif relevant:
                action = 'resolve-role-blocker'
            elif reconciliation['failed_ci']:
                action = 'resolve-ci'
            elif any(b['kind'] == 'review' for b in blockers):
                action = 'address-review'
            elif holds:
                action = holds[0]['action']
            elif task['state'] in SUSPENDED:
                action = 'resume-existing-task'
            elif task['pending_roles']:
                action = 'continue-role'
            elif task['verified']:
                action = 'verify-and-complete-existing-task'
            else:
                action = 'request-independent-quality-review'
            next_step = {'action': action, 'task': task['id'], 'objective': task['objective'],
                         'role': 'repository' if action in {'resolve-ci', 'address-review'} else task['pending_roles'][0] if task['pending_roles'] else 'quality',
                         'reason': 'Continue persisted infrastructure' if task['infrastructure'] else 'Continue highest-priority existing work',
                         'delegation': 'recommendation-only; orchestrator coordinates assigned roles'}
            if action in {'request-explicit-human-approval', 'verify-protected-auto-merge'}:
                next_step['related_task'] = next_step.pop('task')
                next_step['pr'] = holds[0]['pr']
                next_step['role'] = 'repository'
        elif holds:
            next_step = {'action': holds[0]['action'], 'pr': holds[0]['pr'],
                         'reason': holds[0]['reason']}
        elif reconciliation['failed_ci']:
            next_step = {'action': 'resolve-ci', 'role': 'repository', 'reason': 'Recorded CI failure requires triage'}
        elif backlog:
            issue = backlog[0]
            next_step = {'action': 'resolve-issue-dependency' if any(b.get('kind')=='issue-dependency' and b.get('issue')==issue['id'] for b in blockers) else 'resolve-issue-blocker' if issue.get('status') == 'blocked' else 'analyze-existing-issue', 'issue': issue['id'],
                         'reason': f'Highest existing backlog priority {issue["priority"]}'}
        return {'schema_version':SCHEMA_VERSION,'role': 'project_manager', 'milestone': milestone,
                'progress': [{'task': t['id'], 'state': t['state'], 'proof_state': t['proof_state'],
                              'verified_roles': t['verified_roles'], 'pending_roles': t['pending_roles']}
                             for t in tasks],
                'backlog': backlog, 'collaboration': collaboration, 'blockers': blockers,
                'holds': holds, 'recorded_done_issues': recorded_issues, 'github': reconciliation, 'next': next_step,
                'coordination': {'client': client, 'shared_store': '.local/chatdev/memory.sqlite3',
                                 'same_project_state': True, 'processes_started': False,
                                 'automatic_merge': False, 'persistence_owner': 'orchestrator'}}
