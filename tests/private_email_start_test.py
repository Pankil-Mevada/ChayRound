"""Synthetic credentials only; no providers, services or databases are contacted."""
import importlib.util
import os
from pathlib import Path
import subprocess
import sys
import unittest
import tempfile
import fcntl
from unittest.mock import patch

SCRIPT = Path(__file__).resolve().parents[1] / 'scripts/private_email_start.py'
spec = importlib.util.spec_from_file_location('private_email_start', SCRIPT)
launcher = importlib.util.module_from_spec(spec)
spec.loader.exec_module(launcher)


class PrivateEmailTests(unittest.TestCase):
    def test_duplicate_supervisor_reports_action_without_traceback(self):
        with tempfile.TemporaryDirectory() as folder:
            with open(Path(folder) / 'stack.lock', 'a') as lock:
                fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
                result = subprocess.run([sys.executable, str(SCRIPT.with_name('dev.py'))],
                                        env=dict(os.environ, CHAY_DATA_DIR=folder),
                                        capture_output=True, text=True)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn('already running', result.stderr)
                self.assertIn('Ctrl+C', result.stderr)
                self.assertNotIn('Traceback', result.stderr)
                self.assertFalse((Path(folder) / 'internal.key').exists())

    def test_configuration_removes_fixture_overrides(self):
        values = iter(['synthetic-key', 'ChayRound <sender@example.test>'])
        with patch.dict(os.environ, {'CHAY_TEST_FIXTURES': '1',
                                    'CHAY_EMAIL_TEST_URL': 'http://fixture',
                                    'CHAY_RAZORPAY_TEST_URL': 'http://fixture',
                                    'CHAY_TEST_CLOCK_FILE': '/synthetic'}):
            env = launcher.configuration(lambda _: next(values))
            self.assertEqual(env['CHAY_EMAIL_API_KEY'], 'synthetic-key')
            for name in ('CHAY_TEST_FIXTURES', 'CHAY_EMAIL_TEST_URL',
                         'CHAY_RAZORPAY_TEST_URL', 'CHAY_TEST_CLOCK_FILE'):
                self.assertNotIn(name, env)
            self.assertEqual(os.environ['CHAY_TEST_FIXTURES'], '1')

    def test_rejects_empty_and_control_characters(self):
        for key, sender in [('', 'a@example.test'), ('key', ''),
                            ('key\nvalue', 'a@example.test'),
                            ('key', 'a@example.test\rInjected')]:
            values = iter([key, sender])
            with self.assertRaises(ValueError):
                launcher.configuration(lambda _: next(values))

    def test_refuses_piped_credentials_before_reading_them(self):
        result = subprocess.run([sys.executable, str(SCRIPT)], input='DO_NOT_READ',
                                capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertNotIn('DO_NOT_READ', result.stdout + result.stderr)
        self.assertNotIn('Resend API key', result.stdout + result.stderr)


if __name__ == '__main__':
    unittest.main()
