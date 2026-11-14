"""Provider-neutral adapter tests; all optional executable discovery is mocked."""
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
from chatdev_adapters import capabilities, export, validate_client


class AdapterTests(unittest.TestCase):
    def setUp(self):
        Path('/tmp/opencode').mkdir(parents=True, exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(dir='/tmp/opencode')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / 'AGENTS.md').write_text('Current project authority')
        self.task = {'id': 5, 'objective': 'Review actual shared work', 'mode': 'analysis',
                     'state': 'in-progress', 'roles': ['analyst', 'quality'],
                     'sources': {'AGENTS.md': hashlib.sha256((self.root / 'AGENTS.md').read_bytes()).hexdigest()},
                     'scope': ['tools/example.py'], 'snapshot': {'private': 'not exported'},
                     'results': [{'summary': 'full transcript is not exported'}]}

    def test_file_protocol_always_works_without_optional_agents(self):
        with patch('chatdev_adapters.shutil.which', return_value=None), patch('subprocess.run', side_effect=AssertionError('Must not execute')):
            found = capabilities()
            receipt = export(self.task, 'generic-editor', self.root)
        self.assertTrue(found['builtin']['available'])
        self.assertTrue(all(not x['discovered'] for x in found['optional'].values()))
        self.assertFalse(found['processes_started'])
        payload = json.loads((self.root / receipt['assignment']).read_text())
        self.assertEqual(payload['schema_version'], 1)
        self.assertEqual(payload['task']['id'], 5)
        self.assertEqual(payload['client'], 'generic-editor')
        self.assertEqual(payload['shared_state']['status'], '.local/chatdev/status.json')
        self.assertNotIn('snapshot', payload['task'])
        self.assertNotIn('results', payload['task'])
        self.assertTrue(payload['requirements']['protected_auto_merge_required'])
        self.assertTrue(payload['requirements']['explicit_approval_for_sensitive_changes'])
        self.assertEqual(payload['requirements']['merge_policy_source'], '.github/merge-policy.json')
        self.assertNotIn('fresh_explicit_merge_approval', payload['requirements'])
        self.assertFalse(payload['execution']['authorization_granted'])

    def test_discovery_does_not_execute_installed_launchers_or_prove_authentication(self):
        with patch('chatdev_adapters.shutil.which', return_value='/mock/bin/executable') as which, patch('subprocess.run', side_effect=AssertionError('Must not execute')):
            found = capabilities()
        self.assertEqual([x.args[0] for x in which.call_args_list], ['codex', 'cursor-agent', 'cursor'])
        self.assertTrue(all(x['discovered'] for x in found['optional'].values()))
        self.assertTrue(all(not x['execution_enabled'] and not x['installation_attempted'] for x in found['optional'].values()))
        self.assertIn('does not prove', found['note'])

    def test_exports_are_idempotent_immutable_and_private(self):
        one = export(self.task, 'cli', self.root)
        two = export(self.task, 'cli', self.root)
        self.assertEqual(one, two)
        file = self.root / one['assignment']
        self.assertEqual(file.stat().st_mode & 0o777, 0o600)
        self.assertEqual(file.parent.stat().st_mode & 0o777, 0o700)
        self.assertEqual(len(list(file.parent.glob('*.json'))), 1)
        self.assertEqual(hashlib.sha256(file.read_bytes()).hexdigest(), one['sha256'])
        self.task['state'] = 'in-review'
        changed = export(self.task, 'cli', self.root)
        self.assertNotEqual(changed['assignment'], one['assignment'])
        self.assertEqual(json.loads(file.read_text())['task']['state'], 'in-progress')

    def test_export_does_not_reuse_a_file_with_broadened_permissions(self):
        receipt = export(self.task, 'cli', self.root)
        os.chmod(self.root / receipt['assignment'], 0o644)
        with self.assertRaises(ValueError):
            export(self.task, 'cli', self.root)

    def test_concurrent_exports_publish_complete_matching_payloads(self):
        with ThreadPoolExecutor(max_workers=4) as workers:
            receipts = list(workers.map(lambda _: export(self.task, 'cli', self.root), range(8)))
        self.assertTrue(all(r == receipts[0] for r in receipts))
        self.assertEqual(len(list((self.root / '.local/chatdev/assignments').iterdir())), 1)

    def test_client_paths_and_commands_are_rejected(self):
        for client in ('../cursor', '/tmp/client', 'cursor agent', 'x;touch', '', 'UPPER', 'a' * 49):
            with self.subTest(client=client), self.assertRaises(ValueError):
                export(self.task, client, self.root)
        self.assertFalse((self.root / '.local').exists())
        self.assertEqual(validate_client('external_ide-2'), 'external_ide-2')

    def test_protected_sources_and_scopes_are_rejected(self):
        for path in ('../outside', '/tmp/outside', '.git/config', '.local/private', '.env', 'sub/.env.prod', '..\\outside'):
            task = {**self.task, 'scope': [path]}
            with self.subTest(path=path), self.assertRaises(ValueError):
                export(task, 'cli', self.root)
        for path in ('.git/config', '.local/private', '../outside'):
            task = {**self.task, 'sources': {path: 'a' * 64}}
            with self.assertRaises(ValueError):
                export(task, 'cli', self.root)

    def test_managed_directory_and_external_scope_symlinks_are_rejected(self):
        with tempfile.TemporaryDirectory(dir='/tmp/opencode') as outside:
            (self.root / '.local').symlink_to(outside, target_is_directory=True)
            with self.assertRaises(ValueError):
                export(self.task, 'cli', self.root)
            self.assertFalse(list(Path(outside).iterdir()))
            (self.root / '.local').unlink()
            (self.root / 'linked').symlink_to(outside, target_is_directory=True)
            with self.assertRaises(ValueError):
                export({**self.task, 'scope': ['linked/file']}, 'cli', self.root)

    def test_db_json_fields_and_invalid_identity(self):
        task = {**self.task, 'roles': json.dumps(self.task['roles']), 'sources': json.dumps(self.task['sources'])}
        self.assertEqual(export(task, 'cli', self.root)['task'], 5)
        for ident in (None, 0, -1, True, '../5'):
            with self.subTest(ident=ident), self.assertRaises(ValueError):
                export({**self.task, 'id': ident}, 'cli', self.root)
        with self.assertRaises(ValueError):
            export({**self.task, 'sources': {'AGENTS.md': 'not-a-sha'}}, 'cli', self.root)


if __name__ == '__main__':
    unittest.main()
