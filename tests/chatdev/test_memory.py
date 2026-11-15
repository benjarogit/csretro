"""Memory acceptance and failure cases; all data stays in temporary projects."""
from contextlib import closing
import importlib.util
from pathlib import Path
import sqlite3
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('chatdev_memory', Path(__file__).resolve().parents[2] / 'tools/chatdev_memory.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
Memory = module.Memory


class MemoryTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix='memory-tests-')
        self.root = Path(self.tmp.name)
        (self.root / 'source.py').write_text('first')
        (self.root / 'AGENTS.md').write_text('contract')
        self.memory = Memory(self.root)

    def tearDown(self):
        self.memory.close()
        self.tmp.cleanup()

    def test_restart_and_normalized_deduplication(self):
        ident = self.memory.put('Known   FACT', ['source.py'], 'client', 'finding')
        self.assertEqual(self.memory.put('known fact', [], 'client', 'finding'), ident)
        self.memory.close()
        self.memory = Memory(self.root)
        results = self.memory.search('FACT')
        self.assertEqual(len(results), 1)
        self.assertEqual(results[0]['id'], ident)
        self.assertIn('source.py', results[0]['sources'])
        self.assertFalse(results[0]['stale'])

    def test_sources_requirements_dependencies_and_expiry(self):
        ident = self.memory.put('font rendering', ['source.py'])
        self.assertFalse(self.memory.search('font')[0]['stale'])
        (self.root / 'source.py').write_text('changed')
        self.assertTrue(self.memory.search('font')[0]['stale'])
        self.memory.put('font rendering', ['source.py'])
        (self.root / 'AGENTS.md').write_text('new contract')
        self.assertTrue(self.memory.search('font')[0]['stale'])
        self.memory.put('font rendering', ['source.py'])
        (self.root / 'versions.lock.json').write_text('{}')
        self.assertTrue(self.memory.search('font')[0]['stale'])
        self.memory.put('font rendering', ['source.py'])
        with self.memory.conn:
            self.memory.conn.execute('UPDATE memories SET expires=0 WHERE id=?', (ident,))
        self.assertEqual(self.memory.search('font', include_stale=False), [])

    def test_invalidation_and_safe_search(self):
        ident = self.memory.put('renderer clipping test')
        self.memory.invalidate(ident, 'failed regression')
        self.assertIn('failed regression', self.memory.search('renderer')[0]['reasons'])
        self.assertEqual(self.memory.search('renderer', include_stale=False), [])
        self.assertEqual(len(self.memory.search('')), 1)
        self.assertEqual(len(self.memory.search('"renderer" OR * NOT :')), 1)
        with self.assertRaises(KeyError):
            self.memory.invalidate(999, 'missing')

    def test_ranked_results_and_reuse_after_unrelated_change(self):
        relevant = self.memory.put('font glyph atlas rendering font glyph')
        self.memory.put('unrelated networking packet')
        (self.root / 'unrelated.py').write_text('other change')
        result = self.memory.search('font glyph')
        self.assertEqual(result[0]['id'], relevant)
        self.assertFalse(result[0]['stale'])

    def test_backup_restore_preserves_recovery_and_refuses_overwrite(self):
        self.memory.put('original finding')
        backup = self.root / '.local/chatdev/snapshot.sqlite3'
        self.memory.backup(backup)
        with self.assertRaises(FileExistsError):
            self.memory.backup(backup)
        self.memory.put('later finding')
        recovery = self.memory.restore(backup)
        self.assertEqual(len(self.memory.search('')), 1)
        self.assertTrue(Path(recovery).is_file())
        self.memory.restore(recovery)
        self.assertEqual(len(self.memory.search('')), 2)

    def test_bad_restore_leaves_memory_untouched(self):
        self.memory.put('keep this')
        invalid = self.root / '.local/chatdev/invalid.sqlite3'
        invalid.write_bytes(b'not a database')
        with self.assertRaises(sqlite3.DatabaseError):
            self.memory.restore(invalid)
        backup = self.root / '.local/chatdev/wrong.sqlite3'
        self.memory.backup(backup)
        with closing(sqlite3.connect(backup)) as conn:
            conn.execute("UPDATE metadata SET value='other' WHERE key='project'")
            conn.commit()
        with self.assertRaises(ValueError):
            self.memory.restore(backup)
        self.assertEqual(len(self.memory.search('keep')), 1)

    def test_restore_rejects_invalid_provenance_before_changing_live_state(self):
        self.memory.put('keep this', ['source.py'])
        backup = self.root / '.local/chatdev/malformed.sqlite3'
        self.memory.backup(backup)
        with closing(sqlite3.connect(backup)) as conn:
            conn.execute("UPDATE memories SET sources='[]'")
            conn.commit()
        with self.assertRaises(ValueError):
            self.memory.restore(backup)
        self.assertEqual(len(self.memory.search('keep')), 1)
        self.assertEqual(self.memory.search('keep')[0]['sources']['source.py'],
                         self.memory._digest(self.root / 'source.py'))

    def test_paths_reject_private_external_and_symlink_sources(self):
        for source in ('../outside', '/etc/passwd', '.env', '.local/private'):
            with self.assertRaises(ValueError):
                self.memory.put('unsafe', [source])
        (self.root / 'escape').symlink_to('/etc/passwd')
        with self.assertRaises(ValueError):
            self.memory.put('unsafe', ['escape'])
        with self.assertRaises(ValueError):
            self.memory.backup(self.root / 'tracked.sqlite3')
        with self.assertRaises(ValueError):
            Memory(self.root, self.root / 'external.sqlite3')
        alias = self.root / '.local/chatdev/alias.sqlite3'
        alias.symlink_to(self.memory.path)
        with self.assertRaises(ValueError):
            self.memory.restore(alias)


if __name__ == '__main__':
    unittest.main()
