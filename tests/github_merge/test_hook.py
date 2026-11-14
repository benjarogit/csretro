"""Pre-push must isolate nested Git fixtures while retaining the LFS context."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class HookEnvironmentTests(unittest.TestCase):
    def test_worktree_hook_variables_cannot_redirect_fixture_git_init(self):
        Path('/tmp/opencode').mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir='/tmp/opencode', prefix='hook-fixture-') as directory:
            fixture = Path(directory)
            repo = fixture / 'repo'
            subprocess.run(['git', 'init', '-q', str(repo)], check=True)
            subprocess.run(['git', '-C', str(repo), 'config', 'core.bare', 'false'], check=True)
            scripts = repo / 'scripts'
            scripts.mkdir()
            verify = scripts / 'verify.sh'
            verify.write_text('''#!/usr/bin/env bash
set -eu
[[ -z "${GIT_DIR+x}" && -z "${GIT_INDEX_FILE+x}" ]]
git init --bare -q "$PWD/fixture-bare"
[[ "$(git config --get core.bare)" == false ]]
printf 'verified clean nested Git environment\\n'
''')
            verify.chmod(0o755)
            commands = fixture / 'bin'
            commands.mkdir()
            lfs = commands / 'git-lfs'
            lfs.write_text('''#!/usr/bin/env bash
set -eu
[[ -n "${GIT_DIR:-}" && -n "${GIT_INDEX_FILE:-}" ]]
printf 'original LFS context retained\\n'
''')
            lfs.chmod(0o755)
            environment = {**os.environ, 'GIT_DIR': str(repo / '.git'),
                           'GIT_WORK_TREE': str(repo), 'GIT_INDEX_FILE': str(repo / '.git/index'),
                           'PATH': str(commands) + os.pathsep + os.environ['PATH']}
            result = subprocess.run(['bash', str(ROOT / '.githooks/pre-push'), 'origin', 'unused'],
                                    cwd=repo, env=environment, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn('verified clean nested Git environment', result.stdout)
            self.assertIn('original LFS context retained', result.stdout)
            bare = subprocess.check_output(['git', '-C', str(repo), 'config', '--get', 'core.bare'], text=True)
            self.assertEqual(bare.strip(), 'false')


if __name__ == '__main__':
    unittest.main()
