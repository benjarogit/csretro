"""Upstream monitoring uses offline REST fixtures, never imports or writes."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
from chatdev_github import GitHub


class Api:
    def __init__(self):
        self.calls = []
        self.deny_security = False
        self.many = False

    def __call__(self, argv, **kwargs):
        self.calls.append(argv)
        endpoint = argv[2]
        code = 0
        if endpoint in ('repos/test/one', 'repos/test/reference'):
            value = {'default_branch': 'develop/new'}
        elif '/commits?' in endpoint:
            value = [{'sha': 'reference-new', 'html_url': 'https://example.test/ref/commit/new',
                      'commit': {'message': 'Interesting reference implementation'}}]
        elif '/compare/' in endpoint:
            if self.many:
                value = {'commits': [{'sha': f'new{i}', 'html_url': f'https://example.test/c/{i}',
                                     'commit': {'message': 'Change'}} for i in range(100)]}
            elif 'page=1' in endpoint:
                value = {'commits': [{'sha': 'pin', 'html_url': 'https://example.test/c/pin',
                                     'commit': {'message': 'Already integrated'}},
                                    {'sha': 'new', 'html_url': 'https://example.test/c/new',
                                     'commit': {'message': 'Fix crash'}}]}
            else:
                value = {'commits': []}
        elif '/pulls?' in endpoint:
            value = [{'head': {'sha': 'new'}, 'html_url': 'https://example.test/pr/2',
                      'title': 'Same commit security correction', 'labels': [{'name': 'security'}]},
                     {'head': {'sha': 'pin'}, 'html_url': 'https://example.test/pr/1',
                      'title': 'Already integrated'}]
        elif '/releases?' in endpoint:
            value = [{'tag_name': '1.0', 'html_url': 'https://example.test/release/old',
                      'target_commitish': 'pin'},
                     {'tag_name': '2.0', 'html_url': 'https://example.test/release/new',
                      'target_commitish': 'develop/new', 'name': 'Next version'}]
        elif '/security-advisories?' in endpoint:
            value = [{'ghsa_id': 'GHSA-test', 'html_url': 'https://example.test/security/1',
                      'summary': 'Unsafe handling'}]
            if self.deny_security:
                code = 1
        else:
            raise AssertionError(endpoint)
        return subprocess.CompletedProcess(argv, code, json.dumps(value), 'private error')


class UpstreamTests(unittest.TestCase):
    def setUp(self):
        Path('/tmp/opencode').mkdir(parents=True, exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(dir='/tmp/opencode')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.lock = {
            'project': {'repository': 'https://github.com/own/project'},
            'upstreams': {'first': {'repository': 'https://github.com/test/one.git', 'commit': 'pin', 'prefix': 'client/'}},
            'server_toolchain': {'compiler': {'url': 'https://github.com/test/one/releases/download/1.0/build.tgz',
                                            'source_commit': 'pin', 'version': '1.0', 'source_prefix': 'server/vendor/compiler'}},
            'server_runtime': {'container': {'image': 'external/image@sha256:example'}},
            'references': {'ideas': {'repository': 'https://github.com/test/reference', 'license': 'GPL'}}}
        self.write_lock()
        self.api = Api()
        self.github = GitHub(self.root, self.api)

    def write_lock(self):
        (self.root / 'versions.lock.json').write_text(json.dumps(self.lock))

    def test_derives_and_deduplicates_sources_from_authoritative_register(self):
        original = (self.root / 'versions.lock.json').read_bytes()
        result = self.github.upstreams()
        self.assertEqual(len(result['sources']), 2)
        first = result['sources'][0]
        self.assertEqual(first['repository'], 'test/one')
        self.assertEqual(first['pins'], ['pin'])
        self.assertEqual(first['components'], ['client/', 'server/vendor/compiler'])
        self.assertEqual(first['provenance'], ['versions.lock.json#upstreams.first',
                                              'versions.lock.json#server_toolchain.compiler'])
        self.assertEqual((self.root / 'versions.lock.json').read_bytes(), original)
        self.assertFalse(result['mutations'])
        self.assertEqual(len([x for x in self.api.calls if x[2] == 'repos/test/one']), 1)
        self.assertTrue(any('pin...develop%2Fnew' in x[2] for x in self.api.calls))
        self.assertFalse(any('--method' in x for x in self.api.calls))

    def test_candidates_merge_duplicate_commits_preserve_security_and_pending_reviews(self):
        result = self.github.upstreams()
        candidates = [x for x in result['candidates'] if x['source'] == 'test/one']
        self.assertEqual(len(candidates), 3)
        commit = next(x for x in candidates if x['commit'] == 'new')
        self.assertEqual(commit['observations'], ['commit', 'pull-request'])
        self.assertEqual(commit['references'], ['https://example.test/c/new', 'https://example.test/pr/2'])
        self.assertEqual(commit['classification'], 'security')
        self.assertEqual(commit['priority'], 'P1')
        self.assertTrue(all(x['commit'] != 'pin' for x in candidates))
        self.assertTrue(all(x['license_review'] == 'pending' and x['compatibility'] == 'pending'
                            and x['strategy'] == 'review' and not x['import_allowed'] for x in candidates))

    def test_reference_sources_are_always_read_only_ideas(self):
        result = self.github.upstreams()
        candidates = [x for x in result['candidates'] if x['source'] == 'test/reference']
        self.assertTrue(any('commit' in x['observations'] for x in candidates))
        source = next(x for x in result['sources'] if x['reference_only'])
        self.assertEqual(source['commit_scope'], 'latest 20 commits, no recorded baseline')
        self.assertTrue(all(x['reference_only'] and not x['import_allowed'] for x in candidates))

    def test_unavailable_advisories_are_explicit_without_leaking_error_details(self):
        self.api.deny_security = True
        result = self.github.upstreams()
        self.assertEqual(len(result['errors']), 2)
        self.assertTrue(all('security-advisories' in x['endpoint'] for x in result['errors']))
        self.assertNotIn('private error', json.dumps(result))
        self.assertTrue(result['candidates'])

    def test_pagination_is_bounded_and_incomplete_coverage_is_reported(self):
        self.api.many = True
        result = self.github.upstreams()
        comparisons = [x for x in self.api.calls if '/compare/' in x[2]]
        self.assertEqual(len(comparisons), 3)
        self.assertTrue(any('Truncated' in x['error'] for x in result['errors']))
        self.assertEqual(len([x for x in result['candidates'] if x['source'] == 'test/one'
                              and x['commit'].startswith('new')]), 101)

    def test_comparison_pagination_includes_new_commits_on_later_pages(self):
        api = self.api
        def paginated(argv, **kwargs):
            if '/compare/' in argv[2]:
                page = int(argv[2].rsplit('page=', 1)[1])
                start, count = (0, 100) if page == 1 else (100, 1)
                payload = {'commits': [{'sha': f'page-{i}',
                    'html_url': f'https://example.test/page/{i}', 'commit': {'message': 'Change'}}
                    for i in range(start, start + count)]}
                return subprocess.CompletedProcess(argv, 0, json.dumps(payload), '')
            return api(argv, **kwargs)
        result = GitHub(self.root, paginated).upstreams()
        self.assertTrue(any(x['commit'] == 'page-100' for x in result['candidates']))
        self.assertFalse(result['errors'])

    def test_missing_default_branch_is_an_error_not_empty_success(self):
        def invalid(argv, **kwargs):
            return subprocess.CompletedProcess(argv, 0, '{}', '')
        result = GitHub(self.root, invalid).upstreams()
        self.assertFalse(result['candidates'])
        self.assertEqual(len(result['errors']), 2)


if __name__ == '__main__':
    unittest.main()
