"""Fail-closed merge eligibility regressions; no GitHub writes."""
import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import unittest
from unittest.mock import Mock, patch

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('gate', ROOT / 'tools/github_merge_gate.py')
gate = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gate)


class GateTests(unittest.TestCase):
    def setUp(self):
        self.policy = json.loads((ROOT / '.github/merge-policy.json').read_text())
        self.policy['acceptance_app_id'] = 99999
        self.pr = {'state': 'open', 'draft': False, 'base': {'ref': 'main'},
                   'head': {'sha': 'abc', 'repo': {'full_name': 'owner/repo'}},
                   'user': {'login': 'developer'}, 'mergeable': True, 'mergeable_state': 'clean',
                   'body': '<!-- chatdev-acceptance ' + json.dumps({'head': 'abc',
                       'criteria': [{'criterion': 'behavior verified', 'evidence': 'CI URL', 'passed': True}],
                       'human_runtime_required': False}) + ' -->'}
        self.protection = {'required_status_checks': {'strict': True, 'checks': [
            {'context': name, 'app_id': 15368} for name in self.policy['required_checks'] + [self.policy['acceptance_check']]]},
            'required_pull_request_reviews': {'required_approving_review_count': 1,
                'dismiss_stale_reviews': True, 'require_last_push_approval': True}}
        for name in ('enforce_admins', 'required_conversation_resolution', 'required_linear_history'):
            self.protection[name] = {'enabled': True}
        declaration = json.loads(self.pr['body'].split('chatdev-acceptance ')[1].split(' -->')[0])
        digest = hashlib.sha256(json.dumps(declaration, sort_keys=True, separators=(',', ':')).encode()).hexdigest()
        self.protection['required_status_checks']['checks'] += self.policy['security_checks']
        self.protection['required_status_checks']['checks'][-2]['app_id'] = 99999
        self.reviews = [{'body': f'CSRETRO-ACCEPTANCE {digest}', 'user': {'login': 'coderabbitai[bot]'}, 'state': 'APPROVED', 'commit_id': 'abc'}]
        self.checks = [{'id': i, 'app': {'id': 15368}, 'head_sha': 'abc', 'name': n,
                       'status': 'completed', 'conclusion': 'success'} for i, n in enumerate(self.policy['required_checks'])]

        self.checks += [{'id': 50, 'app': {'id': 57789}, 'head_sha': 'abc', 'name': 'CodeQL',
                         'status': 'completed', 'conclusion': 'success'}]

    def result(self, files=None, threads=None):
        return gate.evaluate(self.pr, files or ['client/code.cpp'], self.reviews, self.checks,
                             self.protection, {'full_name': 'owner/repo', 'allow_auto_merge': True},
                             threads or [], self.policy)

    def test_complete_ordinary_change_is_eligible(self):
        self.assertTrue(self.result()['eligible'])

    def test_green_ci_without_admin_enforcement_is_held(self):
        self.protection['enforce_admins']['enabled'] = False
        self.assertFalse(self.result()['eligible'])

    def test_skipped_coderabbit_is_not_review(self):
        self.reviews = []
        self.assertFalse(self.result()['eligible'])

    def test_stale_approval_is_not_review(self):
        self.reviews[0]['commit_id'] = 'old'
        self.assertFalse(self.result()['eligible'])

    def test_sensitive_paths_require_explicit_independent_maintainer(self):
        self.assertFalse(self.result(['.github/workflows/ci.yml'])['eligible'])
        self.reviews.append({'user': {'login': 'benjarogit'}, 'state': 'APPROVED', 'commit_id': 'abc'})
        self.assertTrue(self.result(['.github/workflows/ci.yml'])['eligible'])

    def test_stale_acceptance_and_pending_human_runtime_are_held(self):
        self.pr['body'] = self.pr['body'].replace('abc', 'old')
        self.assertFalse(self.result()['eligible'])
        self.pr['body'] = self.pr['body'].replace('old', 'abc').replace('false', 'true')
        self.assertFalse(self.result()['eligible'])

    def test_newest_failure_overrides_older_success(self):
        failure = copy.deepcopy(self.checks[0])
        failure.update(id=100, conclusion='failure')
        self.checks.append(failure)
        self.assertFalse(self.result()['eligible'])

    def test_foreign_check_app_and_unresolved_threads_block(self):
        self.assertFalse(self.result(threads=[{'isResolved': False}])['eligible'])
        self.checks[0]['app']['id'] = 999
        self.assertFalse(self.result()['eligible'])

    def test_requested_changes_and_fork_are_held(self):
        self.reviews.append({'user': {'login': 'reviewer'}, 'state': 'CHANGES_REQUESTED', 'commit_id': 'abc'})
        self.assertFalse(self.result()['eligible'])
        self.reviews.pop()
        self.pr['head']['repo']['full_name'] = 'fork/repo'
        self.assertFalse(self.result()['eligible'])


    def test_acceptance_edit_invalidates_independent_attestation(self):
        self.pr['body'] = self.pr['body'].replace('behavior verified', 'a new unreviewed claim')
        self.assertFalse(self.result()['eligible'])

    def test_open_scanning_finding_blocks_green_upload_job(self):
        result = gate.evaluate(self.pr, ['client/code.cpp'], self.reviews, self.checks,
            self.protection, {'full_name': 'owner/repo', 'allow_auto_merge': True}, [], self.policy,
            security_alerts=[{'state': 'open'}])
        self.assertFalse(result['eligible'])

    def test_sensitive_comment_is_bound_to_head(self):
        args = (self.pr, ['.github/workflows/ci.yml'], self.reviews, self.checks,
                self.protection, {'full_name': 'owner/repo', 'allow_auto_merge': True}, [], self.policy)
        old = [{'user': {'login': 'benjarogit'}, 'body': 'CSRETRO-APPROVE-SENSITIVE old'}]
        self.assertFalse(gate.evaluate(*args, authorizations=old)['eligible'])
        old[0]['body'] = 'CSRETRO-APPROVE-SENSITIVE abc'
        self.assertTrue(gate.evaluate(*args, authorizations=old)['eligible'])

    def test_privileged_workflow_has_no_pr_merge_ref_review_trigger(self):
        workflow = (ROOT / '.github/workflows/protected-merge.yml').read_text()
        self.assertNotIn('pull_request_review:', workflow)
        self.assertIn('ref: main', workflow)
        self.assertIn('persist-credentials: false', workflow)

    def test_public_stranger_cannot_attest_acceptance(self):
        body = self.reviews[0]['body']
        self.reviews[0]['body'] = ''
        self.reviews.append({'user': {'login': 'stranger'}, 'state': 'APPROVED',
                             'commit_id': 'abc', 'body': body})
        self.assertFalse(self.result()['eligible'])

    def test_security_upload_without_completed_result_blocks(self):
        self.checks = [c for c in self.checks if c['name'] != 'CodeQL']
        self.assertFalse(self.result()['eligible'])

    def publisher_fixture(self):
        github = Mock(repo='owner/repo')
        github.check_writer.return_value = github
        github.api.side_effect = [self.pr, {'id': 70, 'app': {'id': 99999}}, {}]
        return github

    def test_publisher_blocks_old_success_before_refresh_api_failure(self):
        github = self.publisher_fixture()
        with patch.object(gate, 'inspect', side_effect=RuntimeError('API unavailable')), \
             patch.object(gate, 'disarm') as disarm, patch.object(gate.subprocess, 'run') as run:
            with self.assertRaises(RuntimeError):
                gate.publish(github, 1, self.policy)
            self.assertEqual(github.api.call_args_list[1].args[2]['status'], 'in_progress')
            self.assertEqual(github.api.call_args_list[2].args[2]['conclusion'], 'failure')
            disarm.assert_called_once()
            run.assert_not_called()

    def test_publisher_head_change_cannot_arm(self):
        github = self.publisher_fixture()
        with patch.object(gate, 'inspect', side_effect=[{'head': 'abc'}, {'head': 'changed'}]), \
             patch.object(gate, 'disarm'), patch.object(gate.subprocess, 'run') as run:
            with self.assertRaises(RuntimeError):
                gate.publish(github, 1, self.policy)
            run.assert_not_called()

    def test_publisher_hold_revokes_previous_arm(self):
        github = self.publisher_fixture()
        held = {'head': 'abc', 'eligible': False, 'holds': ['missing review']}
        with patch.object(gate, 'inspect', return_value=held), \
             patch.object(gate, 'disarm') as disarm, patch.object(gate.subprocess, 'run') as run:
            gate.publish(github, 1, self.policy)
            disarm.assert_called_once()
            run.assert_not_called()

    def test_publisher_uses_native_auto_merge_and_exact_head_only(self):
        github = self.publisher_fixture()
        passed = {'head': 'abc', 'eligible': True, 'holds': []}
        with patch.object(gate, 'inspect', return_value=passed), \
             patch.object(gate, 'merge_env', return_value={}), \
             patch.object(gate.subprocess, 'run') as run:
            gate.publish(github, 1, self.policy)
            command = run.call_args.args[0]
            self.assertIn('--auto', command)
            self.assertIn('--match-head-commit', command)
            self.assertNotIn('--admin', command)

    def test_shared_actions_app_cannot_produce_acceptance(self):
        self.policy['acceptance_app_id'] = 15368
        self.assertFalse(self.result()['eligible'])
        with self.assertRaises(RuntimeError):
            gate.publish(Mock(), 1, self.policy)

    def test_rename_away_from_sensitive_path_keeps_original_classification(self):
        files = gate.changed_paths([{'filename': 'ordinary.txt',
                                    'previous_filename': '.github/workflows/ci.yml'}])
        self.assertFalse(self.result(files)['eligible'])

    def test_native_ruleset_requires_effective_no_bypass_scanning_protection(self):
        github = Mock()
        self.policy['native_ruleset_id'] = 7
        github.pages.return_value = [{'ruleset_id': 7}]
        github.api.return_value = {'enforcement': 'active', 'bypass_actors': [], 'rules': [
            {'type': 'code_scanning', 'parameters': {'code_scanning_tools': [
                {'tool': 'CodeQL', 'alerts_threshold': 'all', 'security_alerts_threshold': 'all'}]}}]}
        state = gate.native_protection(github, self.policy)
        self.assertTrue(state['enforce_admins']['enabled'])
        github.api.return_value['bypass_actors'] = [{'actor_type': 'RepositoryRole'}]
        with self.assertRaises(RuntimeError):
            gate.native_protection(github, self.policy)
        del github.api.return_value['bypass_actors']
        with self.assertRaises(RuntimeError):
            gate.native_protection(github, self.policy)
        github.api.return_value['bypass_actors'] = []
        github.pages.return_value = []
        with self.assertRaises(RuntimeError):
            gate.native_protection(github, self.policy)

    def test_app_token_permissions_are_read_only_except_checks(self):
        workflow = (ROOT / '.github/workflows/protected-merge.yml').read_text()
        self.assertIn('permission-checks: write', workflow)
        self.assertNotIn('permission-pull-requests:', workflow)
        for forbidden in ('permission-administration:', 'permission-contents:', 'permission-security-events:'):
            self.assertNotIn(forbidden, workflow)

    def test_sensitive_labels_match_github_case_insensitively(self):
        self.pr['labels'] = [{'name': 'PAID-SERVICE'}]
        self.assertTrue(self.result()['sensitive'])
        self.assertFalse(self.result()['eligible'])

    def test_protection_unbound_check_is_held(self):
        self.protection['required_status_checks']['checks'][0]['app_id'] = None
        self.assertFalse(self.result()['eligible'])


if __name__ == '__main__':
    unittest.main()
