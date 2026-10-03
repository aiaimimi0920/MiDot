"""Scan only, using verified tools and an explicit tracked-file snapshot."""

import argparse
import hashlib
import io
import json
from pathlib import Path, PurePosixPath
import platform
import re
import subprocess
import tarfile
import tempfile
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[2]
LOCK = Path(__file__).with_name('tool-lock.json')
SEVERITIES = ('Informational', 'Low', 'Medium', 'High')
CONFIDENCES = ('Low', 'Medium', 'High')


class CheckError(Exception):
    pass


def digest(data):
    return hashlib.sha256(data).hexdigest()


def checked_process(command, cwd):
    try:
        return subprocess.run(command, cwd=cwd, capture_output=True, timeout=180)
    except (OSError, subprocess.TimeoutExpired) as exc:
        raise CheckError('Tool could not execute or timed out') from exc


def git(repo, *args):
    result = checked_process(['git', '-C', str(repo), *args], repo)
    if result.returncode:
        raise CheckError('Git input discovery failed')
    return result.stdout


def ensure_tools(folder, lock_path=LOCK):
    lock = json.loads(lock_path.read_text(encoding='utf-8'))
    system = platform.system().lower()
    if system not in ('linux', 'windows') or platform.machine().lower() not in ('amd64', 'x86_64'):
        raise CheckError('Only Linux/Windows x86_64 scanner builds are pinned')
    tools = {}
    for name in ('gitleaks', 'zizmor'):
        spec = lock['tools'][name]
        build = spec['platforms'][system]
        path = folder / name / spec['version'] / build['member']
        if not path.exists():
            request = urllib.request.Request(build['url'], headers={'User-Agent': 'MiDot-security-check'})
            with urllib.request.urlopen(request, timeout=90) as response:
                raw = response.read()
            if digest(raw) != build['archive_sha256']:
                raise CheckError(f'{name} archive SHA-256 mismatch')
            if build['url'].endswith('.zip'):
                with zipfile.ZipFile(io.BytesIO(raw)) as archive:
                    members = [m for m in archive.namelist() if PurePosixPath(m).name == build['member']]
                    if len(members) != 1:
                        raise CheckError('Missing or ambiguous executable in archive')
                    binary = archive.read(members[0])
            else:
                with tarfile.open(fileobj=io.BytesIO(raw), mode='r:gz') as archive:
                    members = [m for m in archive if m.isfile() and PurePosixPath(m.name).name == build['member']]
                    if len(members) != 1:
                        raise CheckError('Missing or ambiguous executable in archive')
                    binary = archive.extractfile(members[0]).read()
            if digest(binary) != build['binary_sha256']:
                raise CheckError(f'{name} executable SHA-256 mismatch')
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(binary)
            if system == 'linux':
                path.chmod(0o700)
        if path.is_symlink() or digest(path.read_bytes()) != build['binary_sha256']:
            raise CheckError(f'{name} cached executable SHA-256 mismatch')
        tools[name] = path.resolve()
    return tools


def snapshot(repo, destination):
    names = git(repo, 'ls-files', '--cached', '-z').decode('utf-8').split('\0')
    names = sorted(set(n for n in names if n))
    if not names:
        raise CheckError('No tracked files to scan')
    files, skipped, workflows = [], [], []
    for name in names:
        relative = PurePosixPath(name)
        if relative.is_absolute() or '..' in relative.parts or '\\' in name:
            raise CheckError('Invalid tracked path')
        if relative.name == '.gitleaksignore':
            raise CheckError('Tracked .gitleaksignore is not permitted to suppress findings')
        action_input = ((name.startswith('.github/workflows/') and relative.suffix in ('.yml', '.yaml'))
                        or relative.name in ('action.yml', 'action.yaml')
                        or name in ('.github/dependabot.yml', '.github/dependabot.yaml'))
        path = repo / name
        if path.is_symlink() or not path.is_file() or not path.resolve().is_relative_to(repo):
            raise CheckError('Tracked input is missing or is a symlink')
        data = path.read_bytes()
        try:
            data.decode('utf-8-sig')
            if b'\0' in data:
                raise UnicodeError()
        except UnicodeError:
            if action_input:
                raise CheckError('Actions definition is binary or non-UTF-8')
            skipped.append({'path': name, 'reason': 'binary or non-UTF-8'})
            continue
        target = destination / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
        files.append({'path': name, 'sha256': digest(data)})
        if action_input:
            workflows.append(name)
    if not files or not any(n.startswith('.github/workflows/') for n in workflows):
        raise CheckError('No tracked text or workflow inputs to scan')
    return {'head': git(repo, 'rev-parse', 'HEAD').decode().strip(),
            'files': files, 'skipped': skipped, 'actions_inputs': workflows}


def safe_path(value, directory=None):
    if not isinstance(value, str) or not value or any(ord(c) < 32 for c in value):
        raise CheckError('Scanner returned an invalid finding path')
    value = value.replace('\\', '/')
    if directory is not None:
        prefix = directory.resolve().as_posix() + '/'
        if value.startswith(prefix):
            value = value[len(prefix):]
    relative = PurePosixPath(value)
    if relative.is_absolute() or '..' in relative.parts or ':' in value:
        raise CheckError('Scanner returned a finding outside the tracked snapshot')
    if directory is not None and not (directory / relative).is_file():
        raise CheckError('Scanner returned a finding outside the tracked snapshot')
    return relative.as_posix()


def position(value, minimum=0):
    if type(value) is not int or value < minimum:
        raise CheckError('Scanner returned an invalid finding position')
    return value


def parse_findings(tool, result, raw, directory=None):
    try:
        findings = json.loads(raw)
    except (ValueError, UnicodeError) as exc:
        raise CheckError(f'{tool} returned missing or invalid JSON') from exc
    if not isinstance(findings, list) or any(not isinstance(f, dict) for f in findings):
        raise CheckError(f'{tool} returned an invalid finding schema')
    allowed = (0, 1) if tool == 'gitleaks' else (0, 11, 12, 13, 14)
    if result.returncode not in allowed or (result.returncode != 0 and not findings):
        raise CheckError(f'{tool} failed (exit {result.returncode})')
    safe = []
    for item in findings:
        key = 'RuleID' if tool == 'gitleaks' else 'ident'
        if not isinstance(item.get(key), str) or not re.fullmatch(r'[a-zA-Z0-9_.-]+', item[key]):
            raise CheckError(f'{tool} returned an invalid finding identifier')
        if tool == 'gitleaks':
            line = position(item.get('StartLine'), 1)
            safe.append({'rule': item[key], 'file': safe_path(item.get('File'), directory),
                         'line': line, 'end_line': position(item.get('EndLine', line), line),
                         'column': position(item.get('StartColumn', 0)),
                         'end_column': position(item.get('EndColumn', 0))})
        else:
            if (not isinstance(item.get('locations'), list) or not item['locations']
                    or not isinstance(item.get('determinations'), dict)):
                raise CheckError('zizmor returned an invalid finding location')
            determinations = item['determinations']
            if determinations.get('severity') not in SEVERITIES or determinations.get('confidence') not in CONFIDENCES:
                raise CheckError('zizmor returned an invalid finding determination')
            locations = []
            for location in item['locations']:
                try:
                    symbolic = location['symbolic']
                    path = safe_path(symbolic['key']['Local']['verbatim_path'], directory)
                    bounds = location['concrete']['location']
                    start, end = bounds['start_point'], bounds['end_point']
                    locations.append({'file': path, 'line': position(start['row']) + 1,
                                      'column': position(start['column']) + 1,
                                      'end_line': position(end['row']) + 1,
                                      'end_column': position(end['column']) + 1})
                except (KeyError, TypeError) as exc:
                    raise CheckError('zizmor returned an invalid finding location') from exc
            # Never retain source features, annotations, comments, matches or secrets.
            safe.append({'rule': item[key], 'severity': determinations['severity'],
                         'confidence': determinations['confidence'], 'locations': locations})
    return safe


def scan_snapshot(directory, inputs, tools):
    result = {}
    with tempfile.TemporaryDirectory(prefix='midot-reports-') as temp:
        temp = Path(temp)
        config = temp / 'gitleaks.toml'
        config.write_text('[extend]\nuseDefault = true\n', encoding='utf-8')
        ignore = temp / 'empty.ignore'
        ignore.write_text('', encoding='utf-8')
        report = temp / 'gitleaks.json'
        commands = {
            'gitleaks': [str(tools['gitleaks']), 'dir', str(directory), '--config', str(config),
                         '--gitleaks-ignore-path', str(ignore), '--ignore-gitleaks-allow',
                         '--redact=100', '--no-banner', '--report-format=json', '--report-path', str(report)],
            'zizmor': [str(tools['zizmor']), '--offline', '--strict-collection', '--no-config', '--no-ignores',
                       '--format=json-v1', *inputs],
        }
        for tool, command in commands.items():
            try:
                process = checked_process(command, directory)
                raw = report.read_bytes() if tool == 'gitleaks' else process.stdout
                findings = parse_findings(tool, process, raw, directory)
                result[tool] = {'status': 'findings' if findings else 'pass', 'exit_code': process.returncode,
                                'findings': findings}
            except (CheckError, OSError) as exc:
                message = str(exc) if isinstance(exc, CheckError) else f'{tool} produced no readable JSON report'
                result[tool] = {'status': 'error', 'error': message}
    return result


def run(repo, tools_dir, output, mode='advisory'):
    repo = repo.resolve()
    output.mkdir(parents=True, exist_ok=True)
    summary = {'schema_version': 1, 'status': 'error', 'mode': mode,
               'coverage': 'tracked UTF-8 inputs using Gitleaks built-in rules and offline Actions audit',
               'limitations': ['Gitleaks built-in allowlists can exclude text paths/tokens (including SVG, GLTF and lock files)',
                               'Binary/non-UTF-8 inputs, bundle contents and Git history are not secret-scanned',
                               'Godot/third-party C++ vulnerabilities and online Actions advisories are not audited']}
    try:
        if mode not in ('advisory', 'strict'):
            raise CheckError('Unknown security policy mode')
        tools = ensure_tools(tools_dir)
        with tempfile.TemporaryDirectory(prefix='midot-snapshot-') as temp:
            directory = Path(temp)
            summary['source'] = snapshot(repo, directory)
            summary['tools'] = json.loads(LOCK.read_text(encoding='utf-8'))['tools']
            summary['checks'] = scan_snapshot(directory, summary['source']['actions_inputs'], tools)
        summary['finding_count'] = sum(len(c.get('findings', [])) for c in summary['checks'].values())
        if any(c['status'] == 'error' for c in summary['checks'].values()):
            summary['status'] = 'error'
        elif summary['finding_count']:
            summary['status'] = 'advisory' if mode == 'advisory' else 'fail'
        else:
            summary['status'] = 'pass'
    except (CheckError, OSError, ValueError, KeyError) as exc:
        # Never echo scanner stdout/stderr, source contents, or network exceptions.
        summary['error'] = str(exc) if isinstance(exc, CheckError) else 'Preparation failed; no successful scan'
    (output / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n', encoding='utf-8')
    return summary


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, default=ROOT)
    parser.add_argument('--tools-dir', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--mode', choices=('advisory', 'strict'), default='advisory')
    args = parser.parse_args()
    summary = run(args.repo, args.tools_dir, args.output, args.mode)
    print(f"Security checks: {summary['status']}; sanitized summary.json written")
    if summary['status'] == 'advisory':
        print(f"::warning::{summary['finding_count']} valid security findings; see the sanitized report artifact")
    return 0 if summary['status'] in ('pass', 'advisory') else 1


if __name__ == '__main__':
    raise SystemExit(main())
