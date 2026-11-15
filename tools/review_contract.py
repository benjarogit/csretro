#!/usr/bin/env python3
"""Candidate provider-neutral content review contract. Never grants merge authority.

Trust is supplied by an independently approved executor profile, never by receipt
keys. No signing, enrollment, provider invocation or GitHub mutation is offered.
"""
from __future__ import annotations
import argparse
import base64
from dataclasses import asdict, dataclass
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile


class InvalidReview(ValueError):
    """Evidence is invalid; this is distinct from a substantive negative review."""


def canonical(value):
    return json.dumps(value, sort_keys=True, separators=(',', ':'), ensure_ascii=True).encode()


def digest(value):
    return hashlib.sha256(canonical(value)).hexdigest()


def require(condition, reason):
    if not condition:
        raise InvalidReview(reason)


def relative_path(value):
    require(isinstance(value, str) and value and '\\' not in value, 'Invalid review path')
    require('//' not in value and not value.endswith('/'), 'Noncanonical review path')
    path = Path(value)
    require(not path.is_absolute() and not any(part in ('.', '..', '.git', '.local')
            or part.startswith('.env') for part in value.split('/')), 'Private or escaping review path')
    return value


def hex_digest(value, length=64):
    return isinstance(value, str) and re.fullmatch('[0-9a-f]{' + str(length) + '}', value) is not None


@dataclass(frozen=True)
class Result:
    provider: str
    availability: str
    verdict: str
    trusted: bool
    reviewer: str
    reasons: tuple[str, ...]
    snapshot_sha256: str


def validate_snapshot(snapshot):
    require(type(snapshot.get('repository_id')) is int and snapshot['repository_id'] > 0, 'Repository ID missing')
    require(isinstance(snapshot.get('repository'), str) and re.fullmatch(r'[^/\s]+/[^/\s]+', snapshot['repository']), 'Repository name missing')
    require(type(snapshot.get('pr')) is int and snapshot['pr'] > 0, 'PR identity missing')
    require(hex_digest(snapshot.get('head'), 40) and hex_digest(snapshot.get('base'), 40)
            and snapshot['head'] != snapshot['base'], 'Exact distinct head/base required')
    require(hex_digest(snapshot.get('manifest_sha256')), 'Canonical manifest digest missing')
    criteria = snapshot.get('criterion_ids')
    require(isinstance(criteria, list) and criteria and all(isinstance(c, str) and c for c in criteria)
            and len(set(criteria)) == len(criteria), 'Duplicate or missing criterion IDs')
    files = snapshot.get('files')
    require(isinstance(files, list) and files, 'Complete changed-file inventory required')
    paths = []
    for file in files:
        paths.append(relative_path(file['path']))
        if file.get('previous_path'):
            old = relative_path(file['previous_path'])
            if old != file['path']: paths.append(old)
        require(file.get('status') in ('A', 'M', 'D', 'R', 'T'), 'Unsupported file change')
        for side in ('before', 'after'):
            blob, sha = file.get(side + '_blob'), file.get(side + '_sha256')
            require((blob is None and sha is None) or (hex_digest(blob, 40) and hex_digest(sha)), 'Blob/source fingerprint missing')
        require(file.get('before_blob') or file.get('after_blob'), 'Empty source entry')
        require((file['status'] != 'A' or file.get('before_blob') is None)
                and (file['status'] != 'D' or file.get('after_blob') is None)
                and (file['status'] != 'R' or file.get('previous_path')), 'Source status/blob mismatch')
    require(len(paths) == len(set(paths)), 'Duplicate changed paths')
    return set(paths)


def validate_assessment(assessment, snapshot, source_blobs):
    paths = validate_snapshot(snapshot)
    require(assessment.get('verdict') in ('pass', 'fail', 'inconclusive'), 'Unknown content verdict')
    covered = assessment.get('reviewed_paths')
    require(isinstance(covered, list) and len(covered) == len(paths) and set(covered) == paths, 'Incomplete/duplicate reviewed paths')
    require(isinstance(assessment.get('summary'), str) and len(assessment['summary'].strip()) >= 40, 'Substantive summary missing')
    decisions = assessment.get('criteria')
    require(isinstance(decisions, list) and len(decisions) == len(snapshot['criterion_ids'])
            and {d['id'] for d in decisions} == set(snapshot['criterion_ids']), 'Incomplete/duplicate criterion decisions')
    known_blobs = {file[side + '_blob']: file[side + '_sha256'] for file in snapshot['files']
                   for side in ('before', 'after') if file.get(side + '_blob')}
    for blob, sha in known_blobs.items():
        content = source_blobs.get(blob)
        require(isinstance(content, bytes) and hashlib.sha256(content).hexdigest() == sha, 'Source bytes altered/missing')
        actual = hashlib.sha1(b'blob ' + str(len(content)).encode() + b'\0' + content).hexdigest()
        require(actual == blob, 'Source Git blob does not match bytes')
    for decision in decisions:
        require(decision.get('verdict') in ('pass', 'fail', 'inconclusive')
                and isinstance(decision.get('reason'), str) and len(decision['reason'].strip()) >= 20, 'Criterion lacks concrete decision/reason')
        evidence = decision.get('evidence')
        require(isinstance(evidence, list) and evidence, 'Criterion source evidence missing')
        for item in evidence:
            require(item.get('path') in paths and item.get('blob') in known_blobs, 'Unknown source reference')
            associated = [f for f in snapshot['files'] if item['path'] in (f['path'], f.get('previous_path'))]
            require(any(item['blob'] in (f['before_blob'], f['after_blob']) for f in associated), 'Blob belongs to another path')
            quote = item.get('quote')
            require(isinstance(quote, str) and len(quote) >= 8 and quote.encode() in source_blobs[item['blob']], 'Source excerpt not found')
    findings = assessment.get('findings')
    require(isinstance(findings, list), 'Findings inventory missing')
    ids = []
    for finding in findings:
        require(isinstance(finding.get('id'), str) and finding['id'], 'Finding ID missing')
        ids.append(finding['id'])
        require(finding.get('path') in paths and finding.get('severity') in ('critical', 'major', 'minor', 'info')
                and finding.get('state') in ('open', 'resolved') and isinstance(finding.get('reason'), str)
                and len(finding['reason'].strip()) >= 20, 'Malformed finding')
    require(len(ids) == len(set(ids)), 'Duplicate findings')
    if assessment['verdict'] == 'pass':
        require(all(item['verdict'] == 'pass' for item in decisions), 'Pass contradicts criterion decision')
        require(not any(f['state'] == 'open' and f['severity'] != 'info' for f in findings), 'Pass hides unresolved findings')


def verify_receipt(envelope, snapshot, trust, trust_pin, raw_input, raw_output, implementers):
    """Verify origin and exact evidence. A signature never proves semantic quality."""
    require(digest(trust) == trust_pin, 'Trust profile is not the independently pinned profile')
    require(trust.get('schema') == 1, 'Unsupported trust profile')
    if trust.get('enabled') is not True:
        return Result('alternative', 'unavailable', 'not-run', False, '', ('Alternative route is not enrolled',), digest(snapshot))
    validate_snapshot(snapshot)
    require(envelope.get('schema') == 1, 'Unsupported receipt envelope')
    payload, signature = envelope['payload'], envelope['signature']
    require(signature.get('scheme') == 'ed25519', 'Unsupported signature')
    key = trust.get('executors', {}).get(signature.get('key_id'))
    require(isinstance(key, dict) and key.get('revoked') is False, 'Unknown or revoked executor; receipt keys cannot self-enroll')
    require(payload.get('contract') == 'csretro-review/v1' and payload.get('snapshot') == snapshot, 'Stale or altered source/manifest binding')
    require(payload.get('implementers') == sorted(implementers) and implementers, 'Implementer context mismatch')
    reviewer, execution = payload['reviewer'], payload['execution']
    require(reviewer.get('actor') not in implementers and reviewer.get('actor'), 'Implementer cannot approve its own work')
    require(reviewer.get('provider') in key.get('providers', []) and execution.get('executor') == key.get('executor'), 'Wrong provider/executor identity')
    require(reviewer.get('actor') in key.get('reviewer_actors', []), 'Unenrolled reviewer identity')
    require(reviewer.get('runtime_digest') in key.get('runtime_digests', []), 'Runtime/model provenance not pinned')
    require(execution.get('completed') is True and isinstance(execution.get('id'), str) and execution['id'], 'Review did not execute to completion')
    require(execution.get('input_sha256') == hashlib.sha256(raw_input).hexdigest()
            and execution.get('output_sha256') == hashlib.sha256(raw_output).hexdigest(), 'Execution input/output altered')
    request = json.loads(raw_input)
    require(request.get('snapshot') == snapshot and digest(request.get('instructions')) == key.get('prompt_sha256'), 'Source/prompt provenance mismatch')
    manifest = request.get('acceptance_manifest')
    require(isinstance(manifest, dict) and digest(manifest) == snapshot['manifest_sha256'], 'Actual acceptance manifest missing/altered')
    require(manifest.get('base_sha') == snapshot['base'] and manifest.get('schema') == 1, 'Manifest source base/schema mismatch')
    require([c['id'] for c in manifest['criteria']] == snapshot['criterion_ids']
            and all(isinstance(c.get('requirement'), str) and c['requirement'].strip() for c in manifest['criteria']), 'Manifest criterion requirements missing/mismatched')
    expected_paths = validate_snapshot(snapshot)
    require(isinstance(manifest.get('changed_paths'), list) and len(manifest['changed_paths']) == len(expected_paths)
            and set(manifest['changed_paths']) == expected_paths, 'Manifest changed-path inventory mismatch')
    report = json.loads(raw_output)
    require(report == payload.get('assessment'), 'Reported review differs from actual executor output')
    blobs = {blob: base64.b64decode(value, validate=True) for blob, value in request['source_blobs'].items()}
    validate_assessment(report, snapshot, blobs)
    Path('/tmp/opencode').mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='review-verify-', dir='/tmp/opencode') as folder:
        folder = Path(folder)
        (folder / 'key.pem').write_text(key['public_key_pem'])
        require('PRIVATE KEY' not in key['public_key_pem'], 'Private key is forbidden in trust profile')
        (folder / 'payload.json').write_bytes(canonical(payload))
        (folder / 'signature.bin').write_bytes(base64.b64decode(signature['value'], validate=True))
        completed = subprocess.run(['openssl', 'pkeyutl', '-verify', '-pubin', '-inkey', str(folder / 'key.pem'),
            '-rawin', '-in', str(folder / 'payload.json'), '-sigfile', str(folder / 'signature.bin')], capture_output=True)
        require(completed.returncode == 0, 'Review origin signature is invalid')
    return Result(reviewer['provider'], 'available', report['verdict'], True, reviewer['actor'], (), digest(snapshot))


def choose(results, expected_snapshot_sha256):
    """Unavailable providers may fall back; negative reviews may never be hidden."""
    require(all(isinstance(result, Result) for result in results), 'Only adapter-validated results may be selected')
    require(hex_digest(expected_snapshot_sha256) and all(r.snapshot_sha256 == expected_snapshot_sha256 for r in results), 'Cannot combine reviews of different sources')
    executed = [r for r in results if r.trusted and r.availability == 'available']
    if any(r.verdict == 'fail' for r in executed): return 'blocked-negative-review'
    if any(r.verdict == 'pass' for r in executed): return 'content-accepted'
    return 'content-unavailable-or-inconclusive'


def formal_approval_holds(pr, reviews, permissions):
    """Content receipts never substitute for actual independent GitHub approval."""
    latest = {}
    for review in sorted(reviews, key=lambda r: r['id']):
        if review['state'] in ('APPROVED', 'CHANGES_REQUESTED', 'DISMISSED'):
            latest[review['user']['id']] = review
    if any(r['state'] == 'CHANGES_REQUESTED' for uid, r in latest.items() if uid != pr['user']['id']):
        return ['Independent blocking review remains']
    eligible = [r for uid, r in latest.items() if uid != pr['user']['id'] and r.get('commit_id') == pr['head']['sha']
                and permissions.get(uid) in ('write', 'maintain', 'admin')]
    if any(r['state'] == 'CHANGES_REQUESTED' for r in eligible): return ['Independent blocking review remains']
    return [] if any(r['state'] == 'APPROVED' for r in eligible) else ['Actual current-head approval by a different authorized GitHub identity is missing']


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for flag in ('snapshot', 'trust', 'trust-pin', 'receipt', 'input', 'output'):
        parser.add_argument('--' + flag, required=True)
    parser.add_argument('--implementer', action='append', required=True)
    args = parser.parse_args()
    try:
        result = verify_receipt(json.loads(Path(args.receipt).read_text()), json.loads(Path(args.snapshot).read_text()),
            json.loads(Path(args.trust).read_text()), args.trust_pin, Path(args.input).read_bytes(),
            Path(args.output).read_bytes(), args.implementer)
        print(json.dumps(asdict(result), indent=2))
        raise SystemExit(0 if result.trusted and result.verdict == 'pass' else 1)
    except (InvalidReview, KeyError, TypeError, ValueError) as error:
        print(json.dumps({'status': 'invalid-evidence', 'reason': str(error)}))
        raise SystemExit(2)


if __name__ == '__main__': main()
