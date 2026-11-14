"""Native protection and host arming regressions; no network or mutations."""
import json
from pathlib import Path
import sys
import unittest
from unittest.mock import Mock, patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
import github_merge_gate as gate


class GateTests(unittest.TestCase):
    def setUp(self):
        self.policy = json.loads((ROOT / '.github/merge-policy.json').read_text())
        self.pr = {'state': 'open', 'draft': False, 'base': {'ref': 'main'},
                   'head': {'sha': 'abc', 'repo': {'full_name': 'owner/repo'}},
                   'user': {'login': 'developer'}, 'mergeable': True, 'mergeable_state': 'clean'}
        self.protection = {'required_status_checks': {'strict': True, 'checks': [
            {'context': name, 'app_id': 15368} for name in self.policy['required_checks'] + [self.policy['acceptance_check']]]},
            'required_pull_request_reviews': {'required_approving_review_count': 1,
                'dismiss_stale_reviews': True, 'require_last_push_approval': True}}
        self.protection['required_status_checks']['checks'] += self.policy['security_checks']
        for name in ('enforce_admins', 'required_conversation_resolution', 'required_linear_history'):
            self.protection[name] = {'enabled': True}
        self.reviews = [{'user': {'login': 'coderabbitai[bot]'}, 'state': 'APPROVED', 'commit_id': 'abc'}]
        self.checks = [{'id': i, 'app': {'id': 15368}, 'head_sha': 'abc', 'name': name,
                       'status': 'completed', 'conclusion': 'success'} for i, name in enumerate(self.policy['required_checks'])]
        self.checks.append({'id': 50, 'app': {'id': 57789}, 'head_sha': 'abc', 'name': 'CodeQL',
                            'status': 'completed', 'conclusion': 'success'})

    def result(self, files=None, threads=None, **kwargs):
        return gate.evaluate(self.pr, files or ['client/code.cpp'], self.reviews, self.checks,
                             self.protection, {'full_name': 'owner/repo', 'allow_auto_merge': True},
                             threads or [], self.policy, **kwargs)

    def test_otherwise_complete_ordinary_change_passes_native_preconditions(self):
        self.assertTrue(self.result()['eligible'])

    def test_green_ci_without_admin_enforcement_is_held(self):
        self.protection['enforce_admins']['enabled'] = False
        self.assertFalse(self.result()['eligible'])

    def test_missing_or_stale_vote_is_held(self):
        self.reviews[0]['commit_id'] = 'old'
        self.assertFalse(self.result()['eligible'])
        self.reviews = []
        self.assertFalse(self.result()['eligible'])

    def test_sensitive_paths_and_renames_require_scoped_approval(self):
        files = gate.changed_paths([{'filename': 'ordinary.txt', 'previous_filename': '.github/workflows/ci.yml'}])
        self.assertFalse(self.result(files)['eligible'])
        old = [{'user': {'login': 'benjarogit'}, 'body': 'CSRETRO-APPROVE-SENSITIVE old'}]
        self.assertFalse(self.result(files, authorizations=old)['eligible'])
        old[0]['body'] = 'CSRETRO-APPROVE-SENSITIVE abc'
        self.assertTrue(self.result(files, authorizations=old)['eligible'])

    def test_acceptance_verifier_changes_remain_sensitive(self):
        self.assertTrue(self.result(['tools/github_acceptance.py'])['sensitive'])
        self.assertFalse(self.result(['tools/github_acceptance.py'])['eligible'])

    def test_sensitive_labels_are_case_insensitive(self):
        self.pr['labels'] = [{'name': 'PAID-SERVICE'}]
        self.assertTrue(self.result()['sensitive'])
        self.assertFalse(self.result()['eligible'])

    def test_newest_failure_replaces_previous_success(self):
        self.checks.append({**self.checks[0], 'id': 100, 'conclusion': 'failure'})
        self.assertFalse(self.result()['eligible'])

    def test_foreign_app_or_unresolved_threads_block(self):
        self.assertFalse(self.result(threads=[{}])['eligible'])
        self.checks[0]['app']['id'] = 99
        self.assertFalse(self.result()['eligible'])

    def test_requested_changes_and_forks_block(self):
        self.reviews.append({'user': {'login': 'reviewer'}, 'state': 'CHANGES_REQUESTED', 'commit_id': 'abc'})
        self.assertFalse(self.result()['eligible'])
        self.reviews.pop()
        self.pr['head']['repo']['full_name'] = 'stranger/repo'
        self.assertFalse(self.result()['eligible'])

    def test_upload_success_without_scanning_results_blocks(self):
        self.checks = [check for check in self.checks if check['name'] != 'CodeQL']
        self.assertFalse(self.result()['eligible'])

    def test_missing_source_bound_required_check_blocks(self):
        self.protection['required_status_checks']['checks'][0]['app_id'] = None
        self.assertFalse(self.result()['eligible'])

    def test_native_ruleset_requires_effectiveness_visibility_and_no_bypass(self):
        github = Mock()
        github.pages.return_value = [{'ruleset_id': self.policy['native_ruleset_id']}]
        github.api.return_value = {'enforcement': 'active', 'bypass_actors': [], 'rules': [
            {'type': 'code_scanning', 'parameters': {'code_scanning_tools': [
                {'tool': 'CodeQL', 'alerts_threshold': 'all', 'security_alerts_threshold': 'all'}]}}]}
        self.assertTrue(gate.native_protection(github, self.policy)['enforce_admins']['enabled'])
        github.api.return_value['bypass_actors'] = [{}]
        with self.assertRaises(RuntimeError):
            gate.native_protection(github, self.policy)
        del github.api.return_value['bypass_actors']
        with self.assertRaises(RuntimeError):
            gate.native_protection(github, self.policy)
        github.pages.return_value = []
        with self.assertRaises(RuntimeError):
            gate.native_protection(github, self.policy)

    def test_read_only_review_workflow_has_no_secrets_or_write_permissions(self):
        workflow = (ROOT / '.github/workflows/protected-merge.yml').read_text()
        self.assertNotIn(': write', workflow)
        self.assertNotIn('secrets.', workflow)
        self.assertNotIn('create-github-app-token', workflow)
        self.assertIn('pull_request_review:', workflow)
        self.assertIn('persist-credentials: false', workflow)
        self.assertIn('--expected-head', workflow)

    def test_refresh_failure_cannot_arm_or_publish_success(self):
        github = Mock()
        with patch.object(gate, 'inspect', side_effect=RuntimeError('unavailable')), \
             patch.object(gate.subprocess, 'run') as run, patch.object(gate, 'disarm') as disarm:
            with self.assertRaises(RuntimeError):
                gate.publish(github, 1, self.policy)
            run.assert_not_called()
            disarm.assert_called_once()
            github.api.assert_not_called()

    def test_head_change_cannot_arm(self):
        with patch.object(gate, 'inspect', side_effect=[{'head': 'old'}, {'head': 'new'}]), \
             patch.object(gate.subprocess, 'run') as run:
            with self.assertRaises(RuntimeError):
                gate.publish(Mock(), 1, self.policy)
            run.assert_not_called()

    def test_unmet_acceptance_revokes_previous_auto_merge(self):
        held = {'head': 'abc', 'eligible': False, 'holds': ['no actual review']}
        with patch.object(gate, 'inspect', return_value=held), patch.object(gate, 'disarm') as disarm, \
             patch.object(gate.subprocess, 'run') as run:
            gate.publish(Mock(), 1, self.policy)
            disarm.assert_called_once()
            run.assert_not_called()

    def test_sandbox_cannot_target_main_or_change_runtime(self):
        github = Mock()
        with self.assertRaises(ValueError):
            gate.sandbox_policy(github, 18, self.policy, 'main', 42)
        with self.assertRaises(ValueError):
            gate.sandbox_policy(github, 18, self.policy, 'codex/auto-merge-sandbox', self.policy['native_ruleset_id'])
        with patch.object(gate, 'native_protection', return_value=self.protection):
            github.pages.return_value = [{'filename': 'client/code.cpp'}]
            with self.assertRaises(RuntimeError):
                gate.sandbox_policy(github, 18, self.policy, 'codex/auto-merge-sandbox', 42)

    def test_sandbox_requires_equal_native_gates_and_exact_harmless_scope(self):
        github = Mock()
        github.pages.return_value = [{'filename': path} for path in (
            'docs/de/maintainers/merge-canary.md', 'docs/en/maintainers/merge-canary.md', self.policy['acceptance_path'])]
        with patch.object(gate, 'native_protection', side_effect=[self.protection, {}]):
            with self.assertRaises(RuntimeError):
                gate.sandbox_policy(github, 18, self.policy, 'codex/auto-merge-sandbox', 42)
        with patch.object(gate, 'native_protection', return_value=self.protection):
            sandbox = gate.sandbox_policy(github, 18, self.policy, 'codex/auto-merge-sandbox', 42)
            self.assertEqual(sandbox['base'], 'codex/auto-merge-sandbox')
            self.assertEqual(self.policy['base'], 'main')

    def test_complete_gate_only_arms_native_auto_merge_exact_head(self):
        passed = {'head': 'abc', 'eligible': True, 'holds': []}
        github = Mock(repo='owner/repo')
        with patch.object(gate, 'inspect', return_value=passed), \
             patch.object(gate, 'merge_env', return_value={}), patch.object(gate.subprocess, 'run') as run:
            gate.publish(github, 1, self.policy)
            command = run.call_args.args[0]
            self.assertIn('--auto', command)
            self.assertIn('--match-head-commit', command)
            self.assertNotIn('--admin', command)
            github.api.assert_not_called()


if __name__ == '__main__':
    unittest.main()
