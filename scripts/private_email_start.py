"""Run in your own WSL terminal; never enter credentials in an AI-managed terminal."""
import getpass
import os
from pathlib import Path
import subprocess
import sys
import warnings

ROOT = Path(__file__).resolve().parents[1]


def hidden(prompt):
    # Refuse getpass's echoed-input fallback.
    with warnings.catch_warnings():
        warnings.simplefilter('error', getpass.GetPassWarning)
        return getpass.getpass(prompt).strip()


def configuration(secret=hidden):
    key = secret('Resend API key (hidden): ')
    sender = secret('Verified sender, e.g. ChayRound <hello@your-domain> (hidden): ')
    if not key or not sender or '@' not in sender:
        raise ValueError('Incomplete configuration')
    if any(c in key + sender for c in '\r\n\x00'):
        raise ValueError('Invalid configuration')
    env = dict(os.environ)
    # A private interactive session must never redirect mail into test fixtures.
    for name in ('CHAY_TEST_FIXTURES', 'CHAY_TEST_CLOCK_FILE',
                 'CHAY_EMAIL_TEST_URL', 'CHAY_RAZORPAY_TEST_URL'):
        env.pop(name, None)
    env['CHAY_EMAIL_API_KEY'] = key
    env['CHAY_EMAIL_FROM'] = sender
    return env


def main():
    if os.name != 'posix' or not sys.stdin.isatty() or not sys.stderr.isatty():
        raise ValueError('Use your own interactive WSL terminal')
    print('Enter credentials privately here, never in chat. Nothing is saved by this launcher.')
    env = configuration()
    # Replace this process: Ctrl+C and service cleanup remain owned by dev.py.
    os.execve(sys.executable, [sys.executable, str(ROOT / 'scripts/dev.py')], env)


if __name__ == '__main__':
    try:
        main()
    except (ValueError, getpass.GetPassWarning):
        print('Setup stopped. Use an interactive WSL terminal and valid sender configuration.',
              file=sys.stderr)
        sys.exit(1)
    except (KeyboardInterrupt, EOFError):
        print('\nSetup cancelled.', file=sys.stderr)
        sys.exit(1)
