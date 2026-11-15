"""Real cryptographic fixture receipts are not live reviewer enrollment."""
import base64
import copy
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch, Mock

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
import review_contract as rc
import review_adapters as adapters


class ContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory(prefix='review-tests-')
        cls.key = Path(cls.tmp.name) / 'fixture.key'
        subprocess.run(['openssl', 'genpkey', '-algorithm', 'ed25519', '-out', str(cls.key)], check=True, capture_output=True)
        cls.public = subprocess.check_output(['openssl', 'pkey', '-in', str(cls.key), '-pubout']).decode()

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def setUp(self):
        self.content = b'Reviewed source requirement is preserved.\n'
        blob = hashlib.sha1(b'blob ' + str(len(self.content)).encode() + b'\0' + self.content).hexdigest()
        self.manifest = {'schema': 1, 'base_sha': 'b'*40, 'changed_paths': ['docs/a.md'],
                         'criteria': [{'id': 'preserved', 'requirement': 'Preserve source requirement'}]}
        self.snapshot = {'repository_id': 1, 'repository': 'owner/repo', 'pr': 18, 'head': 'a'*40, 'base': 'b'*40,
                         'manifest_sha256': rc.digest(self.manifest), 'criterion_ids': ['preserved'], 'files': [
                             {'path': 'docs/a.md', 'status': 'A', 'before_blob': None, 'before_sha256': None,
                              'after_blob': blob, 'after_sha256': hashlib.sha256(self.content).hexdigest()}]}
        self.report = {'verdict': 'pass', 'reviewed_paths': ['docs/a.md'],
                       'summary': 'The independently inspected source retains the requested behavior.',
                       'criteria': [{'id': 'preserved', 'verdict': 'pass', 'reason': 'Source retains the specified requirement.',
                                     'evidence': [{'path': 'docs/a.md', 'blob': blob, 'quote': 'source requirement'}]}], 'findings': []}
        self.request = {'snapshot': self.snapshot, 'instructions': 'Review all criteria independently.',
                        'acceptance_manifest': self.manifest, 'source_blobs': {blob: base64.b64encode(self.content).decode()}}
        self.trust = {'schema': 1, 'enabled': True, 'executors': {'test-only': {'revoked': False,
                      'providers': ['native-agent', 'local-model'], 'executor': 'fixture-host', 'reviewer_actors': ['reviewer'],
                      'runtime_digests': ['model-sha256:test-only'], 'prompt_sha256': rc.digest(self.request['instructions']),
                      'public_key_pem': self.public}}}
        self.actor = {'actor': 'reviewer', 'provider': 'native-agent', 'runtime_digest': 'model-sha256:test-only'}
        self.execution = {'executor': 'fixture-host', 'completed': True, 'id': 'actual-test-execution'}

    def receipt(self):
        raw_input, raw_output = rc.canonical(self.request), rc.canonical(self.report)
        payload = {'contract': 'csretro-review/v1', 'snapshot': self.snapshot, 'implementers': ['author'],
                   'reviewer': self.actor, 'execution': dict(self.execution, input_sha256=hashlib.sha256(raw_input).hexdigest(),
                   output_sha256=hashlib.sha256(raw_output).hexdigest()), 'assessment': self.report}
        data = Path(self.tmp.name) / 'payload.json'
        data.write_bytes(rc.canonical(payload))
        signature = subprocess.check_output(['openssl', 'pkeyutl', '-sign', '-inkey', str(self.key), '-rawin', '-in', str(data)])
        return {'schema': 1, 'payload': copy.deepcopy(payload), 'signature': {'scheme': 'ed25519', 'key_id': 'test-only',
                'value': base64.b64encode(signature).decode()}}, raw_input, raw_output

    def verify(self, receipt=None):
        envelope, raw_input, raw_output = receipt or self.receipt()
        return rc.verify_receipt(envelope, self.snapshot, self.trust, rc.digest(self.trust), raw_input, raw_output, ['author'])

    def test_genuine_fixture_signature_and_full_source_accept(self):
        result = self.verify()
        self.assertEqual(rc.choose([result], rc.digest(self.snapshot)), 'content-accepted')

    def test_local_model_uses_same_contract(self):
        self.actor['provider'] = 'local-model'
        self.assertEqual(self.verify().verdict, 'pass')

    def test_disabled_route_cannot_enroll_itself(self):
        self.trust['enabled'] = False
        self.assertFalse(self.verify().trusted)

    def test_corrupt_signature_and_wire_bytes(self):
        envelope, request, output = self.receipt()
        envelope['signature']['value'] = base64.b64encode(b'x'*64).decode()
        for sample in ((envelope, request, output), (self.receipt()[0], request+b' ', output),
                       (self.receipt()[0], request, output+b' ')):
            with self.subTest(sample=sample[1][-2:]), self.assertRaises(rc.InvalidReview): self.verify(sample)

    def test_unknown_revoked_and_embedded_key_do_not_enroll(self):
        for mutation in ('unknown', 'revoked', 'embedded'):
            envelope, request, output = self.receipt()
            if mutation == 'revoked': self.trust['executors']['test-only']['revoked'] = True
            else: envelope['signature']['key_id'] = 'untrusted'
            envelope['public_key_pem'] = self.public
            with self.subTest(mutation=mutation), self.assertRaises(rc.InvalidReview): self.verify((envelope, request, output))

    def test_self_review_unexecuted_and_unpinned_runtime(self):
        for target, key, value in ((self.actor, 'actor', 'author'), (self.actor, 'runtime_digest', 'other'),
                                   (self.execution, 'completed', False)):
            old = target[key]
            target[key] = value
            with self.subTest(key=key), self.assertRaises(rc.InvalidReview): self.verify()
            target[key] = old

    def test_stale_or_altered_source_binding(self):
        receipt = self.receipt()
        for field, value in (('head', 'c'*40), ('base', 'd'*40), ('repository_id', 2), ('pr', 19), ('manifest_sha256', 'e'*64)):
            old = self.snapshot[field]
            self.snapshot[field] = value
            with self.subTest(field=field), self.assertRaises(rc.InvalidReview): self.verify(receipt)
            self.snapshot[field] = old

    def test_actual_manifest_required_and_hash_bound(self):
        for kind in ('missing', 'requirement', 'id'):
            original = copy.deepcopy(self.request)
            if kind == 'missing': self.request.pop('acceptance_manifest')
            elif kind == 'requirement': self.request['acceptance_manifest']['criteria'][0]['requirement'] = 'Ignore previous requirements'
            else: self.request['acceptance_manifest']['criteria'][0]['id'] = 'invented'
            with self.subTest(kind=kind), self.assertRaises(rc.InvalidReview): self.verify()
            self.request = original

    def test_missing_duplicate_paths_criteria_and_invented_quote(self):
        for kind in ('missing-path', 'duplicate-path', 'missing-criterion', 'duplicate-criterion', 'invented-quote'):
            original = copy.deepcopy(self.report)
            if kind == 'missing-path': self.report['reviewed_paths'] = []
            elif kind == 'duplicate-path': self.report['reviewed_paths'] *= 2
            elif kind == 'missing-criterion': self.report['criteria'] = []
            elif kind == 'duplicate-criterion': self.report['criteria'] *= 2
            else: self.report['criteria'][0]['evidence'][0]['quote'] = 'not present in any source'
            with self.subTest(kind=kind), self.assertRaises(rc.InvalidReview): self.verify()
            self.report = original

    def test_modified_source_bytes(self):
        blob = next(iter(self.request['source_blobs']))
        self.request['source_blobs'][blob] = base64.b64encode(b'Other content').decode()
        with self.assertRaises(rc.InvalidReview): self.verify()

    def test_open_finding_cannot_pass(self):
        self.report['findings'] = [{'id': 'one', 'path': 'docs/a.md', 'severity': 'major', 'state': 'open',
                                   'reason': 'Concrete requirement is not satisfied.'}]
        with self.assertRaises(rc.InvalidReview): self.verify()

    def test_unavailable_is_distinct_from_negative(self):
        current = rc.digest(self.snapshot)
        unavailable = rc.Result('service', 'unavailable', 'not-run', False, '', (), current)
        passed = self.verify()
        failed = rc.Result('service', 'available', 'fail', True, 'reviewer', (), current)
        self.assertEqual(rc.choose([unavailable, passed], current), 'content-accepted')
        self.assertEqual(rc.choose([failed, passed], current), 'blocked-negative-review')
        self.assertEqual(rc.choose([unavailable], current), 'content-unavailable-or-inconclusive')

    def test_mixed_snapshot_cannot_select_old_pass(self):
        passed = self.verify()
        with self.assertRaises(rc.InvalidReview): rc.choose([passed], 'f'*64)

    def test_untrusted_profile_pin(self):
        envelope, request, output = self.receipt()
        with self.assertRaises(rc.InvalidReview):
            rc.verify_receipt(envelope, self.snapshot, self.trust, 'f'*64, request, output, ['author'])

    def test_formal_review_is_separate_and_author_blocked(self):
        pr = {'user': {'id': 1}, 'head': {'sha': 'a'*40}}
        vote = {'id': 1, 'user': {'id': 1}, 'state': 'APPROVED', 'commit_id': 'a'*40}
        self.assertTrue(rc.formal_approval_holds(pr, [vote], {1:'admin'}))
        vote['user']['id'] = 2
        self.assertFalse(rc.formal_approval_holds(pr, [vote], {2:'write'}))
        vote['commit_id'] = 'b'*40
        self.assertTrue(rc.formal_approval_holds(pr, [vote], {2:'write'}))

    def test_old_blocking_review_survives_new_head_and_other_pass(self):
        pr = {'user': {'id': 1}, 'head': {'sha': 'a'*40}}
        votes = [{'id': 1, 'user': {'id': 2}, 'state': 'CHANGES_REQUESTED', 'commit_id': 'b'*40},
                 {'id': 2, 'user': {'id': 3}, 'state': 'APPROVED', 'commit_id': 'a'*40}]
        self.assertIn('blocking', rc.formal_approval_holds(pr, votes, {2:'write',3:'write'})[0])

    def test_provider_adapter_rejects_wrong_repository_pr_base_manifest_and_files(self):
        live = copy.deepcopy(self.snapshot)
        inspected = {'head':live['head'], 'acceptance_sha256':live['manifest_sha256'], 'eligible':True, 'holds':[]}
        with patch('review_adapters.source_request', return_value={'snapshot':live}), patch('github_acceptance.inspect', return_value=inspected):
            self.assertTrue(adapters.coderabbit(None, 18, {'reviewer':'service'}, self.snapshot).trusted)
            for field, value in (('repository_id', 99), ('repository','other/repo'), ('pr',19), ('base','f'*40),
                                 ('manifest_sha256','e'*64), ('files',[])):
                wrong = dict(self.snapshot, **{field:value})
                with self.subTest(field=field), self.assertRaises(rc.InvalidReview):
                    adapters.coderabbit(None, 18, {'reviewer':'service'}, wrong)
            inspected['eligible'] = False
            inspected['holds'] = ['Negative actual review']
            with patch('review_adapters.coderabbit_state', return_value=None), self.assertRaises(rc.InvalidReview):
                adapters.coderabbit(None, 18, {'reviewer':'service'}, self.snapshot)

    def test_authenticated_quota_is_not_failed_review(self):
        policy = {'reviewer':'service', 'reviewer_user_id':7, 'reviewer_app_id':8}
        github = Mock()
        github.pages.return_value = []
        check = {'id':1, 'name':'CodeRabbit', 'app':{'id':8}, 'head_sha':self.snapshot['head'],
                 'status':'completed', 'conclusion':'failure', 'output':{'title':'Review rate limited'}}
        github.api.return_value = {'check_runs':[check]}
        self.assertEqual(adapters.coderabbit_state(github,18,policy,self.snapshot).availability, 'unavailable')
        check['output']['title'] = 'Actual code findings'
        self.assertEqual(adapters.coderabbit_state(github,18,policy,self.snapshot).verdict, 'fail')
        check['app']['id'] = 999
        self.assertFalse(adapters.coderabbit_state(github,18,policy,self.snapshot).trusted)
        github.pages.side_effect = [[{'id':1,'user':{'id':7,'login':'service'},'state':'CHANGES_REQUESTED'}]]
        self.assertEqual(adapters.coderabbit_state(github,18,policy,self.snapshot).verdict, 'fail')

    def test_quota_rerun_cannot_erase_prior_failure(self):
        policy = {'reviewer':'service', 'reviewer_user_id':7, 'reviewer_app_id':8}
        github = Mock()
        github.pages.return_value = []
        failed = {'id':1, 'name':'CodeRabbit','app':{'id':8},'head_sha':self.snapshot['head'],
                  'status':'completed','conclusion':'failure','output':{'title':'Substantive findings'}}
        quota = dict(failed, id=2, output={'title':'Review rate limited'})
        github.api.return_value = {'check_runs':[failed,quota]}
        self.assertEqual(adapters.coderabbit_state(github,18,policy,self.snapshot).verdict,'fail')
        # Only a full genuine substantive validator PASS can supersede failure;
        # a success check by itself still cannot be used for alternative fallback.
        quota['conclusion'] = 'success'
        quota['output'] = {'title':'Review completed'}
        self.assertEqual(adapters.coderabbit_state(github,18,policy,self.snapshot).verdict,'fail')
        with patch('review_adapters.source_request',return_value={'snapshot':self.snapshot}), \
             patch('github_acceptance.inspect',return_value={'head':self.snapshot['head'],
                   'acceptance_sha256':self.snapshot['manifest_sha256'],'eligible':True,'holds':[]}):
            self.assertEqual(adapters.coderabbit(github,18,policy,self.snapshot).verdict,'pass')

    def test_source_capture_uses_authenticated_metadata_and_actual_bytes(self):
        github = Mock()
        pr = {'head':{'sha':self.snapshot['head']}, 'base':{'sha':self.snapshot['base']}}
        blob = self.snapshot['files'][0]['after_blob']
        def api(path):
            if not path: return {'id':1,'full_name':'owner/repo'}
            if path == 'pulls/18': return pr
            if path.startswith('contents/'):
                return {'type':'file','encoding':'base64','content':base64.b64encode(self.content).decode(),'sha':blob}
            raise AssertionError(path)
        github.api.side_effect = api
        github.pages.return_value = [{'filename':'docs/a.md','status':'added'}]
        with patch('github_acceptance.read_artifact', return_value=self.manifest):
            actual = adapters.source_request(github,18,{},'Review source')
            self.assertEqual(actual['snapshot'], self.snapshot)
            self.assertEqual(actual['source_blobs'], self.request['source_blobs'])
            github.pages.return_value = [{'filename':'docs/a.md','status':'invented'}]
            with self.assertRaises(rc.InvalidReview): adapters.source_request(github,18,{},'Review source')

    def test_private_paths_and_inconsistent_blob_status_rejected(self):
        for path in ('/etc/passwd','../outside','.local/secrets','.env','docs//a.md','docs/../a.md'):
            with self.subTest(path=path), self.assertRaises(rc.InvalidReview): rc.relative_path(path)
        self.snapshot['files'][0]['status'] = 'D'
        with self.assertRaises(rc.InvalidReview): rc.validate_snapshot(self.snapshot)

    def test_base_policy_pin_is_required_even_when_file_exists(self):
        completed = Mock(returncode=0, stdout=rc.canonical(self.trust))
        with patch('review_adapters.subprocess.run', return_value=completed) as run:
            self.assertEqual(adapters.base_trust('.', 'b'*40, rc.digest(self.trust)), self.trust)
            self.assertEqual(run.call_args.args[0][-1], 'b'*40+':.github/review-contract.json')
            with self.assertRaises(rc.InvalidReview): adapters.base_trust('.', 'b'*40, 'f'*64)

    def test_head_cannot_enroll_missing_base_policy(self):
        with tempfile.TemporaryDirectory(prefix='review-git-') as tmp:
            subprocess.run(['git', 'init', '-q', tmp], check=True)
            subprocess.run(['git', '-C', tmp, '-c', 'user.name=Test', '-c', 'user.email=test@example.invalid',
                            'commit', '--allow-empty', '-qm', 'base'], check=True)
            base = subprocess.check_output(['git', '-C', tmp, 'rev-parse', 'HEAD']).decode().strip()
            Path(tmp, '.github').mkdir()
            Path(tmp, '.github/review-contract.json').write_text(json.dumps(self.trust))
            with self.assertRaises(rc.InvalidReview): adapters.base_trust(tmp, base, rc.digest(self.trust))


if __name__ == '__main__': unittest.main()
