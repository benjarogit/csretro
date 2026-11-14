"""Real local orchestration acceptance; GitHub mutation is not exercised here."""
import json
from pathlib import Path
import sqlite3
import subprocess
import sys
import tempfile
import unittest

TOOLS = Path(__file__).resolve().parents[2] / 'tools'
sys.path.insert(0, str(TOOLS))
from chatdev import AUTHORITY, Coordinator, maintenance, roles_for


class CoordinatorTests(unittest.TestCase):
    def setUp(self):
        Path('/tmp/opencode').mkdir(parents=True, exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(prefix='chatdev-coordinator-', dir='/tmp/opencode')
        self.root = Path(self.temp.name)
        subprocess.run(['git', 'init', '-q', str(self.root)], check=True)
        for name in AUTHORITY:
            self.write(name, '{}\n' if name.endswith('.json') else f'Authority {name}\n')
        self.write('evidence.txt', 'Observed acceptance evidence\n')
        self.instances = []
        self.coordinator = self.open()

    def tearDown(self):
        for item in self.instances:
            item.db.close()
            item.memory.conn.close()
        self.temp.cleanup()

    def write(self, name, text):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)
        return path

    def open(self):
        instance = Coordinator(self.root)
        self.instances.append(instance)
        return instance

    def start(self, objective='Correct paragraph', mode='docs', paths=()):
        return self.coordinator.start(objective, mode=mode, paths=paths, offline=True)

    def record(self, task, role, actor='contributor', status='passed'):
        return self.coordinator.record(task, role, actor, status,
                                       f'{role}: actual fixture result', ['evidence.txt'])

    def finish_phases(self, task):
        for role in self.coordinator.resume(task)['roles']:
            self.record(task, role, 'independent-reviewer' if role == 'quality' else 'contributor')

    def test_simple_docs_task_activates_only_three_controlled_roles(self):
        task = self.start()
        self.assertEqual(task['roles'], ['orchestrator', 'documentation', 'quality'])
        self.assertEqual(task['next'], 'orchestrator')
        self.assertIn('no model process started', task['execution'])
        self.assertIn('explicit offline', task['github']['unavailable'])
        self.assertEqual(set(task['context']), set(AUTHORITY))
        self.assertEqual(len(roles_for('Audit texture and translation', 'analysis')), 5)

    def test_role_keywords_do_not_activate_repository_for_precision(self):
        self.assertNotIn('repository', roles_for('Fix precision of coordinates', 'change'))

    def test_nested_harness_is_loaded_and_changes_invalidate_task(self):
        self.write('client/AGENTS.md', 'Client rules\n')
        nested = self.write('client/render/AGENTS.md', 'Renderer rules\n')
        self.write('client/render/code.cpp', '// sample\n')
        task = self.start(paths=['client/render/code.cpp'])
        self.assertEqual(task['context']['client/AGENTS.md'], 'Client rules\n')
        self.assertEqual(task['context']['client/render/AGENTS.md'], 'Renderer rules\n')
        nested.write_text('Changed renderer rules\n')
        self.assertTrue(self.coordinator.resume(task['id'])['stale'])
        with self.assertRaises(ValueError):
            self.record(task['id'], 'orchestrator')

    def test_new_nested_harness_cannot_be_missed_on_resume(self):
        self.write('client/render/code.cpp', '// sample\n')
        task = self.start(paths=['client/render/code.cpp'])
        self.write('client/AGENTS.md', 'New rules\n')
        self.assertTrue(self.coordinator.resume(task['id'])['stale'])

    def test_root_authority_change_blocks_work_and_completion(self):
        task = self.start()
        self.finish_phases(task['id'])
        self.write('AGENTS.md', 'Changed authority\n')
        resumed = self.coordinator.resume(task['id'])
        self.assertTrue(resumed['stale'])
        self.assertIn('reconcile', resumed['next'])
        with self.assertRaises(ValueError):
            self.coordinator.complete(task['id'])

    def test_restart_recovers_work_results_and_memory_without_duplicates(self):
        task = self.start()
        self.record(task['id'], 'orchestrator')
        restarted = self.open()
        resumed = restarted.resume(task['id'])
        self.assertEqual(resumed['objective'], 'Correct paragraph')
        self.assertEqual(resumed['next'], 'documentation')
        self.assertEqual(len(resumed['results']), 1)
        self.assertTrue(restarted.memory.search('actual fixture result'))
        self.assertEqual(restarted.db.execute('SELECT count(*) FROM tasks').fetchone()[0], 1)
        self.assertEqual(restarted.db.execute('SELECT count(*) FROM results').fetchone()[0], 1)

    def test_cli_entry_and_second_process_resume_persist_actual_state(self):
        entry = TOOLS.parent / 'scripts/chatdev.sh'
        started = json.loads(subprocess.check_output(
            [str(entry), '--root', str(self.root), 'run', 'Correct paragraph', '--mode', 'docs', '--offline'],
            cwd='/tmp/opencode', text=True))
        resumed = json.loads(subprocess.check_output(
            [str(entry), '--root', str(self.root), 'resume', str(started['id'])],
            cwd='/tmp/opencode', text=True))
        self.assertEqual(started['id'], resumed['id'])
        self.assertEqual(resumed['next'], 'orchestrator')
        self.assertFalse(resumed['stale'])

    def test_collision_rolls_back_every_file_in_failed_claim(self):
        first = self.start('Implement addition', mode='change')['id']
        second = self.start('Implement subtraction', mode='change')['id']
        self.coordinator.claim(first, 'developer', ['src/shared.cpp'])
        with self.assertRaises((ValueError, sqlite3.IntegrityError)):
            self.coordinator.claim(second, 'developer', ['src/new.cpp', 'src/shared.cpp'])
        claims = [tuple(row) for row in self.coordinator.db.execute('SELECT * FROM claims')]
        self.assertEqual(claims, [('src/shared.cpp', first, 'developer')])

    def test_readonly_roles_and_edit_scope_boundaries_are_enforced(self):
        task = self.start('Implement addition', mode='change')['id']
        for role, path in [('orchestrator', 'code.cpp'), ('quality', 'code.cpp'),
                           ('tester', 'code.cpp'), ('documentation', 'code.cpp'),
                           ('developer', '.git/config'), ('developer', '.local/private'),
                           ('developer', '../escape.cpp')]:
            with self.subTest(role=role, path=path), self.assertRaises(ValueError):
                self.coordinator.claim(task, role, [path])
        self.coordinator.claim(task, 'tester', ['tests/test.cpp'])
        self.coordinator.claim(task, 'documentation', ['docs/en/guide.md', 'HANDOFF.md'])

    def test_independent_quality_actor_is_required(self):
        task = self.start()['id']
        self.record(task, 'orchestrator')
        self.record(task, 'documentation')
        with self.assertRaises(ValueError):
            self.record(task, 'quality', actor='contributor')
        self.record(task, 'quality', actor='independent-reviewer')
        self.assertEqual(self.coordinator.complete(task)['state'], 'done')

    def test_quality_cannot_pass_until_contributing_phases_pass(self):
        task = self.start()['id']
        with self.assertRaises(ValueError):
            self.record(task, 'quality', actor='independent-reviewer')
        with self.assertRaises(ValueError):
            self.coordinator.complete(task)

    def test_contributor_cannot_reuse_quality_actor_after_review(self):
        task = self.start()['id']
        self.finish_phases(task)
        try:
            self.record(task, 'documentation', actor='independent-reviewer')
        except ValueError:
            return
        with self.assertRaises(ValueError):
            self.coordinator.complete(task)

    def test_changed_evidence_invalidates_completion(self):
        task = self.start()['id']
        self.finish_phases(task)
        self.write('evidence.txt', 'Different evidence\n')
        with self.assertRaises(ValueError):
            self.coordinator.complete(task)
        self.assertNotEqual(self.coordinator.resume(task)['state'], 'done')

    def test_completion_does_not_follow_replaced_evidence_symlink(self):
        task = self.start()['id']
        self.finish_phases(task)
        with tempfile.TemporaryDirectory(dir='/tmp/opencode') as outside:
            target = Path(outside) / 'evidence.txt'
            target.write_bytes((self.root / 'evidence.txt').read_bytes())
            (self.root / 'evidence.txt').unlink()
            (self.root / 'evidence.txt').symlink_to(target)
            with self.assertRaises(ValueError):
                self.coordinator.complete(task)

    def test_other_success_cannot_hide_blocked_phase_or_release_its_claim(self):
        task = self.start('Implement addition', mode='change')['id']
        self.coordinator.claim(task, 'developer', ['src/blocked.cpp'])
        self.record(task, 'developer', status='blocked')
        self.record(task, 'orchestrator')
        self.assertEqual(self.coordinator.resume(task)['state'], 'blocked')
        self.assertEqual(self.coordinator.db.execute('SELECT count(*) FROM claims').fetchone()[0], 1)
        with self.assertRaises(ValueError):
            self.coordinator.complete(task)

    def test_completed_task_releases_claims_and_cannot_claim_more(self):
        task = self.start()['id']
        self.coordinator.claim(task, 'documentation', ['docs/en/guide.md'])
        self.finish_phases(task)
        self.assertEqual(self.coordinator.complete(task)['state'], 'done')
        self.assertEqual(self.coordinator.db.execute('SELECT count(*) FROM claims').fetchone()[0], 0)
        with self.assertRaises(ValueError):
            self.coordinator.claim(task, 'documentation', ['docs/en/another.md'])

    def test_stale_task_cannot_claim_files(self):
        task = self.start()['id']
        self.write('HANDOFF.md', 'New project state\n')
        with self.assertRaises(ValueError):
            self.coordinator.claim(task, 'documentation', ['docs/en/guide.md'])

    def test_rejected_private_evidence_cannot_leave_a_passed_result(self):
        task = self.start()['id']
        self.write('.env', 'PRIVATE_TEST_VALUE=fixture-only\n')
        with self.assertRaises(ValueError):
            self.coordinator.record(task, 'orchestrator', 'contributor', 'passed',
                                    'Fixture-only result', ['.env'])
        self.assertEqual(self.coordinator.resume(task)['results'], [])

    def test_empty_summary_rejection_cannot_leave_a_passed_result(self):
        task = self.start()['id']
        with self.assertRaises(ValueError):
            self.coordinator.record(task, 'orchestrator', 'contributor', 'passed',
                                    '   ', ['evidence.txt'])
        self.assertEqual(self.coordinator.resume(task)['results'], [])

    def test_completed_task_cannot_be_reopened_by_record(self):
        task = self.start()['id']
        self.finish_phases(task)
        self.coordinator.complete(task)
        with self.assertRaises(ValueError):
            self.record(task, 'documentation')
        self.assertEqual(self.coordinator.resume(task)['state'], 'done')

    def test_backup_and_restore_recover_tasks_claims_and_memory_together(self):
        task = self.start('Implement addition', mode='change')['id']
        self.coordinator.claim(task, 'developer', ['src/claimed.cpp'])
        self.record(task, 'orchestrator')
        backup = self.root / '.local/chatdev/acceptance-backup.sqlite3'
        self.coordinator.memory.backup(backup)
        self.record(task, 'developer', status='blocked')
        self.coordinator.memory.restore(backup)
        restarted = self.open()
        resumed = restarted.resume(task)
        self.assertEqual(len(resumed['results']), 1)
        self.assertEqual(resumed['results'][0]['role'], 'orchestrator')
        self.assertEqual(restarted.db.execute('SELECT count(*) FROM claims').fetchone()[0], 1)
        self.assertTrue(restarted.memory.search('actual fixture result'))

    def test_maintenance_reports_without_deleting_tracked_or_local_files(self):
        self.write('leftover.log', 'review this log\n')
        self.write('assets/one.svg', '<svg/>\n')
        self.write('assets/two.svg', '<svg/>\n')
        self.write('.local/private.bin', 'protected original placeholder\n')
        subprocess.run(['git', '-C', str(self.root), 'add', '-f', 'leftover.log', 'assets'], check=True)
        before = {p.relative_to(self.root).as_posix(): p.read_bytes() for p in self.root.rglob('*') if p.is_file()}
        report = maintenance(self.root)
        self.assertTrue(report['review_only'])
        self.assertIn('leftover.log', report['temporary_candidates'])
        self.assertEqual(report['duplicate_assets'], [['assets/one.svg', 'assets/two.svg']])
        after = {p.relative_to(self.root).as_posix(): p.read_bytes() for p in self.root.rglob('*') if p.is_file()}
        self.assertEqual(before, after)

    def test_stale_claims_can_be_released_for_a_reconciled_task(self):
        task = self.start()['id']
        self.coordinator.claim(task, 'documentation', ['docs/en/guide.md'])
        self.record(task, 'orchestrator')
        self.write('HANDOFF.md', 'Reconciled project state\n')
        self.assertTrue(self.coordinator.resume(task)['stale'])
        released = self.coordinator.release(task, 'Reconcile changed authority in a new task')
        self.assertTrue(released['history_preserved'])
        prior = self.coordinator.resume(task)
        self.assertEqual(prior['state'], 'cancelled')
        self.assertEqual(len(prior['results']), 1)
        replacement = self.start('Correct paragraph after reconciliation')['id']
        self.coordinator.claim(replacement, 'documentation', ['docs/en/guide.md'])
        owner = self.coordinator.db.execute(
            'SELECT task FROM claims WHERE path=?', ('docs/en/guide.md',)).fetchone()[0]
        self.assertEqual(owner, replacement)

    def test_cancelled_task_cannot_record_claim_or_complete(self):
        task = self.start()['id']
        self.finish_phases(task)
        self.coordinator.release(task, 'Superseded by another task')
        with self.assertRaises(ValueError):
            self.record(task, 'documentation')
        with self.assertRaises(ValueError):
            self.coordinator.claim(task, 'documentation', ['docs/en/guide.md'])
        with self.assertRaises(ValueError):
            self.coordinator.complete(task)
        self.assertEqual(self.coordinator.resume(task)['state'], 'cancelled')

    def test_release_requires_reason_and_preserves_claim_on_rejection(self):
        task = self.start()['id']
        self.coordinator.claim(task, 'documentation', ['docs/en/guide.md'])
        with self.assertRaises(ValueError):
            self.coordinator.release(task, '   ')
        self.assertNotEqual(self.coordinator.resume(task)['state'], 'cancelled')
        self.assertEqual(self.coordinator.db.execute('SELECT count(*) FROM claims').fetchone()[0], 1)

    def test_scoped_untracked_implementation_change_after_review_denies_complete(self):
        implementation = self.write('src/implementation.py', 'result = 1\n')
        task = self.start('Implement addition', mode='change', paths=['src'])['id']
        self.finish_phases(task)
        implementation.write_text('result = 2\n')
        with self.assertRaisesRegex(ValueError, 'Reviewed work changed'):
            self.coordinator.complete(task)
        self.assertNotEqual(self.coordinator.resume(task)['state'], 'done')

    def test_claimed_untracked_implementation_change_after_review_denies_complete(self):
        implementation = self.write('src/implementation.py', 'result = 1\n')
        task = self.start('Implement addition', mode='change')['id']
        self.coordinator.claim(task, 'developer', ['src/implementation.py'])
        self.finish_phases(task)
        implementation.write_text('result = 2\n')
        with self.assertRaisesRegex(ValueError, 'Reviewed work changed'):
            self.coordinator.complete(task)
        self.assertEqual(self.coordinator.db.execute('SELECT count(*) FROM claims').fetchone()[0], 1)

    def test_new_file_in_reviewed_scope_requires_fresh_review(self):
        self.write('src/implementation.py', 'result = 1\n')
        task = self.start('Implement addition', mode='change', paths=['src'])['id']
        self.finish_phases(task)
        self.write('src/new_unreviewed.py', 'result = 3\n')
        with self.assertRaisesRegex(ValueError, 'Reviewed work changed'):
            self.coordinator.complete(task)

    def commit_fixture_files(self, message, paths):
        subprocess.run(['git', '-C', str(self.root), 'add', '--', *paths], check=True)
        subprocess.run(['git', '-C', str(self.root), '-c', 'user.name=Fixture Author',
                        '-c', 'user.email=fixture@example.invalid', '-c', 'commit.gpgSign=false',
                        'commit', '-q', '-m', message], check=True)

    def test_committing_the_exact_reviewed_work_keeps_acceptance_valid(self):
        self.write('src/tracked.py', 'result = 0\n')
        self.commit_fixture_files('Initial fixture', [*AUTHORITY, 'evidence.txt', 'src/tracked.py'])
        task = self.start('Implement addition', mode='change')['id']
        self.write('src/tracked.py', 'result = 1\n')
        self.finish_phases(task)
        # No explicit scope/claim: changed tracked work must remain covered
        # across the commit boundary through the task's persisted base commit.
        self.commit_fixture_files('Commit reviewed fixture', ['src/tracked.py'])
        self.assertEqual(self.coordinator.complete(task)['state'], 'done')

    def test_later_changed_commit_cannot_reuse_prior_review(self):
        self.write('src/änderung\n file.py', 'result = 0\n')
        self.commit_fixture_files('Initial fixture', [*AUTHORITY, 'evidence.txt', 'src/änderung\n file.py'])
        task = self.start('Implement addition', mode='change')['id']
        self.write('src/änderung\n file.py', 'result = 1\n')
        self.finish_phases(task)
        self.commit_fixture_files('Commit reviewed fixture', ['src/änderung\n file.py'])
        self.write('src/änderung\n file.py', 'result = 2\n')
        self.commit_fixture_files('Commit unreviewed fixture', ['src/änderung\n file.py'])
        with self.assertRaisesRegex(ValueError, 'Reviewed work changed'):
            self.coordinator.complete(task)
        self.assertNotEqual(self.coordinator.resume(task)['state'], 'done')

    def test_reviewed_executable_permission_change_requires_fresh_review(self):
        script = self.write('src/run.sh', '#!/bin/sh\nexit 0\n')
        script.chmod(0o755)
        task = self.start('Implement script', mode='change', paths=['src'])['id']
        self.finish_phases(task)
        script.chmod(0o644)
        with self.assertRaisesRegex(ValueError, 'Reviewed work changed'):
            self.coordinator.complete(task)

    def test_upstream_observations_persist_deduplicate_and_invalidate_changed_revision(self):
        candidate = {'reference':'https://github.com/example/source/pull/1','commit':'a','source':'example/source','relevance':'test observation'}
        report = {'candidates':[candidate], 'errors':[], 'mutations':False}
        first = self.coordinator.remember_upstreams(report)['memory_ids'][0]
        self.assertEqual(self.coordinator.remember_upstreams(report)['memory_ids'],[first])
        reopened = self.open()
        self.assertEqual(len(reopened.memory.search('observation',include_stale=False)),1)
        candidate['commit']='b'
        second = reopened.remember_upstreams(report)['memory_ids'][0]
        self.assertNotEqual(first,second)
        hits = reopened.memory.search('observation')
        self.assertTrue(next(row for row in hits if row['id']==first)['stale'])
        self.assertFalse(next(row for row in hits if row['id']==second)['stale'])

    def test_repeated_setup_is_idempotent(self):
        task = self.start()['id']
        for _ in range(3):
            instance = self.open()
            self.assertEqual(instance.resume(task)['id'], task)
            self.assertEqual(instance.db.execute('SELECT count(*) FROM tasks').fetchone()[0], 1)
        self.assertEqual(len(list((self.root / '.local/chatdev').glob('memory.sqlite3'))), 1)


if __name__ == '__main__':
    unittest.main()
