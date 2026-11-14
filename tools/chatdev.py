#!/usr/bin/env python3
"""Local, resumable work coordination. Does not launch model agents or a game."""
import argparse
import hashlib
import json
import os
import re
from pathlib import Path
import sqlite3
import subprocess
import time

from chatdev_memory import Memory
from chatdev_github import GitHub, reconcile

ROLES = {
 'orchestrator': ('Coordination, dependencies, memory and acceptance', 'coordinate'),
 'developer': ('Implementation and regression fixes', 'assigned-files'),
 'analyst': ('Causes, interfaces and compatibility', 'read-only'),
 'tester': ('Reproducible tests and evidence', 'tests'),
 'quality': ('Independent requirements and evidence review; may block', 'read-only'),
 'architect': ('Consistency, module boundaries and reviewed cleanup', 'read-only'),
 'security': ('Security and permissions', 'read-only'),
 'performance': ('Measured CPU, memory, rendering and network', 'read-only'),
 'ux': ('Interaction, accessibility and visual consistency', 'read-only'),
 'assets': ('Licensed graphics and resource formats', 'assigned-files'),
 'documentation': ('DE/EN docs, changelog and handoff', 'docs'),
 'localization': ('Existing language resources, keys and fallbacks', 'assigned-files'),
 'repository': ('GitHub, CI and existing upstream mechanisms', 'repository'),
 'server': ('Installation, operations, backup and diagnostics', 'assigned-files'),
}
RULES = {
 'security': ('security', 'secret', 'permission', 'sicherheit'),
 'performance': ('performance', 'fps', 'memory leak', 'benchmark'),
 'ux': ('layout', 'ux', 'screen', 'menü', 'menu'),
 'assets': ('texture', 'icon', 'asset', 'grafik'),
 'localization': ('translation', 'localiz', 'übersetz', 'sprache'),
 'server': ('server', 'backup', 'restore'),
 'repository': ('ci', 'github', 'upstream', 'pull request'),
 'architect': ('architecture', 'refactor', 'cleanup', 'architektur'),
}
AUTHORITY = ('AGENTS.md', 'docs/plan/csretro-plan.md', 'HANDOFF.md', 'versions.lock.json')


def git(root, *args):
    return subprocess.check_output(['git', '-C', str(root), *args], text=True, stderr=subprocess.DEVNULL).strip()


def roles_for(task, mode):
    roles = ['orchestrator']
    if mode == 'analysis':
        roles += ['analyst']
    elif mode == 'docs':
        roles += ['documentation']
    else:
        roles += ['developer', 'tester', 'documentation']
    low = task.lower()
    roles += [r for r, words in RULES.items() if any(re.search(r'(?<!\w)'+re.escape(w)+r'(?!\w)' if w=='ci' else re.escape(w),low) for w in words)]
    return list(dict.fromkeys(roles + ['quality']))


class Coordinator:
    def __init__(self, root):
        self.root = Path(root).resolve()
        for path in AUTHORITY:
            if not (self.root / path).is_file():
                raise ValueError(f'Missing authority: {path}')
        local = self.root / '.local/chatdev'
        if local.resolve() != local or (self.root / '.local').is_symlink():
            raise ValueError('Local state must not escape repository')
        local.mkdir(parents=True, exist_ok=True)
        os.chmod(local, 0o700)
        self.memory = Memory(self.root)
        self.db = self.memory.conn
        self.db.row_factory = sqlite3.Row
        self.db.execute('CREATE TABLE IF NOT EXISTS tasks (id INTEGER PRIMARY KEY, objective TEXT, mode TEXT, state TEXT, roles TEXT, sources TEXT, snapshot TEXT, created REAL)')
        self.db.execute('CREATE TABLE IF NOT EXISTS results (task INTEGER, role TEXT, actor TEXT, status TEXT, summary TEXT, evidence TEXT, PRIMARY KEY(task,role))')
        self.db.execute('CREATE TABLE IF NOT EXISTS claims (path TEXT PRIMARY KEY, task INTEGER, role TEXT)')
        self.db.execute('CREATE TABLE IF NOT EXISTS task_bases (task INTEGER PRIMARY KEY, revision TEXT)')
        self.db.execute('CREATE TABLE IF NOT EXISTS scopes (task INTEGER, path TEXT)')
        self.db.execute('CREATE TABLE IF NOT EXISTS quality_snapshot (task INTEGER PRIMARY KEY, digest TEXT)')
        self.db.commit()

    def fingerprints(self, paths=AUTHORITY):
        return {p: hashlib.sha256((self.root / p).read_bytes()).hexdigest() if (self.root/p).is_file() else None for p in paths}

    def context(self, paths=()):
        selected = list(AUTHORITY)
        for path in paths:
            target = (self.root / path).resolve()
            if not target.is_relative_to(self.root):
                raise ValueError('Scope path escapes repository')
            parent = target if target.is_dir() else target.parent
            while parent != self.root:
                candidate = parent / 'AGENTS.md'
                if candidate.is_file(): selected.append(candidate.relative_to(self.root).as_posix())
                parent = parent.parent
        return {p: (self.root / p).read_text() for p in dict.fromkeys(selected)}

    def start(self, objective, mode='change', paths=(), offline=False):
        context = self.context(paths)
        watched = set(context)
        for path in paths:
            parent = (self.root/path).resolve()
            if not parent.is_dir(): parent = parent.parent
            while parent != self.root:
                watched.add((parent/'AGENTS.md').relative_to(self.root).as_posix())
                parent = parent.parent
        snapshot = {'unavailable': 'explicit offline mode'} if offline else GitHub(self.root).snapshot()
        with self.db:
            row = self.db.execute("INSERT INTO tasks(objective,mode,state,roles,sources,snapshot,created) VALUES(?,?,?,?,?,?,?)", (objective, mode, 'in-progress', json.dumps(roles_for(objective,mode)), json.dumps(self.fingerprints(watched)),json.dumps(snapshot),time.time()))
        try: base = git(self.root,'rev-parse','HEAD')
        except subprocess.CalledProcessError: base = ''
        with self.db:
            self.db.execute('INSERT INTO task_bases VALUES(?,?)',(row.lastrowid,base))
            self.db.executemany('INSERT INTO scopes VALUES(?,?)',[(row.lastrowid,p) for p in paths])
        task = self.resume(row.lastrowid)
        task['context'] = context
        task['memory'] = self.memory.search(objective, include_stale=False)
        task['github'] = snapshot
        task['execution'] = 'Controlled phases awaiting an actual coding agent; no model process started'
        return task

    def resume(self, task):
        row = self.db.execute('SELECT * FROM tasks WHERE id=?',(task,)).fetchone()
        if not row: raise ValueError('Unknown task')
        result = dict(row)
        for key in ('roles','sources','snapshot'): result[key] = json.loads(result[key])
        result['stale'] = result['sources'] != self.fingerprints(result['sources'])
        result['results'] = [dict(r) for r in self.db.execute('SELECT * FROM results WHERE task=?',(task,))]
        pending = [r for r in result['roles'] if not any(x['role']==r and x['status']=='passed' for x in result['results'])]
        result['next'] = 'reconcile changed authority' if result['stale'] else (pending[0] if pending else 'complete')
        return result

    def claim(self, task, role, paths):
        data = self.resume(task)
        if data['stale'] or data['state'] in ('done','cancelled'): raise ValueError('Task is stale or complete')
        if role not in data['roles'] or role not in ('developer','tester','documentation','assets','localization','server'):
            raise ValueError('Role has no edit authority')
        with self.db:
            for path in paths:
                p = (self.root/path).resolve()
                if not p.is_relative_to(self.root) or '.git' in p.relative_to(self.root).parts or '.local' in p.relative_to(self.root).parts:
                    raise ValueError('Protected path')
                name = p.relative_to(self.root).as_posix()
                if role == 'documentation' and not (name.startswith('docs/') or name in ('HANDOFF.md','CHANGELOG.md')): raise ValueError('Documentation scope only')
                if role == 'tester' and not (name.startswith('tests/') or '/tests/' in name): raise ValueError('Test scope only')
                self.db.execute('INSERT INTO claims VALUES(?,?,?)',(name,task,role))
        return {'claimed':paths,'role':role,'enforcement':'coordination lock; OS sandbox belongs to the host agent'}

    def work_digest(self, task):
        paths = {r[0] for r in self.db.execute('SELECT path FROM scopes WHERE task=?',(task,))}
        paths.update(r[0] for r in self.db.execute('SELECT path FROM claims WHERE task=?',(task,)))
        base = self.db.execute('SELECT revision FROM task_bases WHERE task=?',(task,)).fetchone()
        if base and base[0]:
            if not re.fullmatch(r'[0-9a-f]{40,64}',base[0]): raise ValueError('Invalid task base revision')
            names = subprocess.check_output(['git','-C',str(self.root),'diff',base[0],'--name-only','-z'])
            paths.update(name.decode('utf-8','surrogateescape') for name in names.split(b'\0') if name)
        hashes = {}
        for name in paths:
            target = (self.root/name).resolve()
            if not target.is_relative_to(self.root): raise ValueError('Scope escapes repository')
            files = sorted(target.rglob('*')) if target.is_dir() else [target]
            for p in files:
                if any(x in ('.git','.local','__pycache__') or x.startswith('.env') for x in p.relative_to(self.root).parts): continue
                resolved = p.resolve()
                if not resolved.is_relative_to(self.root): raise ValueError('Reviewed path escapes repository')
                if p.is_file(): hashes[p.relative_to(self.root).as_posix()] = {'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'executable':bool(p.stat().st_mode & 0o111),'link':os.readlink(p) if p.is_symlink() else None}
                elif not p.exists(): hashes[name] = None
        return hashlib.sha256(json.dumps(hashes,sort_keys=True).encode()).hexdigest()

    def record(self, task, role, actor, status, summary, evidence):
        data = self.resume(task)
        if data['state'] in ('done','cancelled'): raise ValueError('Task is already complete')
        if data['stale']: raise ValueError('Authorities changed; start a reconciled task')
        if role not in data['roles'] or status not in ('passed','blocked','failed'): raise ValueError('Invalid role/result')
        if not summary.strip(): raise ValueError('Summary required')
        if not actor.strip() or not evidence: raise ValueError('Actor and evidence required')
        checked = []
        for path in evidence:
            p = (self.root/path).resolve()
            if not p.is_relative_to(self.root) or not p.is_file() or '.git' in p.relative_to(self.root).parts or any(part.startswith('.env') for part in p.relative_to(self.root).parts): raise ValueError('Evidence must be a local project file')
            checked.append({'path':p.relative_to(self.root).as_posix(),'sha256':hashlib.sha256(p.read_bytes()).hexdigest()})
        if role == 'quality':
            others = [r for r in data['results'] if r['role'] != 'quality']
            if any(r['actor']==actor for r in others): raise ValueError('Independent reviewer required')
            if status=='passed' and any(not any(r['role']==needed and r['status']=='passed' for r in others) for needed in data['roles'] if needed!='quality'):
                raise ValueError('Required phases incomplete')
        reviewed = self.work_digest(task) if role=='quality' else None
        with self.db:
            if reviewed: self.db.execute('INSERT OR REPLACE INTO quality_snapshot VALUES(?,?)',(task,reviewed))
            else: self.db.execute('DELETE FROM quality_snapshot WHERE task=?',(task,))
            self.db.execute('INSERT OR REPLACE INTO results VALUES(?,?,?,?,?,?)',(task,role,actor,status,summary,json.dumps(checked)))
            if role != 'quality': self.db.execute("DELETE FROM results WHERE task=? AND role='quality'",(task,))
            blocked = self.db.execute("SELECT 1 FROM results WHERE task=? AND status!='passed'",(task,)).fetchone()
            self.db.execute('UPDATE tasks SET state=? WHERE id=?',('blocked' if blocked else 'in-review',task))
        self.memory.put(summary, sources=[x['path'] for x in checked if not x['path'].startswith('.local/')], component='orchestration',kind='review' if role=='quality' else 'experience')
        return self.resume(task)

    def release(self, task, reason):
        self.resume(task)
        if not reason.strip(): raise ValueError('Reason required')
        with self.db:
            self.db.execute("UPDATE tasks SET state='cancelled' WHERE id=?",(task,))
            self.db.execute('DELETE FROM claims WHERE task=?',(task,))
        return {'released':task,'reason':reason,'history_preserved':True}

    def complete(self, task):
        data = self.resume(task)
        if data['state']=='cancelled': raise ValueError('Task cancelled')
        if data['stale'] or data['next']!='complete': raise ValueError('Incomplete or stale acceptance')
        snapshot = self.db.execute('SELECT digest FROM quality_snapshot WHERE task=?',(task,)).fetchone()
        if not snapshot or snapshot[0] != self.work_digest(task): raise ValueError('Reviewed work changed')
        reviewer = next(r['actor'] for r in data['results'] if r['role']=='quality')
        if any(r['actor']==reviewer for r in data['results'] if r['role']!='quality'): raise ValueError('Independent reviewer required')
        for row in data['results']:
            for e in json.loads(row['evidence']):
                p = (self.root/e['path']).resolve()
                if not p.is_relative_to(self.root) or hashlib.sha256(p.read_bytes()).hexdigest()!=e['sha256']: raise ValueError('Evidence changed since review')
        with self.db:
            self.db.execute("UPDATE tasks SET state='done' WHERE id=?",(task,))
            self.db.execute('DELETE FROM claims WHERE task=?',(task,))
        return self.resume(task)


def maintenance(root):
    # Report candidates; dynamic resources and untracked user data are never deleted.
    tracked = git(root,'ls-files').splitlines()
    duplicates = {}; candidates=[]
    for name in tracked:
        p = Path(root)/name
        if not p.is_file() or p.is_symlink(): continue
        if name.endswith(('.log','.tmp','.plan.md')): candidates.append(name)
        if p.suffix.lower() in ('.png','.tga','.svg'):
            duplicates.setdefault(hashlib.sha256(p.read_bytes()).hexdigest(),[]).append(name)
    return {'review_only':True,'temporary_candidates':candidates,'duplicate_assets':[v for v in duplicates.values() if len(v)>1], 'manual_review':['dead code','unused dependencies','dynamic plugin/resource references','naming and documentation links: scripts/verify.sh']}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
    sub=parser.add_subparsers(dest='command',required=True)
    p=sub.add_parser('run');p.add_argument('task');p.add_argument('--mode',choices=('change','analysis','docs'),default='change');p.add_argument('--scope',action='append',default=[]);p.add_argument('--offline',action='store_true')
    for cmd in ('resume','complete'):
        p=sub.add_parser(cmd);p.add_argument('id',type=int)
    p=sub.add_parser('release');p.add_argument('id',type=int);p.add_argument('--reason',required=True)
    p=sub.add_parser('record');p.add_argument('id',type=int);p.add_argument('role',choices=ROLES);p.add_argument('--actor',required=True);p.add_argument('--status',choices=('passed','failed','blocked'),required=True);p.add_argument('--summary',required=True);p.add_argument('--evidence',action='append',required=True)
    p=sub.add_parser('claim');p.add_argument('id',type=int);p.add_argument('role',choices=ROLES);p.add_argument('paths',nargs='+')
    for cmd in ('backlog','maintenance','roles','tasks'): sub.add_parser(cmd)
    p=sub.add_parser('memory');p.add_argument('query')
    p=sub.add_parser('remember');p.add_argument('text');p.add_argument('--source',action='append',default=[]);p.add_argument('--component',default='repo');p.add_argument('--kind',default='analysis')
    p=sub.add_parser('invalidate');p.add_argument('id',type=int);p.add_argument('reason')
    p=sub.add_parser('issue');p.add_argument('title');p.add_argument('body');p.add_argument('--apply',action='store_true')
    p=sub.add_parser('check');p.add_argument('check',choices=('verify','docs'))
    for cmd in ('backup','restore'):
        p=sub.add_parser(cmd);p.add_argument('path',type=Path)
    args=parser.parse_args(); coordinator=Coordinator(args.root)
    if args.command=='run': result=coordinator.start(args.task,args.mode,args.scope,args.offline)
    elif args.command in ('resume','complete'): result=getattr(coordinator,args.command)(args.id)
    elif args.command=='release': result=coordinator.release(args.id,args.reason)
    elif args.command=='record': result=coordinator.record(args.id,args.role,args.actor,args.status,args.summary,args.evidence)
    elif args.command=='claim': result=coordinator.claim(args.id,args.role,args.paths)
    elif args.command=='backlog': result=reconcile(GitHub(args.root).snapshot())
    elif args.command=='maintenance': result=maintenance(args.root)
    elif args.command=='roles': result=ROLES
    elif args.command=='tasks': result=[dict(r) for r in coordinator.db.execute('SELECT id,objective,state FROM tasks ORDER BY id')]
    elif args.command=='remember': result={'id':coordinator.memory.put(args.text,args.source,args.component,args.kind)}
    elif args.command=='invalidate': coordinator.memory.invalidate(args.id,args.reason); result={'invalidated':args.id}
    elif args.command=='issue': result=GitHub(args.root).ensure_issue(args.title,args.body,apply=args.apply)
    elif args.command=='check':
        check=subprocess.run([str(coordinator.root/'scripts'/('verify.sh' if args.check=='verify' else 'build-docs.sh'))],cwd=coordinator.root,text=True,capture_output=True,timeout=600)
        result={'exit_code':check.returncode,'stdout':check.stdout,'stderr':check.stderr}
    elif args.command=='memory': result=coordinator.memory.search(args.query)
    else: result=getattr(coordinator.memory,args.command)(args.path) or {'ok':True}
    print(json.dumps(result,ensure_ascii=False,indent=2))

if __name__=='__main__':
    try: main()
    except (ValueError,sqlite3.Error,OSError,subprocess.SubprocessError) as error:
        raise SystemExit(str(error)) from error
