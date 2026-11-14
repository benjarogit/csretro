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

class RegisteredSourceCoverageTests(unittest.TestCase):
    """Exercise the actual register through the existing observer, offline."""

    def observe(self, unavailable=()):
        root = Path(__file__).resolve().parents[2]
        calls = []
        def runner(argv, **kwargs):
            endpoint = argv[2]
            calls.append(endpoint)
            route = endpoint.split('?', 1)[0]
            if len(route.split('/')) == 3:
                value = {'default_branch': 'main'}
                code = 404 if route.removeprefix('repos/') in unavailable else 0
            elif '/compare/' in route:
                value, code = {'commits': []}, 0
            else:
                value, code = [], 0
            return subprocess.CompletedProcess(argv, code, json.dumps(value), '')
        before = (root / 'versions.lock.json').read_bytes()
        report = GitHub(root, runner).upstreams()
        self.assertEqual(before, (root / 'versions.lock.json').read_bytes())
        return report, calls, json.loads(before)

    def test_all_requested_and_previously_documented_sources_reach_the_observer(self):
        report, _, _ = self.observe()
        repositories = {source['repository'] for source in report['sources']}
        required = {
            'FWGS/xash3d-fwgs', 'CS-NextClient/NextClient',
            'CS-NextClient/NextClientServerApi', 'CS-NextClient/ncl-hl1-source-sdk',
            'ValveSoftware/halflife', 'tmp64/hl1_source_sdk', 'rehlds/ReHLDS',
            'rehlds/ReGameDLL_CS', 'rehlds/Metamod-R', 'FWGS/metamod-fwgs',
            'alliedmodders/amxmodx', 'rehlds/ReAPI', 'microsoft/vcpkg',
            'MoeMod/MetaAudio', 'jeefo/yapb', 'mikkokko/csldr',
            'MetaHookSv/MetaHookSv', 'FWGS/cs16-client', 'FuryBaM/cs16-goldsrc-client',
            'LAGonauta/MetaAudio', 'yapb/yapb', 'MoeMod/Thanatos-Launcher',
            'CS-GoldClient/avatar-spec', 'SanyaSho/BarsTech_goldsrc_compatible_public',
            'portapps/hlsw-portable', 'CM2Walki/steamcmd', 'zensical/zensical',
            'freetype/freetype', 'dejavu-fonts/dejavu-fonts',
            'liberationfonts/liberation-fonts',
        }
        self.assertEqual(repositories, required)
        self.assertFalse(report['mutations'])

    def test_verified_aliases_do_not_duplicate_polling_or_claim_unverified_redirects(self):
        report, calls, lock = self.observe()
        refs = lock['references']
        self.assertIn('https://github.com/Velaron/cs16-client', refs['fwgs_cs16_client']['aliases'])
        self.assertEqual(calls.count('repos/FWGS/cs16-client'), 1)
        self.assertFalse(any('repos/Velaron/cs16-client' in call for call in calls))
        self.assertNotIn('aliases', refs['meta_audio'])
        self.assertNotIn('aliases', refs['yapb'])
        self.assertEqual(refs['freetype_source']['canonical_upstream'],
                         'https://gitlab.freedesktop.org/freetype/freetype')
        for key in ('xash3d_fwgs', 'metahooksv', 'metamod_fwgs', 'meta_audio', 'yapb'):
            self.assertTrue(refs[key]['monitoring_only'])
            self.assertNotIn('commit', refs[key])

    def test_unavailable_requested_identities_stay_registered_and_visible(self):
        missing = ('MoeMod/MetaAudio', 'jeefo/yapb')
        report, _, _ = self.observe(missing)
        self.assertEqual({error['source'] for error in report['errors']}, set(missing))
        self.assertTrue(all(source['reference_only'] for source in report['sources']
                            if source['repository'] in missing))
        self.assertIn('LAGonauta/MetaAudio', {source['repository'] for source in report['sources']})
        self.assertIn('yapb/yapb', {source['repository'] for source in report['sources']})


if __name__ == '__main__':
    unittest.main()
