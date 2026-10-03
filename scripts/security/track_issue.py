"""Maintain one separate advisory issue from a valid, sanitized main-branch report."""
import argparse
from collections import Counter
import json
import os
import re
import urllib.error
import urllib.request

import run_checks as checks

MARKER = '<!-- repository-security-advisory:v1 -->'
TITLE = 'Security advisory: tracked scanner findings'
STATE = '<!-- repository-security-advisory-state:v1 '


class TrackingError(Exception):
    pass


def counts(report, expected_head):
    source = report.get('source') if isinstance(report, dict) else None
    if (not isinstance(report, dict) or report.get('schema_version') != 1
            or report.get('mode') != 'advisory' or report.get('status') not in ('pass', 'advisory')
            or not isinstance(source, dict) or source.get('head') != expected_head):
        raise TrackingError('Report is invalid, unsuccessful, or from a different revision')
    audited = report.get('checks')
    if not isinstance(audited, dict) or set(audited) != {'gitleaks', 'zizmor'}:
        raise TrackingError('Both scanner results are required')
    result = Counter()
    for tool, check in audited.items():
        if not isinstance(check, dict) or check.get('status') not in ('pass', 'findings'):
            raise TrackingError('Tool errors cannot be treated as advisory findings')
        findings = check.get('findings')
        allowed = (0, 1) if tool == 'gitleaks' else (0, 11, 12, 13, 14)
        if (not isinstance(findings, list) or type(check.get('exit_code')) is not int
                or check['exit_code'] not in allowed
                or (check['exit_code'] != 0 and not findings)
                or check['status'] != ('findings' if findings else 'pass')):
            raise TrackingError('Tool result or finding schema is invalid')
        for finding in findings:
            if not isinstance(finding, dict) or not isinstance(finding.get('rule'), str) or not re.fullmatch(r'[a-zA-Z0-9_.-]+', finding['rule']):
                raise TrackingError('Finding identifier is invalid')
            keys = ({'rule', 'file', 'line', 'end_line', 'column', 'end_column'} if tool == 'gitleaks'
                    else {'rule', 'severity', 'confidence', 'locations'})
            if set(finding) != keys:
                raise TrackingError('Finding is incomplete or contains unsanitized fields')
            try:
                if tool == 'gitleaks':
                    checks.safe_path(finding['file'])
                    checks.position(finding['line'], 1)
                    checks.position(finding['end_line'], finding['line'])
                    checks.position(finding['column'])
                    checks.position(finding['end_column'])
                else:
                    if (finding['severity'] not in checks.SEVERITIES
                            or finding['confidence'] not in checks.CONFIDENCES
                            or not isinstance(finding['locations'], list) or not finding['locations']):
                        raise TrackingError('Finding determination or locations are invalid')
                    for location in finding['locations']:
                        if not isinstance(location, dict) or set(location) != {'file', 'line', 'column', 'end_line', 'end_column'}:
                            raise TrackingError('Finding location is incomplete or unsanitized')
                        checks.safe_path(location['file'])
                        for name in ('line', 'column', 'end_line', 'end_column'):
                            checks.position(location[name], 1)
            except checks.CheckError as exc:
                raise TrackingError('Finding location is invalid') from exc
            result[f'{tool}:{finding["rule"]}'] += 1
    total = sum(result.values())
    if (type(report.get('finding_count')) is not int or report['finding_count'] != total
            or report['status'] != ('advisory' if total else 'pass')):
        raise TrackingError('Report counts or status are inconsistent')
    return result


def current_main(api, repository):
    value = api('GET', f'/repos/{repository}/git/ref/heads/main')
    try:
        head = value['object']['sha']
    except (KeyError, TypeError) as exc:
        raise TrackingError('GitHub returned an invalid main reference') from exc
    if not isinstance(head, str) or not re.fullmatch(r'[0-9a-f]{40}', head):
        raise TrackingError('GitHub returned an invalid main reference')
    return head


def synchronize(api, repository, report, head, run_id, artifact_id, run_number=1, run_attempt=1):
    found = counts(report, head)
    if current_main(api, repository) != head:
        return 'skipped stale main revision; no issue changed'
    issue = None
    for page in range(1, 51):
        items = api('GET', f'/repos/{repository}/issues?state=all&per_page=100&page={page}')
        if not isinstance(items, list):
            raise TrackingError('GitHub returned an invalid issue collection')
        for item in items:
            if (not item.get('pull_request') and item.get('title') == TITLE
                    and (item.get('body') or '').startswith(MARKER)
                    and item.get('user', {}).get('login') == 'github-actions[bot]'):
                issue = item
                break
        if issue or len(items) < 100:
            break
    else:
        raise TrackingError('Issue lookup exceeded its bounded pagination')
    if not found and issue is None:
        return 'no findings; no issue needed'
    if issue:
        for line in (issue.get('body') or '').splitlines():
            if line.startswith(STATE):
                try:
                    stored = json.loads(line[len(STATE):-4])
                    previous = (stored['run_number'], stored['run_attempt'])
                    if any(type(n) is not int or n < 1 for n in previous):
                        raise ValueError()
                except (ValueError, KeyError, TypeError) as exc:
                    raise TrackingError('Managed issue state is invalid') from exc
                if previous > (run_number, run_attempt):
                    return 'skipped older workflow run; no issue changed'
    if current_main(api, repository) != head:
        return 'skipped moved main revision; no issue changed'
    run_url = f'https://github.com/{repository}/actions/runs/{run_id}'
    artifact_url = run_url + f'/artifacts/{artifact_id}'
    state = {'head': head, 'run_number': run_number, 'run_attempt': run_attempt}
    body = (MARKER + '\n' + STATE + json.dumps(state) + ' -->\n\n'
            'Valid scanner findings are tracked separately from functional PR checks. '
            'Tool, input, report, tracking and artifact failures remain errors.\n\n'
            f'Revision: `{head}`. [Workflow run]({run_url}); '
            f'[complete sanitized report]({artifact_url}) (`security-report`).\n\n'
            f'Current finding count: {sum(found.values())}.\n\n')
    body += '\n'.join(f'- `{rule}`: {count}' for rule, count in sorted(found.items()))
    body += ('\n\nNo source excerpts, matches, credentials or scanner annotations are copied into this issue. '
             'A clean tracked snapshot does not audit Git history, binary contents, or complete C++ dependencies.\n')
    if issue:
        result = api('PATCH', f'/repos/{repository}/issues/{issue["number"]}',
                     {'body': body, 'state': 'open' if found else 'closed'})
        return f'updated issue #{result["number"]}'
    result = api('POST', f'/repos/{repository}/issues', {'title': TITLE, 'body': body})
    return f'created issue #{result["number"]}'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--report', required=True)
    parser.add_argument('--artifact-id', required=True)
    args = parser.parse_args()
    try:
        if os.environ.get('GITHUB_REF') != 'refs/heads/main' or os.environ.get('GITHUB_EVENT_NAME') not in ('push', 'schedule', 'workflow_dispatch'):
            raise TrackingError('Issue tracking is restricted to trusted main-branch runs')
        repository = os.environ.get('GITHUB_REPOSITORY', '')
        head = os.environ.get('GITHUB_SHA', '')
        run_id = os.environ.get('GITHUB_RUN_ID', '')
        run_number = os.environ.get('GITHUB_RUN_NUMBER', '')
        run_attempt = os.environ.get('GITHUB_RUN_ATTEMPT', '')
        token = os.environ.get('GITHUB_TOKEN', '')
        if (not re.fullmatch(r'[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+', repository)
                or not re.fullmatch(r'[0-9a-f]{40}', head) or not run_id.isdigit()
                or not args.artifact_id.isdigit() or not token
                or not run_number.isdigit() or int(run_number) < 1
                or not run_attempt.isdigit() or int(run_attempt) < 1):
            raise TrackingError('Trusted tracking context is incomplete')
        def api(method, path, payload=None):
            request = urllib.request.Request('https://api.github.com' + path,
                data=json.dumps(payload).encode() if payload is not None else None,
                headers={'Authorization': 'Bearer ' + token, 'Accept': 'application/vnd.github+json',
                         'Content-Type': 'application/json', 'X-GitHub-Api-Version': '2022-11-28'}, method=method)
            try:
                with urllib.request.urlopen(request, timeout=30) as response:
                    return json.load(response)
            except (urllib.error.URLError, ValueError) as exc:
                raise TrackingError('GitHub issue request failed') from exc
        with open(args.report, encoding='utf-8') as stream:
            report = json.load(stream)
        print('Security issue tracker: ' + synchronize(api, repository, report, head, run_id, args.artifact_id,
                                                      int(run_number), int(run_attempt)))
        return 0
    except (TrackingError, OSError, ValueError, KeyError, TypeError):
        print('Security issue tracker failed: invalid context/report or API failure')
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
