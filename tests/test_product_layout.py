"""Compatibility checks for source ownership and release identities."""
import configparser
import importlib.util
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('metadata', ROOT / 'scripts/product_release_metadata.py')
metadata = importlib.util.module_from_spec(spec)
spec.loader.exec_module(metadata)


class ProductLayoutTests(unittest.TestCase):
    def test_all_existing_targets_and_source_ownership(self):
        config = configparser.ConfigParser(interpolation=None)
        config.read(ROOT / 'platformio.ini')
        self.assertEqual({s[4:] for s in config.sections() if s.startswith('env:')}, {
            'logger-nodemcuv2', 'logger-nodemcuv2-ota', 'logger-esp32',
            'logger-tinyc6', 'dash-waveshare-s3-128', 'logger-esp32-production-candidate'})
        self.assertEqual(config['env']['build_src_filter'], '+<logger/firmware/src/>')
        self.assertEqual(config['env:dash-waveshare-s3-128']['build_src_filter'].strip(),
                         '+<dash/firmware/src/dash_main.cpp>')
        self.assertIn('APEXI_PRODUCTION_SECURITY_REQUIRED=1',
                      config['env:logger-esp32-production-candidate']['build_flags'])

    def test_shared_protocols_have_one_definition(self):
        for filename in ['DashLinkProtocol.h', 'DashTelemetry.h', 'SystemEvents.h',
                         'DashUploadStatus.h', 'SensorTypes.h']:
            paths = [p.relative_to(ROOT) for area in ['logger', 'dash', 'shared']
                     for p in (ROOT / area).rglob(filename)]
            self.assertEqual(paths, [Path('shared/protocols') / filename])

    def test_firmware_version_is_preserved_and_hardware_independent(self):
        aggregate = {'version': 'v2.3.4', 'commit_sha': 'abc',
                     'release_security_profile': 'development-only',
                     'targets': [{'environment': 'logger-esp32', 'firmware': 'logger.bin'},
                                 {'environment': 'dash-waveshare-s3-128', 'firmware': 'dash.bin'}]}
        for product in ['logger', 'dash']:
            hardware = json.loads((ROOT / product / 'hardware/revision.json').read_text())
            result = metadata.product_metadata(aggregate, product, hardware)
            self.assertEqual(result['firmware_version'], 'v2.3.4')
            self.assertIsNone(result['hardware_revision'])
            self.assertEqual(len(result['targets']), 1)
            self.assertTrue(result['targets'][0]['environment'].startswith(product + '-'))
            hardware['hardware_revision'] = 'test-fixture-revision'
            self.assertEqual(metadata.product_metadata(aggregate, product, hardware)['firmware_version'], 'v2.3.4')

    def test_mismatched_hardware_fails(self):
        with self.assertRaises(ValueError):
            metadata.product_metadata({}, 'logger', {'product': 'dash'})


if __name__ == '__main__':
    unittest.main()
