"""Provider-neutral assignment export and optional executable discovery.

No function invokes an IDE, agent, installer, model API, daemon or paid service.
Discovery means PATH presence only; authentication/permissions are not probed.
"""
from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import tempfile

from chatdev_contracts import validate_client, SCHEMA_VERSION


def capabilities():
    """Discover optional executables without running even their help commands."""
    optional = {}
    for name, executable in (('codex', 'codex'), ('cursor_agent', 'cursor-agent'),
                             ('cursor_launcher', 'cursor')):
        optional[name] = {'executable': executable, 'discovered': shutil.which(executable) is not None,
                          'verification': 'PATH presence only', 'execution_enabled': False,
                          'installation_attempted': False}
    return {'schema_version': SCHEMA_VERSION,
            'builtin': {'transport': 'local-json', 'available': True,
                        'provider_required': False, 'model_api_required': False},
            'optional': optional, 'processes_started': False,
            'note': 'Cursor launcher discovery does not prove a separately installed Agent CLI'}


def _decoded(value, fallback):
    return json.loads(value) if isinstance(value, str) else value if value is not None else fallback


def _relative(value, root):
    if not isinstance(value, str) or not value or '\\' in value:
        raise ValueError('Source/scope paths must be repository-relative')
    path = Path(value)
    if path.is_absolute() or '..' in path.parts or any(
            part in {'.git', '.local'} or part.startswith('.env') for part in path.parts):
        raise ValueError('Protected or escaping source/scope path')
    if not (root / path).resolve().is_relative_to(root):
        raise ValueError('Source/scope symlink escapes repository')
    return path.as_posix()


def export(task, client, root):
    """Export an immutable, versioned assignment from supplied shared task state.

    Returns a relative file reference. The file consumer must resume against
    current authority before editing; this export grants no executable powers.
    """
    client = validate_client(client)
    root = Path(root).resolve()
    if not root.is_dir() or not isinstance(task, dict):
        raise ValueError('Existing project root and task dictionary required')
    ident = task.get('id')
    if type(ident) is not int or ident <= 0:
        raise ValueError('Persisted positive task ID required')
    if not isinstance(task.get('objective'), str) or not task['objective'].strip():
        raise ValueError('Task objective required')
    roles = _decoded(task.get('roles'), [])
    sources = _decoded(task.get('sources'), {})
    scopes = _decoded(task.get('scope', task.get('scopes')), [])
    if not isinstance(roles, list) or not all(isinstance(role, str) and re.fullmatch(r'[a-z][a-z0-9_-]*', role) for role in roles):
        raise ValueError('Selected role identifiers required')
    if not isinstance(sources, dict) or not isinstance(scopes, list):
        raise ValueError('Source fingerprint map and scope list required')
    normalized = {}
    for name, digest in sources.items():
        if digest is not None and (not isinstance(digest, str) or not re.fullmatch(r'[0-9a-f]{64}', digest)):
            raise ValueError('Source fingerprint must be SHA-256 or absent')
        normalized[_relative(name, root)] = digest
    payload = {'schema_version': SCHEMA_VERSION, 'transport': 'local-json', 'client': client,
               'task': {'id': ident, 'objective': task['objective'], 'mode': task.get('mode'),
                        'state': task.get('state'), 'roles': roles, 'sources': normalized,
                        'scope': [_relative(name, root) for name in scopes]},
               'shared_state': {'database': '.local/chatdev/memory.sqlite3',
                                'status': '.local/chatdev/status.json'},
               'requirements': {'resume_current_authority': True, 'claims_before_edits': True,
                                'independent_quality_required': True,
                                'fresh_explicit_merge_approval': True},
               'execution': {'started': False, 'authorization_granted': False,
                             'automatic_agent_start': False}}
    content = (json.dumps(payload, ensure_ascii=False, sort_keys=True, indent=2) + '\n').encode()
    signature = hashlib.sha256(content).hexdigest()
    directory = root
    # Reject symlinks at every managed level before creating ignored state.
    for part in ('.local', 'chatdev', 'assignments'):
        directory = directory / part
        if directory.is_symlink() or directory.resolve() != directory:
            raise ValueError('Managed assignment directory cannot be a symlink')
        directory.mkdir(exist_ok=True, mode=0o700)
        os.chmod(directory, 0o700)
    destination = directory / f'{ident}-{client}-{signature}.json'
    if destination.is_symlink():
        raise ValueError('Assignment destination cannot be a symlink')
    # Publish without overwriting another assignment or exposing a partial file.
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(dir=directory, prefix='.assignment-', delete=False) as stream:
            temporary = Path(stream.name)
            os.fchmod(stream.fileno(), 0o600)
            stream.write(content)
            stream.flush()
            os.fsync(stream.fileno())
        try:
            os.link(temporary, destination, follow_symlinks=False)
        except FileExistsError:
            descriptor = os.open(destination, os.O_RDONLY | os.O_NOFOLLOW)
            with os.fdopen(descriptor, 'rb') as stream:
                if os.fstat(stream.fileno()).st_mode & 0o777 != 0o600:
                    raise ValueError('Existing assignment file must remain private')
                if stream.read() != content:
                    raise ValueError('Existing assignment content differs; never overwrite it')
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)
    return {'schema_version': SCHEMA_VERSION, 'assignment': destination.relative_to(root).as_posix(),
            'client': client, 'task': ident, 'sha256': signature, 'execution_started': False}
