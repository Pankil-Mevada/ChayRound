"""Local supervisor. Run inside WSL from the project root; Ctrl+C stops children."""
import os
import pathlib
import secrets
import subprocess
import time
import urllib.request

ROOT = pathlib.Path(__file__).resolve().parents[1]

def main():
    data = ROOT / '.run'
    data.mkdir(exist_ok=True)
    env = dict(os.environ, CHAY_INTERNAL_KEY=secrets.token_hex(32),
               CHAY_FRONTEND=str(ROOT / 'frontend'))
    processes, logs = [], []
    try:
        for name, port in [('merchant', 9181), ('order', 9182), ('gateway', 9180)]:
            # Refuse occupied ports; do not replace another running stack.
            import socket
            with socket.socket() as probe:
                probe.bind(('127.0.0.1', port))
            log = open(data / f'{name}.log', 'a')
            logs.append(log)
            child_env = dict(env, PORT=str(port), CHAY_DB=str(data / f'{name}.db'))
            p = subprocess.Popen([str(ROOT / 'build' / name)], env=child_env,
                                 cwd=ROOT, stdout=log, stderr=log)
            processes.append(p)
            for _ in range(60):
                if p.poll() is not None:
                    raise RuntimeError(f'{name} exited; inspect .run/{name}.log')
                try:
                    with urllib.request.urlopen(f'http://127.0.0.1:{port}/health', timeout=.5):
                        break
                except OSError:
                    time.sleep(.1)
            else:
                raise RuntimeError(f'{name} readiness timed out')
        print('ChayRound: http://localhost:9180 — local credit-order preview. Ctrl+C stops it.', flush=True)
        while True:
            if any(p.poll() is not None for p in processes):
                raise RuntimeError('A service exited; inspect .run/*.log')
            time.sleep(1)
    except KeyboardInterrupt:
        pass
    finally:
        for p in processes:
            if p.poll() is None:
                p.terminate()
        for p in processes:
            try:
                p.wait(timeout=5)
            except subprocess.TimeoutExpired:
                p.kill()
                p.wait()
        for log in logs:
            log.close()

if __name__ == '__main__':
    main()
