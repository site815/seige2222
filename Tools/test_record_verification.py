"""Release-evidence regressions; no Unreal, network, or real project mutations.

Run: python -m unittest discover -s Tools -p test_record_verification.py
"""
import copy
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('record_verification', Path(__file__).with_name('record_verification.py'))
gate = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gate)


def scratch_directory():
    # Keep fixtures inside the ignored workspace; desktop sandboxes may deny Temp.
    parent = gate.ROOT / 'Saved/VerificationTests'
    parent.mkdir(parents=True, exist_ok=True)
    return tempfile.TemporaryDirectory(prefix='release-gate-', dir=parent)


class NativeEvidenceTests(unittest.TestCase):
    def setUp(self):
        self.names = ['Seige.AI.Test', 'Seige.Workers.Test']
        self.report = {'succeeded': 2, 'succeededWithWarnings': 0, 'failed': 0, 'notRun': 0, 'inProcess': 0,
                       'tests': [{'fullTestPath': n, 'state': 'Success', 'errors': 0, 'warnings': 0, 'entries': []} for n in self.names]}

    def test_complete_report_passes(self):
        self.assertEqual(gate.native_result(self.report, self.names)['status'], 'single_full_run')

    def test_empty_partial_and_duplicate_reports_never_pass(self):
        candidates = [{}, {**self.report, 'tests': []}, {**self.report, 'tests': self.report['tests'][:1]},
                      {**self.report, 'tests': [self.report['tests'][0]] * 2}]
        for report in candidates:
            with self.subTest(report=report):
                self.assertEqual(gate.native_result(report, self.names)['status'], 'failed')

    def test_summary_counters_and_per_test_errors_are_both_checked(self):
        for key, value in [('succeeded', 1), ('notRun', 1), ('inProcess', 1), ('failed', 1), ('succeeded', '2')]:
            report = copy.deepcopy(self.report)
            report[key] = value
            with self.subTest(key=key, value=value):
                self.assertEqual(gate.native_result(report, self.names)['status'], 'failed')
        for change in ({'state': 'Fail'}, {'errors': 1}, {'entries': [{'event': {'type': 'Error', 'message': 'bad'}}]}):
            report = copy.deepcopy(self.report)
            report['tests'][0].update(change)
            self.assertEqual(gate.native_result(report, self.names)['status'], 'failed')

    def test_registration_scan_includes_new_suite_members_and_excludes_diagnostics(self):
        with scratch_directory() as directory:
            root = Path(directory)
            (root / 'Source').mkdir()
            (root / 'Source/Test.cpp').write_text('IMPLEMENT_SIMPLE_AUTOMATION_TEST(A,"Seige.One",Flags)\n'
                                                'IMPLEMENT_SIMPLE_AUTOMATION_TEST(B, TEXT("Seige.Two"), Flags)\n'
                                                'IMPLEMENT_SIMPLE_AUTOMATION_TEST(C,"Diagnostics.Trace",Flags)')
            self.assertEqual(gate.expected_native_tests(root), ['Seige.One', 'Seige.Two'])

    def test_parameterized_suite_requires_all_authored_combinations(self):
        with scratch_directory() as directory:
            root = Path(directory)
            (root / 'Source').mkdir()
            (root / 'Source/Test.cpp').write_text('const TArray<FString> StandardTypes={TEXT("a"),TEXT("b")};\n'
                                                'const TArray<FString> RareTypes={TEXT("r"),TEXT("s"),TEXT("t")};\n'
                                                'IMPLEMENT_COMPLEX_AUTOMATION_TEST(C,"Seige.Economy.StarterCombinations",Flags)')
            names = gate.expected_native_tests(root)
            self.assertEqual(len(names), 6)
            self.assertIn('Seige.Economy.StarterCombinations.b__s_t', names)


class ValidatorEvidenceTests(unittest.TestCase):
    rules = ('Rules valid: prototype-9.3; 22 items, 14 recipes, 52 buildings.\n'
             'Renewable paths verified for 22 items; startup building/worker affordability verified.\n'
             '133 negative validation cases passed.\n')
    config = ('Configuration valid: prototype-9.3; 22 items, 14 recipes, 52 buildings.\n'
              'AI: 22 priorities. Interface: 31 entries.\n'
              'Graphics: 10 nature roles resolve to Content assets; 6 rendered centimeters per simulation unit.\n')

    def test_exact_success_and_self_test_markers_are_required(self):
        self.assertEqual(gate.validator_result(self.rules, 'rules', 'prototype-9.3')['status'], 'passed')
        self.assertEqual(gate.validator_result(self.config, 'configuration', 'prototype-9.3')['status'], 'passed')
        for text in ('', 'Cannot find module', self.rules.splitlines()[0], self.rules.replace('133 negative', '0 negative'),
                     self.rules.replace('prototype-9.3', 'prototype-9.2'), self.rules + 'Error: runtime failure'):
            with self.subTest(text=text):
                self.assertEqual(gate.validator_result(text, 'rules', 'prototype-9.3')['status'], 'failed')


class PackagedEvidenceTests(unittest.TestCase):
    def setUp(self):
        self.inputs = [{'path': 'Builds/v0.9.3/Windows/seige2222/Binaries/Win64/Seige-Win64-Shipping.exe', 'bytes': 12, 'sha256': 'A' * 64},
                       {'path': 'Builds/v0.9.3/Windows/seige2222/Content/Paks/game.ucas', 'bytes': 24, 'sha256': 'B' * 64}]
        self.source = [{'path': 'Content/Art/NewMesh.uasset', 'bytes': 7, 'sha256': 'C' * 64}]
        self.runtime = [{'path': 'Rules/buildings.json', 'source_sha256': 'D' * 64, 'staged_sha256': 'D' * 64, 'match': True}]
        self.data = {'schema_version': 2, 'version': '0.9.3', 'mode': 'UiSmoke', 'exit_code': 0,
                     'report': {'ready': True, 'failures': 0, 'assertion_failures': [], 'completed_stages': 117,
                                'main_test_speed': 10, 'walking_probe_speed': 1},
                     'socket_samples': [{'process_ids': [123], 'tcp': 0, 'udp': 0}], 'inputs_unchanged': True,
                     'packaged_inputs': copy.deepcopy(self.inputs), 'source_inputs': copy.deepcopy(self.source),
                     'runtime_files': copy.deepcopy(self.runtime)}

    def result(self, data=None):
        return gate.packaged_result(self.data if data is None else data, '0.9.3', self.inputs, self.source, self.runtime)

    def test_complete_bound_route_passes(self):
        self.assertEqual(self.result()['status'], 'passed')

    def test_old_unbound_report_or_wrong_version_rejected(self):
        for key, value in [('schema_version', 1), ('version', '0.9.2'), ('mode', 'PrototypeSmoke'), ('exit_code', 1),
                           ('packaged_inputs', []), ('source_inputs', []), ('inputs_unchanged', False)]:
            data = copy.deepcopy(self.data)
            data[key] = value
            with self.subTest(key=key):
                self.assertEqual(self.result(data)['status'], 'failed')

    def test_exit_zero_does_not_hide_route_failures_or_missing_counters(self):
        for key, value in [('ready', False), ('completed_stages', 116), ('failures', 1), ('failures', None),
                           ('assertion_failures', ['bad']), ('main_test_speed', 1)]:
            data = copy.deepcopy(self.data)
            data['report'][key] = value
            with self.subTest(key=key):
                self.assertEqual(self.result(data)['status'], 'failed')

    def test_absent_or_failed_network_observations_are_not_zero_endpoints(self):
        for samples in ([], [{'process_ids': [123], 'tcp': 1, 'udp': 0}], [{'process_ids': [123], 'tcp': None, 'udp': 0}],
                        [{'process_ids': [], 'tcp': 0, 'udp': 0}], [{'process_ids': [123], 'tcp': 0, 'udp': -1}]):
            data = copy.deepcopy(self.data)
            data['socket_samples'] = samples
            with self.subTest(samples=samples):
                self.assertEqual(self.result(data)['status'], 'failed')

    def test_changed_executable_cooked_assets_source_and_loose_rules_each_fail(self):
        for key, index in [('packaged_inputs', 0), ('packaged_inputs', 1), ('source_inputs', 0)]:
            data = copy.deepcopy(self.data)
            data[key][index]['sha256'] = 'E' * 64
            with self.subTest(key=key, index=index):
                self.assertEqual(self.result(data)['status'], 'failed')
        data = copy.deepcopy(self.data)
        data['runtime_files'][0]['staged_sha256'] = 'E' * 64
        self.assertEqual(self.result(data)['status'], 'failed')

    def test_duplicate_manifest_paths_rejected(self):
        data = copy.deepcopy(self.data)
        data['packaged_inputs'].append(data['packaged_inputs'][0])
        self.assertEqual(self.result(data)['status'], 'failed')


class AssetInventoryTests(unittest.TestCase):
    def test_git_state_preserves_all_modified_and_untracked_paths(self):
        modified = [f'Content/Art/Mesh {i}.uasset' for i in range(75)]
        untracked = ['Content/Art/New mesh.uasset', 'Art/새로운.png']

        def command(args, **kwargs):
            class Result:
                returncode = 0
                stderr = ''
            result = Result()
            result.stdout = ('\0'.join(modified) + '\0' if args[1] == 'diff' else
                             '\0'.join(untracked) + '\0' if args[1] == 'ls-files' else 'a' * 40 + '\n')
            return result

        with patch.object(gate.subprocess, 'run', side_effect=command):
            state = gate.git_state(gate.ROOT)
        self.assertEqual(state['tracked_changes_at_record'], sorted(modified))
        self.assertEqual(state['untracked_files_at_record'], sorted(untracked))

    def test_new_asset_changes_source_manifest_without_a_git_commit(self):
        with scratch_directory() as directory:
            root = Path(directory)
            (root / 'Content/Art').mkdir(parents=True)
            (root / 'seige2222.uproject').write_text('{}')
            original = gate.manifest(gate.source_paths(root), root)
            (root / 'Content/Art/NewMesh.uasset').write_bytes(b'new mesh')
            changed = gate.manifest(gate.source_paths(root), root)
            self.assertNotEqual(original['sha256'], changed['sha256'])
            self.assertIn('Content/Art/NewMesh.uasset', [r['path'] for r in changed['files']])

    def test_missing_changed_and_extra_staged_rules_fail(self):
        with scratch_directory() as directory:
            root = Path(directory)
            binary = root / 'Builds/Binaries'
            (root / 'Rules').mkdir()
            (binary / 'Rules').mkdir(parents=True)
            (root / 'Rules/live.json').write_text('{"value":1}')
            self.assertFalse(gate.runtime_comparison(root, binary)[0]['match'])
            (binary / 'Rules/live.json').write_text('{"value":2}')
            self.assertFalse(gate.runtime_comparison(root, binary)[0]['match'])
            (binary / 'Rules/live.json').write_text('{"value":1}')
            self.assertTrue(gate.runtime_comparison(root, binary)[0]['match'])
            (binary / 'Rules/retired.json').write_text('{}')
            self.assertEqual(sum(not row['match'] for row in gate.runtime_comparison(root, binary)), 1)


if __name__ == '__main__':
    unittest.main()
