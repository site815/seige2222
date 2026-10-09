"""Write docs/verification/v<version>.json from the artefacts of a verification run.

    python Tools/record_verification.py --version 0.9.2 --native Saved/Automation/v092-full \
        --package-log Saved/package-v092.log --config-log Saved/claude-validate-config.log \
        --rules-log Saved/claude-validate-rules.log [--notes "..."]

Standard library only (runs with the bundled Blender Python on the development PC).
Every number is read from the files; nothing is typed in by hand. Missing inputs
are recorded as missing rather than guessed.
"""
import argparse, datetime, hashlib, json, re, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ap = argparse.ArgumentParser()
ap.add_argument('--version', required=True)
ap.add_argument('--native', default='')
ap.add_argument('--package-log', default='')
ap.add_argument('--packaged', default='')
ap.add_argument('--config-log', default='')
ap.add_argument('--rules-log', default='')
ap.add_argument('--notes', default='')
ap.add_argument('--out', default='')
args = ap.parse_args()


def rel(p):
    return str(Path(p).resolve().relative_to(ROOT)).replace('\\', '/') if p and Path(p).exists() else None


def file_record(p):
    path = Path(p)
    if not path.exists():
        return {'path': str(p).replace('\\', '/'), 'missing': True}
    return {'path': rel(path) or str(p), 'bytes': path.stat().st_size, 'sha256': hashlib.sha256(path.read_bytes()).hexdigest().upper()}


def read_text(p):
    path = Path(p)
    return path.read_text(encoding='utf-8', errors='replace') if p and path.exists() else ''


record = {
    'version': args.version, 'engine': '5.8.3', 'platform': 'Windows x64 Shipping',
    'generated_at_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
    'record_date': datetime.date.today().isoformat(),
}
# The source the gate ran on: HEAD, and whether tracked files differed from it.
try:
    import subprocess
    head = subprocess.run(['git', 'rev-parse', 'HEAD'], cwd=ROOT, capture_output=True, text=True, timeout=30)
    dirty = subprocess.run(['git', 'status', '--porcelain', '--untracked-files=no'], cwd=ROOT, capture_output=True, text=True, timeout=60)
    if head.returncode == 0:
        record['source_commit'] = head.stdout.strip()
        record['tracked_changes_at_record'] = [line[3:] for line in dirty.stdout.splitlines()][:50] if dirty.returncode == 0 else 'unknown'
except (OSError, subprocess.SubprocessError):
    record['source_commit'] = 'unknown'
rules = json.loads((ROOT / 'Rules/buildings.json').read_text(encoding='utf-8-sig'))
record['rules_version'] = rules.get('version')
record['save_format'] = 7
record['prior_saves_compatible'] = False

# ---- native automation
native = {'status': 'not_run'}
index = Path(args.native) / 'index.json' if args.native else None
if index and index.exists():
    report = json.loads(index.read_text(encoding='utf-8-sig'))
    tests = report.get('tests', [])
    by_state = {}
    for t in tests:
        by_state[t.get('state', '?')] = by_state.get(t.get('state', '?'), 0) + 1
    failed = [t for t in tests if t.get('state') not in ('Success',)]
    warned = [t for t in tests if t.get('state') == 'Success' and (t.get('warnings', 0) or 0) > 0]
    native = {
        'status': 'single_full_run',
        'report': file_record(index),
        'report_created_on': report.get('reportCreatedOn'),
        'unique_tests': len(tests), 'succeeded': report.get('succeeded'), 'succeeded_with_warnings': report.get('succeededWithWarnings'),
        'failed': report.get('failed'), 'not_run': report.get('notRun'), 'duration_seconds': report.get('totalDuration'),
        'states': by_state,
        'failures': [{'full_test_path': t.get('fullTestPath'), 'state': t.get('state'), 'errors': t.get('errors'),
                      'messages': [e.get('event', {}).get('message') for e in t.get('entries', []) if e.get('event', {}).get('type') in ('Error', 'Warning')][:6]} for t in failed],
        'warnings_in_passing_tests': [t.get('fullTestPath') for t in warned],
    }
elif args.native:
    native = {'status': 'report_missing', 'expected': args.native}
record['native'] = native

# ---- validators
def validator(log, label):
    text = read_text(log)
    if not text:
        return {'status': 'not_run'}
    ok = ('FAILED' not in text.upper()) and bool(text.strip())
    return {'status': 'passed' if ok else 'failed', 'log': file_record(log), 'tail': text.strip().splitlines()[-3:]}
record['validators'] = {'configuration': validator(args.config_log, 'configuration'), 'rules': validator(args.rules_log, 'rules')}

# ---- package
package = {'status': 'not_run'}
if args.package_log and Path(args.package_log).exists():
    text = read_text(args.package_log)
    m = re.search(r'BuildCookRun time: ([\d.]+) s', text)
    exe = ROOT / f'Builds/v{args.version}/Windows/seige2222/Binaries/Win64/Seige-Win64-Shipping.exe'
    package = {'status': 'succeeded' if 'BUILD SUCCESSFUL' in text and 'AutomationTool exiting with ExitCode=0' in text else 'failed',
               'build_log': file_record(args.package_log), 'build_cook_run_seconds': float(m.group(1)) if m else None,
               'executable': file_record(exe) if exe.exists() else {'path': str(exe.relative_to(ROOT)).replace('\\', '/'), 'missing': True}}
record['package'] = package

# ---- packaged gameplay route
packaged = {'status': 'not_run'}
candidate = Path(args.packaged) if args.packaged else ROOT / f'Saved/packaged-v{args.version}-UiSmoke-verification.json'
if candidate.exists():
    data = json.loads(candidate.read_text(encoding='utf-8-sig'))
    rep = data.get('report', {}) or {}
    samples = data.get('socket_samples', []) or []
    packaged = {'status': 'passed' if data.get('exit_code') == 0 and rep.get('completed_stages') == 117 and rep.get('failures') == 0 else 'failed',
                'record': file_record(candidate), 'exit_code': data.get('exit_code'), 'completed_stages': rep.get('completed_stages'), 'failures': rep.get('failures'),
                'socket_samples': len(samples), 'samples_with_endpoints': sum(1 for s in samples if (s.get('tcp') or 0) > 0 or (s.get('udp') or 0) > 0)}
record['packaged_gameplay'] = packaged

overall = all([
    native.get('status') == 'single_full_run' and not native.get('failures'),
    record['validators']['configuration'].get('status') == 'passed', record['validators']['rules'].get('status') == 'passed',
    package.get('status') == 'succeeded', packaged.get('status') == 'passed'])
record['status'] = 'locally_verified' if overall else 'not_verified'
if args.notes:
    record['notes'] = args.notes
out = Path(args.out) if args.out else ROOT / f'docs/verification/v{args.version}.json'
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(json.dumps(record, indent=1) + '\n', encoding='utf-8')
print('VERIFICATION_RECORD', record['status'], str(out))
sys.exit(0 if overall else 1)
