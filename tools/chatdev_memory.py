"""Local, provenance-aware project memory. Authoritative files always win."""
from __future__ import annotations

from contextlib import closing
import hashlib
import json
import os
from pathlib import Path
import re
import sqlite3
import subprocess
import time
import unicodedata
import uuid


class Memory:
    REQUIRED = ('AGENTS.md', 'docs/plan/csretro-plan.md', 'versions.lock.json')

    def __init__(self, root, db=None):
        self.root = Path(root).resolve()
        self.directory = self.root / '.local' / 'chatdev'
        if (self.root / '.local').is_symlink() or self.directory.is_symlink():
            raise ValueError('Local memory directory cannot be a symlink')
        self.directory.mkdir(parents=True, exist_ok=True, mode=0o700)
        os.chmod(self.directory, 0o700)
        self.path = self._local_path(db or self.directory / 'memory.sqlite3')
        try:
            remote = subprocess.check_output(
                ['git', '-C', str(self.root), 'remote', 'get-url', 'origin'],
                stderr=subprocess.DEVNULL, text=True).strip()
        except subprocess.CalledProcessError:
            remote = str(self.root)
        # The remote identifies a project across checkouts; no remote uses the
        # local checkout identity. Only the digest is persisted.
        self.project = hashlib.sha256(remote.encode()).hexdigest()
        self.conn = sqlite3.connect(self.path, timeout=30)
        self.conn.row_factory = sqlite3.Row
        if self.conn.execute("SELECT 1 FROM sqlite_master WHERE name='metadata'").fetchone():
            prior = self.conn.execute("SELECT value FROM metadata WHERE key='project'").fetchone()
            if prior and prior[0] != self.project:
                self.conn.close()
                raise ValueError('Memory belongs to another project')
        self.conn.execute('PRAGMA foreign_keys=ON')
        self.conn.executescript('''
            CREATE TABLE IF NOT EXISTS metadata(key TEXT PRIMARY KEY,value TEXT NOT NULL);
            CREATE TABLE IF NOT EXISTS memories(
              id INTEGER PRIMARY KEY, fingerprint TEXT UNIQUE NOT NULL,
              text TEXT NOT NULL, component TEXT NOT NULL, kind TEXT NOT NULL,
              revision TEXT NOT NULL, created REAL NOT NULL, updated REAL NOT NULL,
              expires REAL NOT NULL, sources TEXT NOT NULL, invalid_reason TEXT);
            CREATE VIRTUAL TABLE IF NOT EXISTS memory_search USING fts5(text,component,kind);
        ''')
        prior = self.conn.execute("SELECT value FROM metadata WHERE key='project'").fetchone()
        if prior and prior[0] != self.project:
            self.conn.close()
            raise ValueError('Memory belongs to another project')
        with self.conn:
            self.conn.execute("INSERT OR IGNORE INTO metadata VALUES('project',?)", (self.project,))
            self.conn.execute("INSERT OR IGNORE INTO metadata VALUES('schema','1')")
        os.chmod(self.path, 0o600)

    def _local_path(self, value):
        path = Path(value)
        if not path.is_absolute():
            path = self.root / path
        # Resolve before writing, including existing symlink parents.
        resolved = path.resolve()
        if not resolved.is_relative_to(self.directory.resolve()) or path.is_symlink():
            raise ValueError('Memory files must stay under .local/chatdev')
        if resolved == self.directory.resolve():
            raise ValueError('A file path is required')
        resolved.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
        return resolved

    def _source(self, value):
        path = Path(value)
        if path.is_absolute() or '..' in path.parts or not path.parts:
            raise ValueError('Sources must be repository-relative')
        if any(p in {'.local', '.git', '.env'} for p in path.parts) or path.name.startswith('.env'):
            raise ValueError('Private files cannot be indexed')
        resolved = (self.root / path).resolve()
        if not resolved.is_relative_to(self.root) or any(
                p in {'.local', '.git', '.env'} for p in resolved.relative_to(self.root).parts):
            raise ValueError('Source escapes the repository')
        return path.as_posix(), resolved

    @staticmethod
    def _digest(path):
        if not path.is_file():
            return None
        with path.open('rb') as stream:
            return hashlib.file_digest(stream, 'sha256').hexdigest()

    def _revision(self):
        try:
            return subprocess.check_output(['git', '-C', str(self.root), 'rev-parse', 'HEAD'],
                                           stderr=subprocess.DEVNULL, text=True).strip()
        except subprocess.CalledProcessError:
            return 'uncommitted'

    def put(self, text, sources=(), component='repo', kind='analysis', ttl_days=30):
        text = text.strip()
        if not text or ttl_days <= 0:
            raise ValueError('Non-empty text and positive review interval required')
        provenance = {}
        for value in (*self.REQUIRED, *sources):
            name, path = self._source(value)
            provenance[name] = self._digest(path)
        normalized = ' '.join(unicodedata.normalize('NFKC', text).casefold().split())
        fingerprint = hashlib.sha256(json.dumps([normalized, component, kind],
                                                ensure_ascii=False).encode()).hexdigest()
        now = time.time()
        with self.conn:
            old = self.conn.execute('SELECT id,sources FROM memories WHERE fingerprint=?',
                                    (fingerprint,)).fetchone()
            if old:
                # Reconfirming a result refreshes all old dependencies too;
                # adding another citation must not erase prior provenance.
                for name in json.loads(old['sources']):
                    _, path = self._source(name)
                    provenance.setdefault(name, self._digest(path))
                ident = old['id']
                self.conn.execute('''UPDATE memories SET text=?,revision=?,updated=?,expires=?,
                                     sources=?,invalid_reason=NULL WHERE id=?''',
                                  (text, self._revision(), now, now + ttl_days * 86400,
                                   json.dumps(provenance, sort_keys=True), ident))
                self.conn.execute('DELETE FROM memory_search WHERE rowid=?', (ident,))
            else:
                ident = self.conn.execute('''INSERT INTO memories
                    (fingerprint,text,component,kind,revision,created,updated,expires,sources)
                    VALUES(?,?,?,?,?,?,?,?,?)''',
                    (fingerprint, text, component, kind, self._revision(), now, now,
                     now + ttl_days * 86400, json.dumps(provenance, sort_keys=True))).lastrowid
            self.conn.execute('INSERT INTO memory_search(rowid,text,component,kind) VALUES(?,?,?,?)',
                              (ident, text, component, kind))
        return ident

    def _result(self, row):
        result = dict(row)
        result['sources'] = json.loads(result['sources'])
        reasons = []
        if result['invalid_reason']:
            reasons.append(result['invalid_reason'])
        if result['expires'] < time.time():
            reasons.append('review interval expired')
        for name, digest in result['sources'].items():
            _, path = self._source(name)
            if self._digest(path) != digest:
                reasons.append('source changed: ' + name)
        result['stale'] = bool(reasons)
        result['reasons'] = reasons
        return result

    def search(self, query, limit=20, include_stale=True):
        if not 1 <= limit <= 1000:
            raise ValueError('Search limit must be between 1 and 1000')
        tokens = re.findall(r'\w+', query, flags=re.UNICODE)
        if tokens:
            # Quote individual tokens rather than interpreting user FTS syntax.
            match = ' OR '.join('"' + token + '"' for token in tokens[:50])
            rows = self.conn.execute('''SELECT m.* FROM memory_search s JOIN memories m
                ON m.id=s.rowid WHERE memory_search MATCH ? ORDER BY bm25(memory_search),m.id''',
                                     (match,))
        else:
            rows = self.conn.execute('SELECT * FROM memories ORDER BY updated DESC,id DESC')
        results = []
        for row in rows:
            result = self._result(row)
            if include_stale or not result['stale']:
                results.append(result)
                if len(results) == limit:
                    break
        return results

    def invalidate(self, ident, reason):
        if not reason.strip():
            raise ValueError('Invalidation requires a reason')
        with self.conn:
            cursor = self.conn.execute('UPDATE memories SET invalid_reason=?,updated=? WHERE id=?',
                                       (reason, time.time(), ident))
            if not cursor.rowcount:
                raise KeyError(ident)

    def backup(self, path):
        path = self._local_path(path)
        if path == self.path:
            raise ValueError('Backup cannot replace active memory')
        fd = os.open(path, os.O_CREAT | os.O_EXCL | os.O_WRONLY, 0o600)
        os.close(fd)
        try:
            with closing(sqlite3.connect(path)) as dest:
                self.conn.backup(dest)
        except BaseException:
            path.unlink(missing_ok=True)
            raise
        return str(path)

    def restore(self, path):
        path = self._local_path(path)
        if path == self.path or not path.is_file():
            raise ValueError('Restore requires a separate backup file')
        source = sqlite3.connect(path.as_uri() + '?mode=ro', uri=True)
        try:
            if source.execute('PRAGMA integrity_check').fetchone()[0] != 'ok':
                raise ValueError('Backup integrity check failed')
            project = source.execute("SELECT value FROM metadata WHERE key='project'").fetchone()
            schema = source.execute("SELECT value FROM metadata WHERE key='schema'").fetchone()
            if not project or project[0] != self.project or not schema or schema[0] != '1':
                raise ValueError('Backup project or schema mismatch')
            # Ensure the full expected model can be read before touching live state.
            for row in source.execute('SELECT id,text,component,kind,revision,created,updated,expires,sources,invalid_reason FROM memories'):
                provenance = json.loads(row[8])
                if not isinstance(provenance, dict):
                    raise ValueError('Invalid backup provenance')
                for name, digest in provenance.items():
                    self._source(name)
                    if digest is not None and not re.fullmatch(r'[0-9a-f]{64}', digest):
                        raise ValueError('Invalid backup source digest')
            source.execute('SELECT rowid,text FROM memory_search').fetchall()
            recovery = self.directory / ('before-restore-' + uuid.uuid4().hex + '.sqlite3')
            self.backup(recovery)
            # SQLite's backup destination is committed atomically on success.
            source.backup(self.conn)
            return str(recovery)
        finally:
            source.close()

    def close(self):
        self.conn.close()
