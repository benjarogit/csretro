"""Actual local PM/CLI protocol integration, not proof of running IDE agents."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from chatdev import AUTHORITY, Coordinator
from chatdev_manager import ProjectManager


class ManagerIntegrationTests(unittest.TestCase):
    def setUp(self):
        Path('/tmp/opencode').mkdir(parents=True, exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(prefix='chatdev-manager-integration-', dir='/tmp/opencode')
        self.root = Path(self.temp.name)
        subprocess.run(['git', 'init', '-q', str(self.root)], check=True)
        for name in AUTHORITY:
            self.write(name, '{}\n' if name.endswith('.json') else f'Authority {name}\n')
        self.write('HANDOFF.md', '# Handoff\n\n- **Gate:** G07 — in progress\n- **Current work:** Orchestration infrastructure\n- **Critical next port work:** Port CS runtime to Xash3D\n')
        self.write('docs/plan/csretro-plan.md', '# Plan\n\n## G07 — original runtime port\n')
        self.write('evidence.txt', 'Observed fixture acceptance\n')
        subprocess.run(['git', '-C', str(self.root), 'add', '--', *AUTHORITY, 'evidence.txt'], check=True)
        subprocess.run(['git', '-C', str(self.root), '-c', 'user.name=Fixture Author',
                        '-c', 'user.email=fixture@example.invalid', '-c', 'commit.gpgSign=false',
                        'commit', '-q', '-m', 'Initial manager fixture'], check=True)
        self.coordinator = Coordinator(self.root)

    def tearDown(self):
        self.coordinator.db.close()
        self.temp.cleanup()

    def write(self, name, text):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)
        return path

    def start(self):
        return self.coordinator.start('Complete ChatDev infrastructure', paths=['src'], offline=True)

    def finish_phases(self, task):
        for role in self.coordinator.resume(task)['roles']:
            self.coordinator.record(task, role, 'independent-reviewer' if role == 'quality' else 'fixture-contributor',
                                    'passed', f'{role} observed fixture evidence', ['evidence.txt'])

    def cli(self, client, *args):
        return subprocess.run([str(ROOT / 'scripts/chatdev.sh'), '--root', str(self.root),
                               '--client', client, *args], cwd='/tmp/opencode', text=True,
                              capture_output=True)

    def test_real_manager_runs_on_start_and_resume_and_reports_reach_sqlite(self):
        actual_report = ProjectManager.report
        with patch.object(ProjectManager, 'report', autospec=True, side_effect=actual_report) as report:
            started = self.start()
            first = started['project_manager']
            self.assertEqual(report.call_count, 1)
            self.assertEqual(first['role'], 'project_manager')
            self.assertEqual(first['milestone']['gate'], 'G07')
            resumed = self.coordinator.resume(started['id'], client='cursor')
            self.assertEqual(report.call_count, 2)
            self.assertEqual(resumed['project_manager']['coordination']['client'], 'cursor')
        rows = self.coordinator.db.execute('SELECT task,client,payload FROM manager_reports ORDER BY id').fetchall()
        self.assertEqual(len(rows), 2)
        self.assertEqual([r['task'] for r in rows], [started['id'], started['id']])
        self.assertEqual([r['client'] for r in rows], ['cli', 'cursor'])
        self.assertEqual(json.loads(rows[-1]['payload'])['next'], resumed['project_manager']['next'])
        self.assertFalse(resumed['project_manager']['coordination']['processes_started'])

    def test_codex_and_cursor_cli_clients_observe_shared_ids_and_next_step(self):
        creation = self.cli('codex', 'run', 'Complete ChatDev infrastructure', '--scope', 'src', '--offline')
        self.assertEqual(creation.returncode, 0, creation.stderr)
        created = json.loads(creation.stdout)
        observation = self.cli('cursor', 'resume', str(created['id']))
        self.assertEqual(observation.returncode, 0, observation.stderr)
        resumed = json.loads(observation.stdout)
        self.assertEqual(resumed['id'], created['id'])
        self.assertEqual(resumed['project_manager']['next'], created['project_manager']['next'])
        status_call = self.cli('codex', 'status')
        self.assertEqual(status_call.returncode, 0, status_call.stderr)
        status = json.loads(status_call.stdout)
        clients = {c['client'] for c in status['clients']}
        self.assertTrue({'codex', 'cursor'}.issubset(clients))
        self.assertEqual(self.coordinator.db.execute('SELECT count(*) FROM tasks').fetchone()[0], 1)
        self.assertFalse(status['coordination']['processes_started'])
        self.assertTrue(status['coordination']['same_project_state'])

    def test_continue_resumes_existing_infrastructure_instead_of_creating_new_task(self):
        started = self.start()
        before = self.coordinator.db.execute('SELECT count(*) FROM tasks').fetchone()[0]
        result = self.cli('cursor', 'continue', '--offline')
        self.assertEqual(result.returncode, 0, result.stderr)
        dispatch = json.loads(result.stdout)
        self.assertEqual(dispatch['dispatch'], 'resume-existing')
        self.assertEqual(dispatch['task']['id'], started['id'])
        self.assertEqual(self.coordinator.db.execute('SELECT count(*) FROM tasks').fetchone()[0], before)
        self.assertFalse(dispatch['processes_started'])

    def test_unverified_protected_merge_does_not_dispatch_resume(self):
        self.start()
        snapshot = {'issues': [], 'pulls': [{'number': 27, 'title': 'Infrastructure', 'state': 'OPEN',
                                            'reviews': [], 'checks': []}], 'errors': []}
        with patch('chatdev.GitHub') as github, patch.object(self.coordinator, 'resume', wraps=self.coordinator.resume) as resume:
            github.return_value.snapshot.return_value = snapshot
            result = self.coordinator.continue_work(client='codex', offline=False)
            github.return_value.snapshot.assert_called_once_with()
            resume.assert_not_called()
        self.assertEqual(result['dispatch'], 'hold-or-handoff')
        self.assertEqual(result['recommendation']['action'], 'verify-protected-auto-merge')
        self.assertEqual(result['recommendation']['pr'], 27)
        self.assertFalse(result['processes_started'])
        self.assertFalse(result['project_manager']['coordination']['automatic_merge'])

    def test_changed_evidence_cannot_be_reported_as_verified(self):
        started = self.start()
        self.finish_phases(started['id'])
        before = self.coordinator.manager_report(task=started['id'])
        progress = next(t for t in before['progress'] if t['task'] == started['id'])
        self.assertEqual(progress['proof_state'], 'verified-local-evidence')
        self.write('evidence.txt', 'Different unreviewed evidence\n')
        after = self.coordinator.resume(started['id'])['project_manager']
        progress = next(t for t in after['progress'] if t['task'] == started['id'])
        self.assertEqual(progress['proof_state'], 'recorded-only')
        self.assertFalse(after['milestone']['completion_verified'])

    def test_changed_scoped_work_cannot_be_reported_as_verified(self):
        implementation = self.write('src/implementation.py', 'result = 1\n')
        started = self.start()
        self.finish_phases(started['id'])
        before = self.coordinator.manager_report(task=started['id'])
        self.assertEqual(next(t for t in before['progress'] if t['task'] == started['id'])['proof_state'],
                         'verified-local-evidence')
        implementation.write_text('result = 2\n')
        after = self.coordinator.resume(started['id'])['project_manager']
        self.assertEqual(next(t for t in after['progress'] if t['task'] == started['id'])['proof_state'],
                         'recorded-only')
        self.assertNotEqual(after['next']['action'], 'verify-and-complete-existing-task')

    def test_atomic_status_json_matches_latest_returned_report(self):
        with patch('chatdev.os.replace', wraps=os.replace) as replace:
            started = self.start()
        filename = self.root / '.local/chatdev/status.json'
        replace.assert_called_once()
        self.assertEqual(Path(replace.call_args.args[1]), filename)
        self.assertNotEqual(Path(replace.call_args.args[0]), filename)
        first = json.loads(filename.read_text())
        self.assertEqual(first, started['project_manager'])
        for client in ('cursor', 'codex', 'cursor'):
            result = self.cli(client, 'status')
            self.assertEqual(result.returncode, 0, result.stderr)
            report = json.loads(result.stdout)
            self.assertEqual(json.loads(filename.read_text()), report)
            self.assertEqual(report['coordination']['client'], client)
        # Each actual write must be an atomic replacement, leaving no partial
        # JSON or extra interchange files after successful sequential calls.
        self.assertEqual(len(list(filename.parent.glob('status.json'))), 1)
        self.assertFalse(list(filename.parent.glob('tmp*')))

    def test_generic_external_host_uses_same_provider_neutral_state(self):
        started = self.start()
        result = self.cli('external-host', 'resume', str(started['id']))
        self.assertEqual(result.returncode, 0, result.stderr)
        resumed = json.loads(result.stdout)
        self.assertEqual(resumed['id'], started['id'])
        self.assertEqual(resumed['project_manager']['coordination']['client'], 'external-host')
        self.assertFalse(resumed['project_manager']['coordination']['processes_started'])
        self.assertEqual(self.coordinator.db.execute('SELECT count(*) FROM tasks').fetchone()[0], 1)

    def test_priorities_order_existing_infrastructure_tasks(self):
        first = self.start()['id']
        second = self.start()['id']
        self.coordinator.assign(first, priority='P3')
        self.coordinator.assign(second, priority='P0', client='external-host')
        report = self.coordinator.manager_report()
        self.assertEqual(report['next']['task'], second)
        self.assertEqual([t['id'] for t in report['backlog'] if t['kind']=='task'], [second, first])
        restarted = Coordinator(self.root)
        try:
            self.assertEqual(restarted.manager_report()['next']['task'], second)
        finally:
            restarted.db.close()

    def test_unresolved_dependency_is_recommended_and_blocks_completion(self):
        dependency = self.start()['id']
        dependent = self.start()['id']
        report = self.coordinator.assign(dependent, priority='P0', dependencies=[dependency])
        self.assertEqual(report['next']['task'], dependent)
        self.assertEqual(report['next']['action'], 'resolve-dependency')
        self.finish_phases(dependent)
        with self.assertRaisesRegex(ValueError, 'Unverified dependencies'):
            self.coordinator.complete(dependent)
        self.finish_phases(dependency)
        self.coordinator.complete(dependency)
        report = self.coordinator.manager_report()
        self.assertFalse([b for b in report['blockers'] if b.get('task')==dependent and b['kind']=='dependency'])
        self.assertEqual(self.coordinator.complete(dependent)['state'], 'done')

    def test_done_dependency_with_changed_evidence_is_not_verified_completion(self):
        dependency = self.start()['id']
        self.finish_phases(dependency)
        self.coordinator.complete(dependency)
        dependent = self.start()['id']
        self.coordinator.assign(dependent, priority='P0', dependencies=[dependency])
        self.write('evidence.txt', 'Unreviewed replacement\n')
        report = self.coordinator.manager_report()
        self.assertTrue([b for b in report['blockers'] if b.get('task')==dependent and b['kind']=='dependency'])
        self.assertEqual(report['next']['action'], 'resolve-dependency')

    def test_invalid_dependency_assignments_roll_back_links_and_priorities(self):
        first, second, third = [self.start()['id'] for _ in range(3)]
        self.coordinator.assign(first, priority='P2', dependencies=[second])
        self.coordinator.assign(second, priority='P3', dependencies=[third])
        def persisted():
            return (list(map(tuple,self.coordinator.db.execute('SELECT * FROM task_priorities ORDER BY task'))),
                    list(map(tuple,self.coordinator.db.execute('SELECT * FROM task_links ORDER BY task,dependency'))))
        before = persisted()
        for dependencies in ([first], [third], [999999]):
            with self.subTest(dependencies=dependencies), self.assertRaises(ValueError):
                self.coordinator.assign(third, priority='P0', dependencies=dependencies)
            self.assertEqual(persisted(), before)

    def test_concurrent_cli_reports_publish_latest_committed_report(self):
        self.start()
        processes = [subprocess.Popen([str(ROOT/'scripts/chatdev.sh'), '--root', str(self.root),
                                      '--client', f'external-host-{i}', 'status'],
                                     cwd='/tmp/opencode', text=True, stdout=subprocess.PIPE,
                                     stderr=subprocess.PIPE) for i in range(6)]
        try:
            for process in processes:
                out, err = process.communicate(timeout=20)
                self.assertEqual(process.returncode, 0, err)
                self.assertFalse(json.loads(out)['coordination']['processes_started'])
        finally:
            for process in processes:
                if process.poll() is None:
                    process.kill()
                    process.wait()
        published = json.loads((self.root/'.local/chatdev/status.json').read_text())
        latest = self.coordinator.db.execute('SELECT id,payload FROM manager_reports ORDER BY id DESC LIMIT 1').fetchone()
        self.assertEqual(published['report_id'], latest['id'])
        self.assertEqual(published, json.loads(latest['payload']))
        self.assertEqual(self.coordinator.db.execute('SELECT count(*) FROM tasks').fetchone()[0], 1)

    def test_fresh_ci_recommendation_adds_repository_role_and_invalidates_quality(self):
        started = self.start()
        self.finish_phases(started['id'])
        self.assertNotIn('repository', self.coordinator.resume(started['id'])['roles'])
        snapshot = {'issues': [], 'pulls': [{'number': 27, 'title': 'Infrastructure', 'state': 'OPEN',
                                            'reviews': [], 'checks': [{'name':'build','conclusion':'FAILURE'}]}], 'errors': []}
        with patch('chatdev.GitHub') as github:
            github.return_value.snapshot.return_value = snapshot
            result = self.coordinator.continue_work(client='external-host')
        self.assertEqual(result['dispatch'], 'resume-existing')
        self.assertEqual(result['recommendation']['action'], 'resolve-ci')
        self.assertIn('repository', result['task']['roles'])
        self.assertFalse([r for r in result['task']['results'] if r['role']=='quality'])
        self.assertEqual(result['task']['snapshot'], snapshot)
        self.assertTrue(result['task']['project_manager']['github']['failed_ci'])

    def test_task_finished_after_recommendation_is_not_dispatched(self):
        started = self.start()
        actual_report = self.coordinator.manager_report
        def report_then_finish(*args, **kwargs):
            report = actual_report(*args, **kwargs)
            # Simulate a competing completed transition at the dispatch boundary;
            # this fixture state is not a claim of verified task completion.
            with self.coordinator.db:
                self.coordinator.db.execute("UPDATE tasks SET state='done' WHERE id=?", (started['id'],))
            return report
        with patch.object(self.coordinator, 'manager_report', side_effect=report_then_finish), \
                patch.object(self.coordinator, 'resume', wraps=self.coordinator.resume) as resume:
            result = self.coordinator.continue_work(offline=True)
            resume.assert_not_called()
        self.assertEqual(result['dispatch'], 'hold-or-handoff')
        self.assertEqual(result['recommendation']['action'], 'refresh-status')
        self.assertFalse(result['processes_started'])

    def test_completed_claim_only_work_retains_verified_proof_for_dependencies(self):
        self.write('docs/en/other.md', 'Reviewed documentation\n')
        original = self.coordinator.start('Complete ChatDev documentation', mode='docs', offline=True)['id']
        self.coordinator.claim(original, 'documentation', ['docs/en/other.md'])
        self.finish_phases(original)
        self.coordinator.complete(original)
        self.assertEqual(self.coordinator.db.execute('SELECT count(*) FROM claims WHERE task=?', (original,)).fetchone()[0], 0)
        report = self.coordinator.manager_report()
        proof = next(row for row in report['progress'] if row['task']==original)
        self.assertEqual(proof['proof_state'], 'verified-local-evidence')
        dependent = self.coordinator.start('Complete ChatDev follow-up', mode='docs', offline=True)['id']
        self.coordinator.assign(dependent, dependencies=[original])
        self.finish_phases(dependent)
        self.assertEqual(self.coordinator.complete(dependent)['state'], 'done')

    def test_changed_completed_claim_only_work_invalidates_proof_and_blocks_dependency(self):
        source = self.write('docs/en/other.md', 'Reviewed documentation\n')
        original = self.coordinator.start('Complete ChatDev documentation', mode='docs', offline=True)['id']
        self.coordinator.claim(original, 'documentation', ['docs/en/other.md'])
        self.finish_phases(original)
        self.coordinator.complete(original)
        dependent = self.coordinator.start('Complete ChatDev follow-up', mode='docs', offline=True)['id']
        self.coordinator.assign(dependent, priority='P0', dependencies=[original])
        self.finish_phases(dependent)
        source.write_text('Unreviewed changed documentation\n')
        report = self.coordinator.manager_report()
        proof = next(row for row in report['progress'] if row['task']==original)
        self.assertEqual(proof['proof_state'], 'recorded-only')
        self.assertTrue([b for b in report['blockers'] if b.get('task')==dependent and b['kind']=='dependency'])
        with self.assertRaisesRegex(ValueError, 'Unverified dependencies'):
            self.coordinator.complete(dependent)

    def test_actual_cli_export_preserves_persisted_scope_after_restart(self):
        self.write('src/render/code.py', 'result = 1\n')
        self.write('tests/check.py', 'assert True\n')
        scopes = ['src/render', 'tests/check.py']
        creation = self.cli('external-host', 'run', 'Complete ChatDev scoped infrastructure',
                            '--scope', scopes[0], '--scope', scopes[1], '--offline')
        self.assertEqual(creation.returncode, 0, creation.stderr)
        started = json.loads(creation.stdout)
        result = self.cli('external-host', 'export', str(started['id']))
        self.assertEqual(result.returncode, 0, result.stderr)
        exported = json.loads(result.stdout)
        payload = json.loads((self.root/exported['assignment']).read_text())
        persisted = [row[0] for row in self.coordinator.db.execute('SELECT path FROM scopes WHERE task=?', (started['id'],))]
        self.assertEqual(set(persisted), set(scopes))
        self.assertEqual(set(payload['task']['scope']), set(persisted))
        self.assertEqual(payload['task']['id'], started['id'])
        self.assertFalse(payload['execution']['automatic_agent_start'])
        self.assertFalse(exported['execution_started'])
        repeated = self.cli('external-host', 'export', str(started['id']))
        self.assertEqual(repeated.returncode, 0, repeated.stderr)
        self.assertEqual(json.loads(repeated.stdout)['assignment'], exported['assignment'])

    def test_unknown_direct_client_cannot_leave_a_partial_task(self):
        with self.assertRaises(ValueError):
            self.coordinator.start('Complete ChatDev infrastructure', offline=True, client='bad/client')
        self.assertEqual(self.coordinator.db.execute('SELECT count(*) FROM tasks').fetchone()[0], 0)

    def test_unknown_client_is_rejected_without_starting_work(self):
        result = self.cli('bad/client', 'status')
        self.assertNotEqual(result.returncode, 0)
        self.assertTrue(result.stderr)
        with self.assertRaises(ValueError):
            self.coordinator.manager_report(client='bad/client')
        self.assertEqual(self.coordinator.db.execute('SELECT count(*) FROM tasks').fetchone()[0], 0)


if __name__ == '__main__':
    unittest.main()
