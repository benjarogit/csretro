"""Functional project-manager recommendations from real input state."""
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
from chatdev_manager import ProjectManager

HANDOFF = '''# Handoff
- **Gate:** G07 — in progress. The first runnable version is not achieved.
- **Current work:** ChatDev infrastructure interrupted before review.
- **Critical next port work:** Port existing gameplay on Xash first.
'''
PLAN = '''## Gates
| G06 | Server | done |
| G07 | Existing CS behavior on Xash | open |
| G08 | Native phase | later |
'''


class ManagerTests(unittest.TestCase):
    def setUp(self):
        Path('/tmp/opencode').mkdir(parents=True, exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(dir='/tmp/opencode')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.manager = ProjectManager(self.root)
        self.snapshot = {'issues': [], 'pulls': [], 'runs': [], 'errors': []}

    def report(self, tasks=(), **kwargs):
        return self.manager.report(tasks, self.snapshot, HANDOFF, PLAN, **kwargs)

    def task(self, ident=1, **kwargs):
        return dict(id=ident, objective='Finish ChatDev orchestration', state='in-progress',
                    roles=['developer', 'quality'], **kwargs)

    def proof(self):
        path = self.root / 'evidence.txt'
        path.write_text('actual retained test evidence')
        return [{'path': 'evidence.txt', 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}]

    def test_uses_authority_active_gate_without_copying_a_gate_list(self):
        result = self.report()
        self.assertEqual(result['milestone']['gate'], 'G07')
        self.assertEqual(result['milestone']['plan_excerpt']['line'], 3)
        self.assertFalse(result['milestone']['completion_verified'])
        self.assertNotIn('G06', json.dumps(result))
        self.assertEqual(result['next']['text'], 'ChatDev infrastructure interrupted before review.')

    def test_authoritative_next_task_overrides_old_port_summary(self):
        handoff = HANDOFF + '\n## Next task\nFinish the approved infrastructure review.\n\n## History\nOld callback work.\n'
        result = self.manager.report([], self.snapshot, handoff, PLAN, client='external-ide')
        self.assertEqual(result['next']['text'], 'Finish the approved infrastructure review.')
        self.assertEqual(result['coordination']['client'], 'external-ide')
        self.assertFalse(result['coordination']['processes_started'])

    def test_suspended_existing_infrastructure_precedes_new_game_work(self):
        paused = self.task()
        paused['state'] = 'suspended'
        game = self.task(2)
        game.update(objective='New rendering feature', priority='P0')
        self.snapshot['issues'] = [{'number': 99, 'title': 'New gameplay request', 'labels': ['P0']}]
        result = self.report([game, paused])
        self.assertEqual(result['next']['task'], 1)
        self.assertEqual(result['next']['action'], 'resume-existing-task')
        self.assertEqual(result['next']['role'], 'developer')

    def test_done_task_without_proof_is_recorded_and_never_rescheduled(self):
        task = self.task()
        task['state'] = 'done'
        result = self.report([task])
        self.assertEqual(result['progress'][0]['proof_state'], 'recorded-only')
        self.assertFalse(result['backlog'])
        self.assertNotIn('task', result['next'])

    def test_verified_progress_requires_current_hashes_and_independent_quality(self):
        task = self.task()
        proof = self.proof()
        task['results'] = [{'role': 'developer', 'actor': 'codex:worker', 'status': 'passed', 'evidence': proof},
                           {'role': 'quality', 'actor': 'cursor:reviewer', 'status': 'passed', 'evidence': proof}]
        result = self.report([task])
        self.assertEqual(result['progress'][0]['proof_state'], 'verified-local-evidence')
        self.assertEqual(result['next']['action'], 'verify-and-complete-existing-task')
        self.assertFalse(result['milestone']['completion_verified'])
        (self.root / 'evidence.txt').write_text('changed after review')
        self.assertEqual(self.report([task])['progress'][0]['proof_state'], 'recorded-only')
        proof = self.proof()
        task['results'][1]['actor'] = 'codex:worker'
        self.assertEqual(self.report([task])['next']['action'], 'request-independent-quality-review')

    def test_json_db_rows_and_role_assignments_remain_visible(self):
        task = self.task()
        task['roles'] = json.dumps(task['roles'])
        task['results'] = json.dumps([{'role': 'developer', 'actor': 'cursor:worker',
                                      'status': 'blocked', 'summary': 'Dependency unavailable', 'evidence': []}])
        result = self.report([task])
        self.assertEqual(result['collaboration'][0]['actor'], 'cursor:worker')
        self.assertEqual(result['next']['action'], 'resolve-role-blocker')
        self.assertFalse(result['collaboration'][0]['evidence_verified'])

    def test_dependencies_and_changed_authorities_block_unsafe_continuation(self):
        task = self.task(dependencies=[2])
        self.assertEqual(self.report([task])['next']['action'], 'resolve-dependency')
        task['stale'] = True
        self.assertEqual(self.report([task])['next']['action'], 'reconcile-authority')
        task['stale'] = False
        done = self.task(2)
        done['state'] = 'done'
        self.assertEqual(self.report([task, done])['next']['action'], 'resolve-dependency')
        proof = self.proof()
        done['results'] = [{'role': 'developer', 'actor': 'worker', 'status': 'passed', 'evidence': proof},
                           {'role': 'quality', 'actor': 'reviewer', 'status': 'passed', 'evidence': proof}]
        self.assertEqual(self.report([task, done])['next']['action'], 'continue-role')

    def test_ci_reviews_and_fresh_merge_approval_generate_actionable_holds(self):
        self.snapshot['pulls'] = [{'number': 9, 'state': 'open',
                                  'reviews': [{'state': 'CHANGES_REQUESTED'}],
                                  'checks': [{'name': 'tests', 'conclusion': 'failure'}]}]
        result = self.report([self.task()])
        self.assertEqual(result['next']['action'], 'resolve-ci')
        self.assertEqual(result['next']['role'], 'repository')
        self.assertEqual(result['holds'][0]['action'], 'request-explicit-human-approval')
        self.snapshot['pulls'][0]['checks'] = []
        self.assertEqual(self.report([self.task()])['next']['action'], 'address-review')
        self.snapshot['pulls'][0]['reviews'] = []
        self.assertEqual(self.report()['next']['action'], 'request-explicit-human-approval')
        held = self.report([self.task()])['next']
        self.assertEqual(held['action'], 'request-explicit-human-approval')
        self.assertNotIn('task', held)
        self.assertEqual(held['related_task'], 1)

    def test_superseded_review_request_does_not_reschedule_already_resolved_work(self):
        user = {'id': 10, 'login': 'reviewer'}
        self.snapshot['pulls'] = [{'number': 9, 'reviews': [
            {'id': 1, 'user': user, 'state': 'CHANGES_REQUESTED'},
            {'id': 2, 'user': user, 'state': 'APPROVED'}]}]
        result = self.report([self.task()])
        self.assertFalse(any(b['kind'] == 'review' for b in result['blockers']))
        self.assertEqual(result['next']['action'], 'request-explicit-human-approval')
        self.snapshot['pulls'][0]['reviews'].append(
            {'id': 3, 'user': {'id': 20, 'login': 'other'}, 'state': 'CHANGES_REQUESTED'})
        self.assertEqual(self.report([self.task()])['next']['action'], 'address-review')

    def test_backlog_priority_and_blocked_or_done_issue_states(self):
        self.snapshot['issues'] = [
            {'number': 1, 'title': 'Optional docs', 'labels': ['P3']},
            {'number': 2, 'title': 'Security issue', 'labels': ['P0', 'status:blocked']},
            {'number': 3, 'title': 'Already done', 'labels': ['status:done']},
            {'number': 4, 'title': 'Closed', 'state': 'closed', 'labels': ['P0']}]
        result = self.report()
        self.assertEqual(result['backlog'][0]['id'], 2)
        self.assertEqual(result['next']['action'], 'resolve-issue-blocker')
        self.assertEqual(result['recorded_done_issues'], [3])
        self.assertEqual(len(result['backlog']), 2)

    def test_db_authority_fingerprints_detect_changes_without_a_supplied_stale_flag(self):
        path = self.root / 'AGENTS.md'
        path.write_text('old contract')
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        task = self.task(sources=json.dumps({'AGENTS.md': digest}))
        self.assertEqual(self.report([task])['next']['action'], 'continue-role')
        path.write_text('updated contract')
        self.assertEqual(self.report([task])['next']['action'], 'reconcile-authority')

    def test_shared_clients_report_same_work_without_claiming_a_launch(self):
        tasks = [self.task()]
        codex = self.report(tasks, client='codex')
        cursor = self.report(tasks, client='cursor')
        self.assertEqual(codex['next'], cursor['next'])
        self.assertEqual(codex['progress'], cursor['progress'])
        self.assertEqual(codex['coordination']['shared_store'], cursor['coordination']['shared_store'])
        self.assertFalse(cursor['coordination']['processes_started'])
        self.assertFalse(cursor['coordination']['automatic_merge'])
        self.assertFalse(list(self.root.iterdir()))
        with self.assertRaises(ValueError):
            self.report(client='../unsafe-client')


if __name__ == '__main__':
    unittest.main()
