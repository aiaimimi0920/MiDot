"""Issue policy boundaries; mocked API only, no external writes or credentials."""
import contextlib
import copy
import io
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import Mock, patch
import urllib.error

import track_issue as tracker

HEAD = '1' * 40
FINDING = {'rule': 'synthetic', 'file': 'patch.diff', 'line': 2, 'end_line': 2,
           'column': 0, 'end_column': 0}


def report(found=True):
    return {'schema_version': 1, 'mode': 'advisory', 'status': 'advisory' if found else 'pass',
            'source': {'head': HEAD}, 'finding_count': 1 if found else 0,
            'checks': {'gitleaks': {'status': 'findings' if found else 'pass', 'exit_code': 1 if found else 0,
                                    'findings': [copy.deepcopy(FINDING)] if found else []},
                       'zizmor': {'status': 'pass', 'exit_code': 0, 'findings': []}}}


def managed_issue():
    return {'title': tracker.TITLE, 'body': tracker.MARKER + '\nold report',
            'number': 7, 'user': {'login': 'github-actions[bot]'}}


class IssueTests(unittest.TestCase):
    def synchronize(self, api, value):
        return tracker.synchronize(api, 'fixture/repository', value, HEAD, '123', '456')

    def test_creates_one_separate_issue_with_complete_artifact_link(self):
        api = Mock(side_effect=[{'object': {'sha': HEAD}}, [], {'object': {'sha': HEAD}}, {'number': 7}])
        self.assertEqual(self.synchronize(api, report()), 'created issue #7')
        method, _, payload = api.call_args.args
        self.assertEqual(method, 'POST')
        self.assertIn('/actions/runs/123/artifacts/456', payload['body'])
        self.assertIn('gitleaks:synthetic', payload['body'])
        self.assertIn(tracker.MARKER, payload['body'])

    def test_existing_bot_issue_is_updated_instead_of_duplicated(self):
        api = Mock(side_effect=[{'object': {'sha': HEAD}}, [managed_issue()], {'object': {'sha': HEAD}}, {'number': 7}])
        self.synchronize(api, report())
        self.assertEqual(api.call_args.args[0], 'PATCH')
        self.assertEqual(api.call_args.args[2]['state'], 'open')

    def test_clean_snapshot_closes_existing_tracker(self):
        api = Mock(side_effect=[{'object': {'sha': HEAD}}, [managed_issue()], {'object': {'sha': HEAD}}, {'number': 7}])
        self.synchronize(api, report(False))
        self.assertEqual(api.call_args.args[2]['state'], 'closed')

    def test_clean_snapshot_does_not_create_empty_issue(self):
        api = Mock(side_effect=[{'object': {'sha': HEAD}}, []])
        self.assertEqual(self.synchronize(api, report(False)), 'no findings; no issue needed')
        self.assertEqual(api.call_count, 2)

    def test_manual_or_spoofed_issue_is_not_taken_over(self):
        item = managed_issue()
        item['user']['login'] = 'someone-else'
        api = Mock(side_effect=[{'object': {'sha': HEAD}}, [item], {'object': {'sha': HEAD}}, {'number': 8}])
        self.synchronize(api, report())
        self.assertEqual(api.call_args.args[0], 'POST')

    def test_error_or_mismatched_report_never_contacts_api(self):
        for mutation in ('tool', 'head', 'count'):
            value = report()
            if mutation == 'tool': value['checks']['gitleaks']['status'] = 'error'
            if mutation == 'head': value['source']['head'] = '0' * 40
            if mutation == 'count': value['finding_count'] = 0
            api = Mock()
            with self.subTest(mutation=mutation), self.assertRaises(tracker.TrackingError):
                self.synchronize(api, value)
            api.assert_not_called()

    def test_unsanitized_or_incomplete_finding_is_rejected(self):
        for change in ('secret', 'location', 'exit'):
            value = report()
            if change == 'secret': value['checks']['gitleaks']['findings'][0]['Secret'] = 'must-never-appear'
            if change == 'location': value['checks']['gitleaks']['findings'][0]['file'] = '../escape'
            if change == 'exit': value['checks']['gitleaks']['exit_code'] = 2
            api = Mock()
            with self.subTest(change=change), self.assertRaises(tracker.TrackingError):
                self.synchronize(api, value)
            api.assert_not_called()

    def test_pr_context_never_reads_or_writes_issues(self):
        with patch.dict(os.environ, {'GITHUB_REF': 'refs/heads/main', 'GITHUB_EVENT_NAME': 'pull_request'}, clear=True), \
                patch.object(sys, 'argv', ['tracker', '--report', 'unused', '--artifact-id', '456']), \
                patch('urllib.request.urlopen') as network, contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(tracker.main(), 1)
        network.assert_not_called()

    def test_stale_clean_or_findings_run_never_changes_latest_issue(self):
        for found in (False, True):
            api = Mock(return_value={'object': {'sha': '2' * 40}})
            self.assertIn('skipped stale', self.synchronize(api, report(found)))
            self.assertEqual(api.call_count, 1)
            self.assertEqual(api.call_args.args[0], 'GET')

    def test_main_moving_during_lookup_never_changes_issue(self):
        api = Mock(side_effect=[{'object': {'sha': HEAD}}, [managed_issue()], {'object': {'sha': '2' * 40}}])
        self.assertIn('skipped moved', self.synchronize(api, report(False)))
        self.assertTrue(all(call.args[0] == 'GET' for call in api.call_args_list))

    def test_older_same_revision_workflow_never_overwrites_newer_report(self):
        item = managed_issue()
        item['body'] += '\n' + tracker.STATE + json.dumps({'head': HEAD, 'run_number': 2, 'run_attempt': 1}) + ' -->'
        api = Mock(side_effect=[{'object': {'sha': HEAD}}, [item]])
        self.assertIn('skipped older', self.synchronize(api, report(False)))
        self.assertTrue(all(call.args[0] == 'GET' for call in api.call_args_list))

    def test_api_failure_is_nonzero_without_token_or_response_details(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / 'report.json'
            path.write_text(json.dumps(report()))
            env = {'GITHUB_REF': 'refs/heads/main', 'GITHUB_EVENT_NAME': 'push',
                   'GITHUB_REPOSITORY': 'fixture/repository', 'GITHUB_SHA': HEAD,
                   'GITHUB_RUN_ID': '123', 'GITHUB_RUN_NUMBER': '1', 'GITHUB_RUN_ATTEMPT': '1',
                   'GITHUB_TOKEN': 'never-print-this-token'}
            stdout = io.StringIO()
            with patch.dict(os.environ, env, clear=True), \
                    patch.object(sys, 'argv', ['tracker', '--report', str(path), '--artifact-id', '456']), \
                    patch('urllib.request.urlopen', side_effect=urllib.error.URLError('private-response-details')), \
                    contextlib.redirect_stdout(stdout):
                self.assertEqual(tracker.main(), 1)
            self.assertNotIn('never-print-this-token', stdout.getvalue())
            self.assertNotIn('private-response-details', stdout.getvalue())


if __name__ == '__main__':
    unittest.main()
