"""Regression tests include real, hash-verified scanner positive/negative cases."""

import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

import run_checks as checks

CLEAN = '''name: Fixture
on: workflow_dispatch
permissions:
  contents: read
jobs:
  check:
    runs-on: ubuntu-24.04
    steps:
      - uses: actions/checkout@11bd71901bbe5b1630ceea73d27597364c9af683
        with:
          persist-credentials: false
      - run: echo checked
'''


def fixture(root, workflow=CLEAN):
    subprocess.run(['git', 'init', '--quiet', str(root)], check=True, capture_output=True)
    path = root / '.github/workflows/check.yml'
    path.parent.mkdir(parents=True)
    path.write_text(workflow, encoding='utf-8')
    (root / 'README.md').write_text('Clean fixture\n', encoding='utf-8')


def stage(root):
    subprocess.run(['git', '-C', str(root), 'add', '.'], check=True, capture_output=True)
    subprocess.run(['git', '-C', str(root), '-c', 'user.name=Security fixture',
                    '-c', 'user.email=fixture@example.invalid', 'commit', '--quiet', '-m', 'Fixture'],
                   check=True, capture_output=True)


class RunnerBoundaryTests(unittest.TestCase):
    def test_invalid_and_missing_json_never_pass(self):
        for tool in ('gitleaks', 'zizmor'):
            for raw in (b'', b'null', b'{}', b'not-json', b'[null]', b'[{}]'):
                with self.subTest(tool=tool, raw=raw), self.assertRaises(checks.CheckError):
                    checks.parse_findings(tool, subprocess.CompletedProcess([], 0), raw)

    def test_process_failures_and_empty_finding_exit_never_pass(self):
        for tool, codes in [('gitleaks', [1, 2, 127]), ('zizmor', [1, 2, 3, 11, 14, 127])]:
            for code in codes:
                with self.subTest(tool=tool, code=code), self.assertRaises(checks.CheckError):
                    checks.parse_findings(tool, subprocess.CompletedProcess([], code), b'[]')

    def test_findings_cannot_pass_when_tool_returns_zero_or_include_secret(self):
        raw = json.dumps([{'RuleID': 'synthetic', 'File': 'patch.diff', 'StartLine': 2,
                           'Secret': 'must-never-appear', 'Match': 'also-sensitive'}]).encode()
        safe = checks.parse_findings('gitleaks', subprocess.CompletedProcess([], 0), raw)
        self.assertTrue(safe)
        self.assertNotIn('must-never-appear', json.dumps(safe))
        self.assertNotIn('also-sensitive', json.dumps(safe))

    def test_missing_report_is_error(self):
        with tempfile.TemporaryDirectory() as temp, patch.object(checks, 'checked_process') as call:
            call.return_value = subprocess.CompletedProcess([], 0, b'[]', b'')
            result = checks.scan_snapshot(Path(temp), ['test.yml'], {'gitleaks': 'unused', 'zizmor': 'unused'})
            self.assertEqual(result['gitleaks']['status'], 'error')

    def test_empty_tracked_inputs_and_missing_workflow_fail(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            subprocess.run(['git', 'init', '--quiet', str(root)], check=True, capture_output=True)
            with self.assertRaises(checks.CheckError):
                checks.snapshot(root, root / 'snapshot')
            (root / 'README.md').write_text('no workflow')
            stage(root)
            with self.assertRaises(checks.CheckError):
                checks.snapshot(root, root / 'snapshot')

    def test_binary_workflow_is_not_hidden_by_another_valid_workflow(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp) / 'repo'
            fixture(root)
            (root / '.github/workflows/binary.yml').write_bytes(CLEAN.encode('utf-16'))
            stage(root)
            with self.assertRaisesRegex(checks.CheckError, 'Actions definition is binary'):
                checks.snapshot(root, Path(temp) / 'snapshot')

    def test_download_hash_mismatch_stops_before_execution(self):
        with tempfile.TemporaryDirectory() as temp, patch('urllib.request.urlopen', return_value=io.BytesIO(b'bad')):
            with self.assertRaisesRegex(checks.CheckError, 'archive SHA-256 mismatch'):
                checks.ensure_tools(Path(temp))

    def test_cached_binary_hash_mismatch_is_not_replaced_or_run(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            lock = json.loads(checks.LOCK.read_text())['tools']['gitleaks']
            spec = lock['platforms'][checks.platform.system().lower()]
            path = root / 'gitleaks' / lock['version'] / spec['member']
            path.parent.mkdir(parents=True)
            path.write_bytes(b'tampered')
            with self.assertRaisesRegex(checks.CheckError, 'cached executable SHA-256 mismatch'):
                checks.ensure_tools(root)
            self.assertEqual(path.read_bytes(), b'tampered')

    def test_advisory_never_downgrades_preparation_errors(self):
        with tempfile.TemporaryDirectory() as temp, patch.object(checks, 'ensure_tools', side_effect=checks.CheckError('hash mismatch')):
            for mode in ('advisory', 'strict'):
                report = checks.run(Path(temp), Path(temp), Path(temp) / mode, mode=mode)
                self.assertEqual(report['status'], 'error')

    def test_zizmor_locations_survive_without_sensitive_source_fields(self):
        location = {'symbolic': {'key': {'Local': {'verbatim_path': '.github/workflows/check.yml'}},
                                 'annotation': 'sensitive-annotation'},
                    'concrete': {'feature': 'sensitive-source', 'comments': ['sensitive-comment'],
                                 'location': {'start_point': {'row': 2, 'column': 3},
                                              'end_point': {'row': 2, 'column': 8}}}}
        finding = {'ident': 'fixture', 'determinations': {'severity': 'High', 'confidence': 'High'},
                   'locations': [location]}
        safe = checks.parse_findings('zizmor', subprocess.CompletedProcess([], 14), json.dumps([finding]))
        self.assertEqual(safe[0]['locations'][0]['line'], 3)
        self.assertEqual(safe[0]['locations'][0]['file'], '.github/workflows/check.yml')
        self.assertNotIn('sensitive-', json.dumps(safe))
        finding['determinations']['severity'] = 'malformed'
        with self.assertRaises(checks.CheckError):
            checks.parse_findings('zizmor', subprocess.CompletedProcess([], 14), json.dumps([finding]))
        for field in ('severity', 'confidence'):
            finding['determinations'] = {'severity': 'High', 'confidence': 'High'}
            finding['determinations'][field] = 'Unknown'
            with self.subTest(field=field), self.assertRaises(checks.CheckError):
                checks.parse_findings('zizmor', subprocess.CompletedProcess([], 14), json.dumps([finding]))

    def test_invalid_finding_paths_and_positions_fail_closed(self):
        for name, line in [('../escape', 1), ('/absolute', 1), ('file', 0), ('file', True)]:
            finding = {'RuleID': 'fixture', 'File': name, 'StartLine': line}
            with self.subTest(name=name, line=line), self.assertRaises(checks.CheckError):
                checks.parse_findings('gitleaks', subprocess.CompletedProcess([], 1), json.dumps([finding]))


class RealScannerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.cache = tempfile.TemporaryDirectory(prefix='midot-test-tools-')
        cls.tools_dir = Path(os.environ.get('MIDOT_SECURITY_TOOLS', cls.cache.name))
        cls.tools = checks.ensure_tools(cls.tools_dir)

    @classmethod
    def tearDownClass(cls):
        cls.cache.cleanup()

    def test_clean_repository_passes_both_real_scanners(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp) / 'repo'
            fixture(root)
            stage(root)
            report = checks.run(root, self.tools_dir, Path(temp) / 'output')
            self.assertEqual(report['status'], 'pass', report)
            self.assertTrue(report['source']['actions_inputs'])

    def test_synthetic_secret_in_patch_fails_and_is_redacted(self):
        # Entirely fabricated, never sent to a provider or accepted as a credential.
        secret = 'gh' + 'p_' + 'aZ9qU8xW7vT6sR5pN4mL3kJ2hG1fE0dC9bA8'
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp) / 'repo'
            fixture(root)
            path = root / 'customizations/patches/synthetic.patch'
            path.parent.mkdir(parents=True)
            path.write_text('diff --git a/token.txt b/token.txt\n+TOKEN=' + secret + '\n')
            stage(root)
            output = Path(temp) / 'output'
            report = checks.run(root, self.tools_dir, output, mode='strict')
            self.assertEqual(report['status'], 'fail')
            self.assertEqual(report['checks']['gitleaks']['status'], 'findings', report)
            self.assertTrue(any(f['file'].endswith('synthetic.patch') for f in report['checks']['gitleaks']['findings']))
            self.assertNotIn(secret, (output / 'summary.json').read_text())

    def test_unsafe_workflow_fails_even_without_sarif(self):
        expression = '$' + '{{ github.event.pull_request.title }}'
        unsafe = CLEAN.replace('on: workflow_dispatch', 'on: pull_request')
        unsafe = unsafe.replace('echo checked', 'echo "' + expression + '"')
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp) / 'repo'
            fixture(root, unsafe)
            stage(root)
            report = checks.run(root, self.tools_dir, Path(temp) / 'output', mode='strict')
            self.assertEqual(report['status'], 'fail')
            findings = report['checks']['zizmor']['findings']
            self.assertTrue(any(f['rule'] == 'template-injection' for f in findings), report)
            self.assertGreater(report['checks']['zizmor']['exit_code'], 0)

    def test_tracked_gitleaksignore_cannot_suppress_a_real_finding(self):
        secret = 'gh' + 'p_' + 'aZ9qU8xW7vT6sR5pN4mL3kJ2hG1fE0dC9bA8'
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp) / 'repo'
            fixture(root)
            (root / 'secret.txt').write_text('TOKEN=' + secret)
            raw_report = Path(temp) / 'raw.json'
            subprocess.run([str(self.tools['gitleaks']), 'dir', str(root), '--redact=100',
                            '--report-format=json', '--report-path', str(raw_report)],
                           cwd=root, capture_output=True, check=False)
            findings = json.loads(raw_report.read_text())
            self.assertTrue(findings)
            (root / '.gitleaksignore').write_text(findings[0]['Fingerprint'] + '\n')
            stage(root)
            output = Path(temp) / 'output'
            report = checks.run(root, self.tools_dir, output)
            self.assertEqual(report['status'], 'error')
            self.assertIn('Tracked .gitleaksignore', report['error'])
            self.assertNotIn(secret, (output / 'summary.json').read_text())

    def test_malformed_workflow_is_a_failed_scan(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp) / 'repo'
            fixture(root, 'on: [\n')
            stage(root)
            report = checks.run(root, self.tools_dir, Path(temp) / 'output')
            self.assertNotEqual(report['status'], 'pass')
            self.assertEqual(report['checks']['zizmor']['status'], 'error')

    def test_real_findings_are_advisory_but_strict_cli_fails(self):
        secret = 'gh' + 'p_' + 'aZ9qU8xW7vT6sR5pN4mL3kJ2hG1fE0dC9bA8'
        expression = '$' + '{{ github.event.pull_request.title }}'
        workflow = CLEAN.replace('on: workflow_dispatch', 'on: pull_request').replace('echo checked', 'echo "' + expression + '"')
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp) / 'repo'
            fixture(root, workflow)
            (root / 'token.patch').write_text('+TOKEN=' + secret)
            stage(root)
            for mode, code, status in [('advisory', 0, 'advisory'), ('strict', 1, 'fail')]:
                output = Path(temp) / mode
                environment = os.environ.copy()
                environment.pop('GITHUB_OUTPUT', None)
                result = subprocess.run([sys.executable, str(Path(checks.__file__).resolve()), '--repo', str(root),
                                         '--tools-dir', str(self.tools_dir), '--output', str(output), '--mode', mode],
                                        capture_output=True, env=environment)
                self.assertEqual(result.returncode, code, result.stdout)
                text = (output / 'summary.json').read_text()
                report = json.loads(text)
                self.assertEqual(report['status'], status)
                self.assertGreaterEqual(report['finding_count'], 2)
                self.assertTrue(report['checks']['zizmor']['findings'][0]['locations'])
                self.assertNotIn(secret, text)

    def test_real_malformed_workflow_is_nonzero_in_advisory_cli(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp) / 'repo'
            fixture(root, 'on: [\n')
            stage(root)
            output = Path(temp) / 'output'
            environment = os.environ.copy()
            environment.pop('GITHUB_OUTPUT', None)
            result = subprocess.run([sys.executable, str(Path(checks.__file__).resolve()), '--repo', str(root),
                                     '--tools-dir', str(self.tools_dir), '--output', str(output), '--mode', 'advisory'],
                                    capture_output=True, env=environment)
            self.assertEqual(result.returncode, 1)
            self.assertEqual(json.loads((output / 'summary.json').read_text())['status'], 'error')

    def test_real_informational_finding_is_advisory_and_strict_fails(self):
        # Static scanner input only; this text is never executed or published.
        workflow = CLEAN.replace('echo checked', 'twine upload dist/*')
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp) / 'repo'
            fixture(root, workflow)
            stage(root)
            for mode, code in [('advisory', 0), ('strict', 1)]:
                output = Path(temp) / mode
                environment = os.environ.copy()
                environment.pop('GITHUB_OUTPUT', None)
                result = subprocess.run([sys.executable, str(Path(checks.__file__).resolve()), '--repo', str(root),
                                         '--tools-dir', str(self.tools_dir), '--output', str(output), '--mode', mode],
                                        capture_output=True, env=environment)
                self.assertEqual(result.returncode, code, result.stdout)
                report = json.loads((output / 'summary.json').read_text())
                finding = report['checks']['zizmor']['findings'][0]
                self.assertEqual(finding['severity'], 'Informational')
                self.assertEqual(report['checks']['zizmor']['exit_code'], 11)


if __name__ == '__main__':
    unittest.main()
