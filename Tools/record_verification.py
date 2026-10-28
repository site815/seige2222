"""Record a release gate from real reports, rejecting absent or mismatched evidence.

python Tools/record_verification.py --version 0.9.3 --native Saved/Automation/v093-full \
    --package-log Saved/package-v093.log --config-log Saved/validate-config-v093.log \
    --rules-log Saved/validate-rules-v093.log

Only standard-library modules are required. Historical reports are never upgraded
to the new gate: rerun verify_packaged.ps1 to capture binary/data/source identities.
"""
import argparse
import datetime
import hashlib
import itertools
import json
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RUNTIME_DIRS = ('Rules', 'AIFILES', 'Interface', 'Graphics')
SOURCE_DIRS = ('Source', 'Config', 'Content', *RUNTIME_DIRS)
SOURCE_EXTENSIONS = {'.cpp', '.h', '.cs', '.ini', '.uasset', '.umap', '.json'}
TEST_PATTERN = re.compile(r'IMPLEMENT_SIMPLE_AUTOMATION_TEST\s*\(\s*\w+\s*,\s*(?:TEXT\(\s*)?"(Seige\.[^"]+)"')
COMPLEX_TEST_PATTERN = re.compile(r'IMPLEMENT_COMPLEX_AUTOMATION_TEST\s*\(\s*\w+\s*,\s*(?:TEXT\(\s*)?"(Seige\.[^"]+)"')


def file_record(path, root=ROOT):
    path = Path(path)
    try:
        display = path.resolve().relative_to(root.resolve()).as_posix()
    except ValueError:
        display = str(path)
    if not path.is_file():
        return {'path': display, 'missing': True}
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(chunk)
    return {'path': display, 'bytes': path.stat().st_size, 'sha256': digest.hexdigest().upper()}


def read_json(path):
    return json.loads(Path(path).read_text(encoding='utf-8-sig'))


def integer(value):
    return type(value) is int and value >= 0


def source_paths(root):
    files = [p for name in SOURCE_DIRS for p in (root / name).rglob('*')
             if p.is_file() and p.suffix.lower() in SOURCE_EXTENSIONS]
    files.append(root / 'seige2222.uproject')
    return sorted(files, key=lambda p: p.relative_to(root).as_posix())


def manifest(paths, root):
    records = [file_record(p, root) for p in paths]
    encoded = json.dumps(records, sort_keys=True, separators=(',', ':')).encode('utf-8')
    return {'file_count': len(records), 'sha256': hashlib.sha256(encoded).hexdigest().upper(), 'files': records}


def expected_native_tests(root):
    names = set()
    for path in (root / 'Source').rglob('*.cpp'):
        text = path.read_text(encoding='utf-8-sig')
        names.update(TEST_PATTERN.findall(text))
        for group in COMPLEX_TEST_PATTERN.findall(text):
            # This suite enumerates one missing standard type and every rare pair.
            # Read its authored arrays, not a stale hardcoded test count. New kinds
            # of parameterized suites need an explicit expansion before release.
            if group != 'Seige.Economy.StarterCombinations':
                raise ValueError(f'Unknown parameterized suite: {group}')
            arrays = []
            for array_name in ('StandardTypes', 'RareTypes'):
                declaration = re.search(rf'\b{array_name}\s*=\s*\{{([^}}]+)\}}', text)
                values = re.findall(r'TEXT\("([^"]+)"\)', declaration.group(1)) if declaration else []
                if len(values) < 2 or len(set(values)) != len(values):
                    raise ValueError(f'Cannot enumerate {group}: {array_name}')
                arrays.append(values)
            for missing in arrays[0]:
                for a, b in itertools.combinations(arrays[1], 2):
                    names.add(f'{group}.{missing}__{a}_{b}')
    return sorted(names)


def native_result(report, expected):
    issues = []
    tests = report.get('tests')
    if not isinstance(tests, list) or not tests:
        tests = []
        issues.append('Native report has no tests')
    if not all(isinstance(t, dict) for t in tests):
        issues.append('Malformed native test entry')
        tests = [t for t in tests if isinstance(t, dict)]
    names = [t.get('fullTestPath') for t in tests]
    valid_names = [name for name in names if isinstance(name, str)]
    if len(valid_names) != len(names) or len(set(valid_names)) != len(names):
        issues.append('Native test names are missing or duplicated')
    missing = sorted(set(expected) - set(valid_names))
    unexpected = sorted(set(valid_names) - set(expected))
    if not expected or missing or unexpected:
        issues.append('Report does not cover the current complete Seige suite')
    counters = ('succeeded', 'succeededWithWarnings', 'failed', 'notRun', 'inProcess')
    if any(not integer(report.get(k)) for k in counters):
        issues.append('Missing or invalid native counters')
    elif (report['succeeded'] + report['succeededWithWarnings'] != len(tests)
          or any(report[k] != 0 for k in ('failed', 'notRun', 'inProcess'))):
        issues.append('Native counters are incomplete or inconsistent')
    failures = [t for t in tests if t.get('state') != 'Success' or t.get('errors') != 0
                or any(e.get('event', {}).get('type') == 'Error' for e in t.get('entries', []))]
    if failures:
        issues.append('Native test failures or errors remain')
    states = {}
    for test in tests:
        states[test.get('state', '?')] = states.get(test.get('state', '?'), 0) + 1
    return {
        'status': 'single_full_run' if not issues else 'failed', 'issues': issues,
        'report_created_on': report.get('reportCreatedOn'), 'unique_tests': len(tests),
        'expected_tests': len(expected), 'missing_tests': missing, 'unexpected_tests': unexpected,
        'succeeded': report.get('succeeded'), 'succeeded_with_warnings': report.get('succeededWithWarnings'),
        'failed': report.get('failed'), 'not_run': report.get('notRun'), 'in_process': report.get('inProcess'),
        'duration_seconds': report.get('totalDuration'), 'states': states,
        'failures': [{'full_test_path': t.get('fullTestPath'), 'state': t.get('state'), 'errors': t.get('errors'),
                      'messages': [e.get('event', {}).get('message') for e in t.get('entries', [])
                                   if e.get('event', {}).get('type') in ('Error', 'Warning')][:6]} for t in failures],
        'warnings_in_passing_tests': [t.get('fullTestPath') for t in tests
                                     if t.get('state') == 'Success' and (t.get('warnings', 0) or 0) > 0],
    }


def validator_result(text, label, rules_version):
    # Arbitrary nonempty text (for example a Node module-not-found error) is not a pass.
    heading = 'Configuration' if label == 'configuration' else 'Rules'
    patterns = [rf'^{heading} valid: {re.escape(rules_version)}; [1-9]\d* items, [1-9]\d* recipes, [1-9]\d* buildings\.$']
    if label == 'configuration':
        patterns += [r'^AI: .+ Interface: .+\.$', r'^Graphics: [1-9]\d* nature roles resolve to Content assets; .+\.$']
    else:
        patterns += [r'^Renewable paths verified for [1-9]\d* items; startup building/worker affordability verified\.$',
                     r'^[1-9]\d* negative validation cases passed\.$']
    passed = all(re.search(p, text, re.MULTILINE) for p in patterns)
    passed = passed and not re.search(r'\b(?:FAILED|ERROR|EXCEPTION|TRACEBACK)\b', text, re.IGNORECASE)
    return {'status': 'passed' if passed else 'failed', 'tail': text.strip().splitlines()[-3:]}


def indexed_files(records):
    if not isinstance(records, list) or not records:
        return None
    result = {}
    for row in records:
        if (not isinstance(row, dict) or not isinstance(row.get('path'), str)
                or not integer(row.get('bytes')) or not isinstance(row.get('sha256'), str)
                or not re.fullmatch(r'[0-9a-fA-F]{64}', row['sha256']) or row['path'] in result):
            return None
        result[row['path']] = (row['bytes'], row['sha256'].upper())
    return result


def packaged_result(data, version, package_inputs, source_inputs, runtime_files):
    issues = []
    rep = data.get('report') if isinstance(data.get('report'), dict) else {}
    if (data.get('schema_version') != 2 or data.get('version') != version or data.get('mode') != 'UiSmoke'
            or type(data.get('exit_code')) is not int or data['exit_code'] != 0):
        issues.append('Missing or unsuccessful current-version UiSmoke record')
    if (rep.get('ready') is not True or type(rep.get('completed_stages')) is not int or rep['completed_stages'] != 117
            or type(rep.get('failures')) is not int or rep['failures'] != 0 or rep.get('assertion_failures') != []):
        issues.append('Packaged gameplay route is incomplete or failed')
    if rep.get('main_test_speed') != 10 or rep.get('walking_probe_speed') != 1:
        issues.append('Packaged gameplay did not use the required 10x route and 1x movement probe')
    if any(rep.get(key) is not True for key in ('deployment_carry_observed', 'deployment_work_observed', 'paid_defense_tower_completed')):
        issues.append('Fresh-colony deployment or paid defense construction was not observed')
    if type(rep.get('transport_segments')) is not int or rep['transport_segments'] != 0:
        issues.append('Current roadless playthrough unexpectedly created roads')
    samples = data.get('socket_samples')
    if not isinstance(samples, list) or not samples:
        samples = []
        issues.append('No live network endpoint observations')
    bad_samples = [s for s in samples if not isinstance(s, dict) or not integer(s.get('tcp'))
                   or not integer(s.get('udp')) or not isinstance(s.get('process_ids'), list) or not s['process_ids']
                   or any(not integer(pid) or pid == 0 for pid in s['process_ids'])]
    endpoints = sum(1 for s in samples if isinstance(s, dict) and
                    ((integer(s.get('tcp')) and s['tcp'] > 0) or (integer(s.get('udp')) and s['udp'] > 0)))
    if bad_samples or endpoints:
        issues.append('Network observation failed or game opened an endpoint')
    for key, current in (('packaged_inputs', package_inputs), ('source_inputs', source_inputs)):
        expected = indexed_files(current)
        if expected is None or indexed_files(data.get(key)) != expected or data.get('inputs_unchanged') is not True:
            issues.append(f'{key} differs from the build exercised by the route')
    # Each loose JSON must have the same source/staged bytes, at run time and now.
    previous_runtime = data.get('runtime_files')
    if (not runtime_files or not isinstance(previous_runtime, list)
            or sorted(previous_runtime, key=lambda r: r.get('path', '')) != runtime_files
            or any(not row.get('match') for row in runtime_files)):
        issues.append('Authoritative runtime JSON differs between source, package or route')
    return {
        'status': 'passed' if not issues else 'failed', 'issues': issues,
        'exit_code': data.get('exit_code'), 'completed_stages': rep.get('completed_stages'), 'failures': rep.get('failures'),
        'socket_samples': len(samples), 'samples_with_endpoints': endpoints, 'invalid_socket_samples': len(bad_samples),
        'inputs_unchanged': data.get('inputs_unchanged'),
    }


def runtime_comparison(root, binary_dir):
    # Enumerate both sides so a stale extra JSON cannot escape the comparison.
    source = {p.relative_to(root).as_posix(): p for name in RUNTIME_DIRS for p in (root / name).rglob('*.json')}
    staged = {p.relative_to(binary_dir).as_posix(): p for name in RUNTIME_DIRS for p in (binary_dir / name).rglob('*.json')}
    rows = []
    for name in sorted(source.keys() | staged.keys()):
        src = file_record(source[name], root).get('sha256') if name in source else None
        dst = file_record(staged[name], root).get('sha256') if name in staged else None
        rows.append({'path': name, 'source_sha256': src, 'staged_sha256': dst, 'match': bool(src and dst and src == dst)})
    return rows


def package_paths(root, version):
    package_dir = root / f'Builds/v{version}/Windows/seige2222'
    executable = package_dir / 'Binaries/Win64/Seige-Win64-Shipping.exe'
    paks = sorted(p for p in (package_dir / 'Content/Paks').glob('*') if p.suffix.lower() in {'.pak', '.ucas', '.utoc'})
    return executable, [executable, *paks]


def git_state(root):
    def git(*args):
        result = subprocess.run(['git', *args], cwd=root, capture_output=True, text=True, timeout=60)
        if result.returncode:
            raise RuntimeError(result.stderr.strip())
        return result.stdout
    try:
        # NUL paths avoid quoting, spaces, Unicode and rename-format ambiguities.
        tracked = sorted(set(filter(None, git('diff', 'HEAD', '--name-only', '-z').split('\0'))))
        untracked = sorted(filter(None, git('ls-files', '--others', '--exclude-standard', '-z').split('\0')))
        return {'source_commit': git('rev-parse', 'HEAD').strip(), 'tracked_changes_at_record': tracked,
                'untracked_files_at_record': untracked}
    except (OSError, RuntimeError, subprocess.SubprocessError) as error:
        return {'source_commit': 'unknown', 'source_state_error': str(error)}


def current_save_formats(root):
    """Read writer/loader contracts instead of repeating stale release constants.

    The scenario envelope and each colony snapshot have independent versions.
    Fail closed when either source contract is absent or no longer recognizable.
    """
    formats = {}
    for label, filename, field in (
            ('colony', 'Source/Seige/Simulation/SeigeSimulation.cpp', 'save_format'),
            ('scenario', 'Source/Seige/SeigePersistence.cpp', 'format')):
        source = (root / filename).read_text(encoding='utf-8-sig')
        writers = set(re.findall(r'SetNumberField\(\s*TEXT\("' + field + r'"\)\s*,\s*(\d+)\s*\)', source))
        loaders = set(re.findall(r'\bFormat\s*!=\s*(\d+)\b', source))
        if len(writers) != 1 or writers != loaders:
            raise ValueError(f'{label} save writer/loader format is missing, ambiguous or inconsistent')
        formats[label] = int(writers.pop())
    return formats


def record(args, root=ROOT):
    result = {
        'schema_version': 2, 'version': args.version, 'engine': '5.8.3', 'platform': 'Windows x64 Shipping',
        'generated_at_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
        'record_date': datetime.date.today().isoformat(), **git_state(root),
    }
    rules_version = read_json(root / 'Rules/buildings.json').get('version', '')
    formats = current_save_formats(root)
    result.update(rules_version=rules_version, save_format=formats['colony'], scenario_metadata_format=formats['scenario'], save_compatibility_policy={
        'required_format': formats['colony'],
        'required_scenario_metadata_format': formats['scenario'],
        'required_rules_version': rules_version,
        'requires_matching_rules_fingerprint': True,
        'requires_matching_ai_fingerprints_for_saved_ai_colonies': True,
        'fingerprint_input': 'Parsed Rules and AIFILES JSON; whitespace, line endings and equivalent number spelling do not alter compatibility.',
        'project_version_only_change_invalidates_saves': False,
        'note': f"Valid colony format-{formats['colony']} snapshots inside scenario format-{formats['scenario']} envelopes with matching rules version and content fingerprints remain eligible to load; this is a loader policy, not a claim that every historical save was tested.",
    })
    source_manifest = manifest(source_paths(root), root)
    result['source_asset_manifest'] = source_manifest
    index = Path(args.native) / 'index.json' if args.native else None
    try:
        native = native_result(read_json(index), expected_native_tests(root)) if index else {'status': 'not_run'}
        if index:
            native['report'] = file_record(index, root)
    except (OSError, ValueError, TypeError, AttributeError) as error:
        native = {'status': 'report_missing_or_invalid', 'error': str(error)}
    result['native'] = native
    result['validators'] = {}
    for label, log in (('configuration', args.config_log), ('rules', args.rules_log)):
        if log and Path(log).is_file():
            result['validators'][label] = {**validator_result(Path(log).read_text(encoding='utf-8-sig', errors='replace'), label, rules_version),
                                           'log': file_record(log, root)}
        else:
            result['validators'][label] = {'status': 'not_run'}
    executable, inputs = package_paths(root, args.version)
    packaged_manifest = manifest(inputs, root)
    runtime_files = runtime_comparison(root, executable.parent)
    result['runtime_files'] = runtime_files
    package = {'status': 'not_run', 'executable': file_record(executable, root), 'input_manifest': packaged_manifest}
    if args.package_log and Path(args.package_log).is_file():
        text = Path(args.package_log).read_text(encoding='utf-8-sig', errors='replace')
        valid = ('BUILD SUCCESSFUL' in text and 'AutomationTool exiting with ExitCode=0' in text
                 and not re.search(r'BUILD FAILED|Packaging failed|AutomationTool exiting with ExitCode=[1-9]', text)
                 and len(inputs) > 1 and not any(row.get('missing') for row in packaged_manifest['files'])
                 and all(row['match'] for row in runtime_files) and bool(runtime_files))
        m = re.search(r'BuildCookRun time: ([\d.]+) s', text)
        package.update(status='succeeded' if valid else 'failed', build_log=file_record(args.package_log, root),
                       build_cook_run_seconds=float(m.group(1)) if m else None)
    result['package'] = package
    candidate = Path(args.packaged) if args.packaged else root / f'Saved/packaged-v{args.version}-UiSmoke-verification.json'
    try:
        packaged = packaged_result(read_json(candidate), args.version, packaged_manifest['files'], source_manifest['files'], runtime_files)
        packaged['record'] = file_record(candidate, root)
    except (OSError, ValueError, TypeError, AttributeError) as error:
        packaged = {'status': 'report_missing_or_invalid', 'error': str(error)}
    result['packaged_gameplay'] = packaged
    overall = (native.get('status') == 'single_full_run'
               and all(v['status'] == 'passed' for v in result['validators'].values())
               and package['status'] == 'succeeded' and packaged['status'] == 'passed'
               and result['source_commit'] != 'unknown')
    result['status'] = 'locally_verified' if overall else 'not_verified'
    if args.notes:
        result['notes'] = args.notes
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--version', required=True)
    for option in ('native', 'package-log', 'packaged', 'config-log', 'rules-log', 'notes', 'out'):
        parser.add_argument('--' + option, default='')
    args = parser.parse_args()
    if not re.fullmatch(r'\d+\.\d+\.\d+', args.version):
        parser.error('Version must be a semantic version')
    result = record(args)
    out = Path(args.out) if args.out else ROOT / f'docs/verification/v{args.version}.json'
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(result, indent=1) + '\n', encoding='utf-8')
    print('VERIFICATION_RECORD', result['status'], str(out))
    return 0 if result['status'] == 'locally_verified' else 1


if __name__ == '__main__':
    sys.exit(main())
