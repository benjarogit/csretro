"""Offline GitHub acceptance scenarios; no network requests or remote mutations."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
from chatdev_github import GitHub, classify, guard_action, reconcile, upstream_candidate


class Remote:
    def __init__(self):
        self.calls = []
        self.issues = []
        self.labels = [{'name': 'bug'}, {'name': 'priority:P1'}, {'name': 'status:new'}]
        self.fail_issues = False
        self.pr = {'state': 'MERGED', 'statusCheckRollup': [{'conclusion': 'SUCCESS'}],
                   'closingIssuesReferences': [{'number': 4}]}

    def __call__(self, argv, **kwargs):
        self.calls.append((argv, kwargs.get('input')))
        result = None
        code = 0
        if argv[1:3] == ['repo', 'view']:
            result = {'nameWithOwner': 'test/project'}
        elif argv[1:3] == ['pr', 'view']:
            result = self.pr
        elif '--method' in argv:
            result = {'number': 4}
        elif 'issues?' in argv[2]:
            result = [self.issues[:1], self.issues[1:]]
            if self.fail_issues:
                code = 1
        elif 'labels?' in argv[2]:
            result = [self.labels]
        elif 'pulls?' in argv[2]:
            result = [[{'number': 3, 'head': {'sha': 'abc'}}]]
        elif '/reviews?' in argv[2]:
            result = [[{'state': 'CHANGES_REQUESTED', 'body': 'Add missing regression'}]]
        elif '/check-runs' in argv[2]:
            result = [{'check_runs': [{'name': 'tests', 'conclusion': 'failure'}]}]
        elif '/actions/runs' in argv[2]:
            result = {'workflow_runs': []}
        elif argv[2] == 'graphql':
            code = 1
        else:
            raise AssertionError(argv)
        return subprocess.CompletedProcess(argv, code, json.dumps(result), 'private error detail')


class GitHubTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.remote = Remote()
        self.github = GitHub(self.root, self.remote)

    def test_snapshot_includes_reviews_ci_and_explicit_project_failure(self):
        self.remote.issues = [{'number': 1, 'title': 'Crash', 'labels': []}]
        snapshot = self.github.snapshot()
        self.assertEqual(snapshot['issues'][0]['number'], 1)
        self.assertEqual(snapshot['pulls'][0]['reviews'][0]['state'], 'CHANGES_REQUESTED')
        self.assertTrue(any('Projects unavailable' in e for e in snapshot['errors']))
        self.assertNotIn('private error detail', json.dumps(snapshot))
        report = reconcile(snapshot, attempts=2)
        self.assertEqual(report['repair'], 'escalate')
        self.assertEqual(report['remaining_attempts'], 0)
        self.assertEqual(report['failed_ci'][0]['pr'], 3)

    def test_live_pending_checks_with_null_conclusion_are_visible(self):
        report = reconcile({'pulls':[{'number':13,'checks':[{'name':'build','conclusion':None,'status':'in_progress'}]}]})
        self.assertEqual(report['failed_ci'], [])
        self.assertEqual(report['pending_ci'], [{'pr':13,'check':'build','status':'in_progress'}])
        self.assertEqual(report['repair'], 'none')

    def test_duplicate_search_includes_later_pages_and_closed_issues(self):
        self.remote.issues = [{'number': 1, 'title': 'Other issue'},
                              {'number': 7, 'title': 'Fix: CRASH!', 'state': 'closed'}]
        result = self.github.ensure_issue('fix crash', 'do not duplicate', apply=True)
        self.assertEqual(result['action'], 'existing')
        self.assertTrue(result['needs_verification'])
        self.assertFalse(any('--method' in a for a, _ in self.remote.calls))
        self.assertTrue(any('state=all' in a[2] and '--paginate' in a for a, _ in self.remote.calls))

    def test_preview_and_explicit_structured_create_reuse_existing_labels(self):
        title = 'Crash `$(touch /tmp/unsafe)`'
        self.assertEqual(self.github.ensure_issue(title, 'body', 'bug', 'P1')['action'], 'preview')
        self.assertFalse(any('--method' in a for a, _ in self.remote.calls))
        result = self.github.ensure_issue(title, 'body\nwith newlines', 'bug', 'P1', apply=True)
        self.assertEqual(result['issue']['number'], 4)
        payload = json.loads(self.remote.calls[-1][1])
        self.assertEqual(payload['title'], title)
        self.assertEqual(payload['body'], 'body\nwith newlines')
        self.assertEqual(payload['labels'], ['bug', 'priority:P1', 'status:new'])

    def test_failed_duplicate_read_never_creates_an_issue(self):
        self.remote.fail_issues = True
        with self.assertRaises(RuntimeError):
            self.github.ensure_issue('Issue', 'body', apply=True)
        self.assertFalse(any('--method' in a for a, _ in self.remote.calls))

    def test_categories_priorities_and_blocked_stale_tasks_remain_visible(self):
        self.assertEqual(classify('CVE-2026-123 security flaw')['category'], 'security')
        self.assertEqual(classify('title', labels=['upstream', 'P1', 'status:blocked']),
                         {'category': 'upstream', 'priority': 'P1', 'status': 'blocked'})
        report = reconcile({'issues': [
            {'number': 1, 'title': 'Same', 'labels': ['status:blocked'], 'updated_at': '2020-01-01T00:00:00Z'},
            {'number': 2, 'title': 'same!'}]})
        self.assertEqual(report['duplicates'], [[1, 2]])
        self.assertEqual(report['blocked'], [1])
        self.assertEqual(report['stale'], [1])

    def test_dependencies_related_work_and_orphaned_in_progress_are_reported(self):
        snapshot = {'issues': [
            {'number': 1, 'title': 'Fix native client callback crash',
             'body': 'Blocked by #3', 'labels': ['status:in-progress']},
            {'number': 2, 'title': 'Fix native client callback crashes', 'labels': []}], 'pulls': []}
        result = reconcile(snapshot)
        self.assertEqual(result['related'], [[1, 2]])
        self.assertEqual(result['dependencies'], [{'issue': 1, 'depends_on': [3]}])
        self.assertEqual(result['orphaned'], [1])

    def _proof(self):
        path = self.root / 'proof.txt'
        path.write_text('actual test and reviewer evidence')
        return {'path': 'proof.txt', 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}

    def test_close_requires_current_hash_independent_quality_and_live_green_merged_pr(self):
        proof = self._proof()
        acceptance = [{'passed': True, 'evidence': proof}]
        review = {'approved': True, 'independent': True, 'evidence': proof}
        self.assertEqual(self.github.close_issue(4, acceptance, review, {'pr': 3})['action'], 'preview-close')
        self.assertFalse(any('--method' in a for a, _ in self.remote.calls))
        self.remote.pr['statusCheckRollup'] = [{'conclusion': 'FAILURE'}]
        with self.assertRaises(ValueError):
            self.github.close_issue(4, acceptance, review, {'pr': 3}, apply=True)
        self.remote.pr['statusCheckRollup'] = [{'conclusion': 'SUCCESS'}]
        self.remote.pr['state'] = 'OPEN'
        with self.assertRaises(ValueError):
            self.github.close_issue(4, acceptance, review, {'pr': 3}, apply=True)
        self.remote.pr['state'] = 'MERGED'
        (self.root / 'proof.txt').write_text('modified after review')
        with self.assertRaises(ValueError):
            self.github.close_issue(4, acceptance, review, {'pr': 3}, apply=True)
        self.assertFalse(any('--method' in a for a, _ in self.remote.calls))

    def test_risky_and_unknown_actions_are_unavailable(self):
        for action in ('force-push', 'history-rewrite', 'game', 'credentials', 'main-merge', 'unknown'):
            self.assertFalse(guard_action(action)['allowed'])
        self.assertTrue(guard_action('snapshot')['allowed'])

    def test_upstream_assessments_deduplicate_and_license_blocks_import(self):
        args = dict(source='upstream/project', reference='https://example.test/commit/123',
                    commit='123', components=['client'], relevance='fix crash',
                    compatibility='compatible', license_review='unknown')
        candidate = upstream_candidate([], **args)
        self.assertFalse(candidate['candidate']['import_allowed'])
        self.assertTrue(upstream_candidate([candidate['candidate']], **args)['existing'])


if __name__ == '__main__':
    unittest.main()
