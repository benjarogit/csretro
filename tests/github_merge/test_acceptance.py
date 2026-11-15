"""Adversarial substantive-review cases. All bot fixtures are synthetic test data."""
import copy
import hashlib
import json
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
import github_acceptance as acceptance


class AcceptanceTests(unittest.TestCase):
    def setUp(self):
        self.policy = json.loads((ROOT / '.github/merge-policy.json').read_text())
        self.pr = {'head': {'sha': 'head'}, 'base': {'sha': 'base'}}
        self.files = [{'filename': 'docs/evidence/acceptance.json'}, {'filename': 'docs/en/guide.md'}]
        self.paths = [item['filename'] for item in self.files]
        self.artifact = {'schema': 1, 'base_sha': 'base', 'human_runtime_required': False,
            'changed_paths': self.paths, 'criteria': [{'id': 'docs',
                'requirement': 'Only the intended documentation changes with independently verified checks.',
                'checks': self.policy['required_checks'] + ['CodeQL']}]}
        self.proof = {'schema': 1, 'head': 'head', 'acceptance_sha256': self.digest(),
            'decision': 'pass', 'reviewed_paths': self.paths,
            'summary': 'Inspected both changed documents and checked the stated criteria against their actual content.',
            'criteria': [{'id': 'docs', 'passed': True, 'reason': 'The exact changed paths contain documentation only and cite required checks.'}]}
        self.review = {'id': 10, 'user': {'login': self.policy['reviewer'], 'id': self.policy['reviewer_user_id']},
            'state': 'APPROVED', 'commit_id': 'head'}
        self.reviews = [self.review]
        self.checks = [{'id': 5, 'name': 'CodeRabbit', 'app': {'id': self.policy['reviewer_app_id']},
            'head_sha': 'head', 'status': 'completed', 'conclusion': 'success', 'output': {'summary': 'Review completed'}}]

    def digest(self):
        return hashlib.sha256(json.dumps(self.artifact, sort_keys=True, separators=(',', ':')).encode()).hexdigest()

    def result(self):
        self.review['body'] = '<!-- csretro-acceptance-review ' + json.dumps(self.proof) + ' -->'
        return acceptance.validate(self.pr, self.files, self.reviews, self.checks, self.artifact, self.policy)

    def published_comment(self, status='Passed', updated='2026-11-15T10:00:00Z'):
        coverage = {'sourceCommitId': 'head', 'coveredCommitId': 'head', 'kind': 'reviewed'}
        body = ('<!-- coderabbit-ai-generated:v1 -->\n'
                + '<!-- This is an auto-generated comment: summarize by coderabbit.ai -->\n'
                + '<!-- final_review_risk_coverage:' + json.dumps(coverage) + ' -->\n'
                + '<!-- pre_merge_checks_walkthrough_start -->\n'
                + '| ' + self.policy['independent_check_name'] + ' | ![' + status
                + '](https://storage.googleapis.com/coderabbit_public_assets/pre-merge-checks-'
                + status.lower() + '.svg) | Source inspected |\n'
                + '<details><summary><strong>Full details: ' + self.policy['independent_check_name'] + '</strong></summary>\n'
                + '```csretro-acceptance-review\n' + json.dumps(self.proof) + '\n```\n</details>\n'
                + '<!-- pre_merge_checks_walkthrough_end -->')
        return {'id': 50, 'user': dict(self.review['user']), 'body': body, 'updated_at': updated}

    def test_published_provider_content_result_does_not_forge_formal_approval(self):
        self.reviews = []
        comment = self.published_comment()
        args = (self.pr, self.files, self.reviews, self.checks, self.artifact, self.policy, [comment])
        self.assertEqual(acceptance.validate(*args, require_approval=False), [])
        self.assertTrue(acceptance.validate(*args))

    def test_formal_approval_plus_provider_attestation_passes_full_gate(self):
        self.review['body'] = 'Approved after the independent content assessment.'
        self.assertEqual(acceptance.validate(self.pr, self.files, self.reviews, self.checks,
                         self.artifact, self.policy, [self.published_comment()]), [])

    def test_bot_chat_reply_echo_is_not_a_provider_assessment(self):
        comment = self.published_comment()
        comment['body'] = '<!-- This is an auto-generated reply by CodeRabbit -->\n' + comment['body']
        self.assertTrue(acceptance.validate(self.pr, self.files, [], self.checks,
            self.artifact, self.policy, [comment], require_approval=False))

    def test_quoted_result_outside_official_section_cannot_shadow_inconclusive(self):
        comment = self.published_comment('Inconclusive')
        quoted = ('| ' + self.policy['independent_check_name'] + ' | ![Passed]'
                  + '(https://storage.googleapis.com/coderabbit_public_assets/pre-merge-checks-passed.svg) | quote |\n')
        comment['body'] = comment['body'].replace('<!-- pre_merge_checks_walkthrough_start -->',
            quoted + '<!-- pre_merge_checks_walkthrough_start -->')
        self.assertTrue(acceptance.validate(self.pr, self.files, [], self.checks,
            self.artifact, self.policy, [comment], require_approval=False))

    def test_duplicate_results_and_unscoped_proof_fail_closed(self):
        for mutation in ('duplicate-row', 'outside-details', 'missing-header'):
            with self.subTest(mutation=mutation):
                comment = self.published_comment()
                if mutation == 'duplicate-row':
                    row = next(line for line in comment['body'].splitlines() if line.startswith('|'))
                    comment['body'] = comment['body'].replace(row, row + '\n' + row)
                elif mutation == 'outside-details':
                    comment['body'] = comment['body'].replace('Full details: ', 'Quoted example: ')
                else:
                    comment['body'] = comment['body'].replace('summarize by coderabbit.ai', 'chat by coderabbit.ai')
                self.assertTrue(acceptance.validate(self.pr, self.files, [], self.checks,
                    self.artifact, self.policy, [comment], require_approval=False))

    def test_inconclusive_or_failed_custom_assessment_cannot_count(self):
        for status in ('Inconclusive', 'Failed', 'Skipped'):
            with self.subTest(status=status):
                self.assertTrue(acceptance.validate(self.pr, self.files, [], self.checks,
                    self.artifact, self.policy, [self.published_comment(status)], require_approval=False))

    def test_comments_require_provider_identity_and_reviewed_head_coverage(self):
        for mutation in ('identity', 'coverage', 'result'):
            with self.subTest(mutation=mutation):
                comment = self.published_comment()
                if mutation == 'identity': comment['user']['id'] = 42
                if mutation == 'coverage': comment['body'] = comment['body'].replace('"kind": "reviewed"', '"kind": "summary"')
                if mutation == 'result': comment['body'] = comment['body'].replace('![Passed]', '![Success]')
                self.assertTrue(acceptance.validate(self.pr, self.files, [], self.checks,
                    self.artifact, self.policy, [comment], require_approval=False))

    def test_later_inconclusive_publication_revokes_earlier_pass(self):
        before = self.published_comment()
        after = self.published_comment('Inconclusive', '2026-11-15T10:01:00Z')
        after['id'] = 51
        after['body'] = after['body'].split('```')[0]
        self.assertTrue(acceptance.validate(self.pr, self.files, [], self.checks,
            self.artifact, self.policy, [before, after], require_approval=False))

    def test_content_only_mode_still_rejects_withdrawn_approval(self):
        self.review['state'] = 'DISMISSED'
        self.assertTrue(acceptance.validate(self.pr, self.files, self.reviews, self.checks,
            self.artifact, self.policy, [self.published_comment()], require_approval=False))

    def test_actual_current_head_review_and_criterion_proof_pass_fixture(self):
        self.assertEqual(self.result(), [])

    def test_green_check_without_substantive_review_never_passes(self):
        self.reviews = []
        self.assertTrue(self.result())

    def test_neutral_skipped_cancelled_failed_and_pending_checks_block(self):
        for conclusion in ('neutral', 'skipped', 'cancelled', 'failure', None):
            with self.subTest(conclusion=conclusion):
                self.checks[0]['conclusion'] = conclusion
                self.assertTrue(self.result())

    def test_success_status_with_skip_or_rate_limit_text_blocks(self):
        for summary in ('Review skipped', 'Review rate limited', 'No model-driven review was run'):
            self.checks[0]['output']['summary'] = summary
            self.assertTrue(self.result())

    def test_summary_only_or_empty_attestation_blocks(self):
        self.result()
        self.review['body'] = 'All looks good'
        self.assertTrue(acceptance.validate(self.pr, self.files, self.reviews, self.checks, self.artifact, self.policy))
        self.proof['summary'] = ''
        self.assertTrue(self.result())

    def test_other_actor_cannot_impersonate_bot_attestation(self):
        self.review['user']['id'] = 42
        self.assertTrue(self.result())

    def test_stale_head_or_artifact_edits_revoke_acceptance(self):
        self.pr['head']['sha'] = 'new'
        self.assertTrue(self.result())
        self.pr['head']['sha'] = 'head'
        self.artifact['criteria'][0]['requirement'] += ' A new unreviewed assertion.'
        self.assertTrue(self.result())

    def test_later_change_request_or_dismissal_revokes_vote(self):
        for state in ('CHANGES_REQUESTED', 'DISMISSED'):
            self.reviews = [self.review, {**self.review, 'id': 11, 'state': state}]
            self.assertTrue(self.result())

    def test_all_current_and_old_rename_paths_must_be_reviewed(self):
        self.files[1]['previous_filename'] = 'docs/en/old.md'
        self.assertTrue(self.result())
        self.artifact['changed_paths'] = self.paths + ['docs/en/old.md']
        self.proof['acceptance_sha256'] = self.digest()
        self.assertTrue(self.result())

    def test_missing_criterion_assessment_or_false_result_blocks(self):
        self.proof['criteria'] = []
        self.assertTrue(self.result())
        self.proof['criteria'] = [{'id': 'docs', 'passed': False, 'reason': 'Actual evidence contradicts the stated acceptance criterion.'}]
        self.assertTrue(self.result())

    def test_human_runtime_cannot_be_synthesized(self):
        self.artifact['human_runtime_required'] = True
        self.proof['acceptance_sha256'] = self.digest()
        self.assertTrue(self.result())

    def test_stale_base_and_invented_checks_block(self):
        self.artifact['base_sha'] = 'old'
        self.assertTrue(self.result())
        self.artifact['base_sha'] = 'base'
        self.artifact['criteria'][0]['checks'].append('nonexistent test')
        self.proof['acceptance_sha256'] = self.digest()
        self.assertTrue(self.result())

    def test_latest_rerun_failure_overrides_historical_success(self):
        self.checks.append({**self.checks[0], 'id': 6, 'conclusion': 'failure'})
        self.assertTrue(self.result())


if __name__ == '__main__':
    unittest.main()
