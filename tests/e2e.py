"""HTTP integration tests: isolated ports, temporary databases, no real payments."""
import concurrent.futures
import json
import os
from pathlib import Path
import secrets
import socket
import sqlite3
import subprocess
import tempfile
import time
import urllib.error
import urllib.request

ROOT = Path(__file__).resolve().parents[1]

def free_port():
    with socket.socket() as s:
        s.bind(('127.0.0.1', 0))
        return s.getsockname()[1]

def main():
    ports = {name: free_port() for name in ('merchant', 'order', 'gateway')}
    base = f"http://127.0.0.1:{ports['gateway']}"
    processes, logs = [], []
    count = 0
    def check(ok, message):
        nonlocal count
        if not ok:
            raise AssertionError(message)
        count += 1

    def request(path, data=None, owner=None, office=None, key=None, url=base):
        headers = {'Content-Type': 'application/json'}
        if owner:
            headers['Authorization'] = 'Bearer ' + owner
        if office:
            headers['X-Office-Token'] = office
        if key:
            headers['Idempotency-Key'] = key
        req = urllib.request.Request(url + path, headers=headers,
                                     data=json.dumps(data).encode() if data is not None else None)
        try:
            with urllib.request.urlopen(req, timeout=12) as r:
                raw = r.read().decode()
                return r.status, json.loads(raw) if 'json' in r.headers.get('Content-Type', '') else raw
        except urllib.error.HTTPError as e:
            return e.code, json.loads(e.read())

    with tempfile.TemporaryDirectory(prefix='chayround-e2e-') as temp:
        env = dict(os.environ, CHAY_INTERNAL_KEY=secrets.token_hex(32),
                   CHAY_MERCHANT_URL=f"http://127.0.0.1:{ports['merchant']}",
                   CHAY_ORDER_URL=f"http://127.0.0.1:{ports['order']}",
                   CHAY_FRONTEND=str(ROOT / 'frontend'))
        try:
            def start(name):
                log = open(Path(temp) / f'{name}.log', 'a')
                logs.append(log)
                p = subprocess.Popen([str(ROOT / 'build' / name)], cwd=ROOT,
                                     env=dict(env, PORT=str(ports[name]), CHAY_DB=str(Path(temp) / f'{name}.db')),
                                     stdout=log, stderr=log)
                processes.append(p)
                for _ in range(100):
                    if p.poll() is not None:
                        raise RuntimeError(f'{name} startup failed')
                    try:
                        if request('/health', url=f'http://127.0.0.1:{ports[name]}')[0] == 200:
                            return p
                    except OSError:
                        pass
                    time.sleep(.05)
                raise RuntimeError('Readiness timeout')

            for name in ports:
                start(name)
            check(request('/')[0] == 200, 'Frontend served')
            anonymous = request('/api/owner')
            check(anonymous[0] == 401, f'Anonymous owner rejected: {anonymous}')
            owners = []
            for n in range(2):
                user = dict(email=f'owner{n}@example.test', password='local-test-password-123', stall_name=f'Test stall {n}')
                check(request('/api/register', user)[0] == 201, 'Registration')
                owners.append(request('/api/login', dict(email=user['email'], password=user['password']))[1]['token'])
            a, b = owners
            check(request('/api/login', dict(email='owner0@example.test', password='incorrect'))[0] == 401, 'Bad password')
            item = request('/api/menu', dict(name='Regular tea', price_paise=1500), owner=a)[1]['id']
            foreign_item = request('/api/menu', dict(name='Other stall tea', price_paise=100), owner=b)[1]['id']
            check(request('/api/menu', dict(name='Invalid', price_paise=1.5), owner=a)[0] == 400, 'Fractional paise rejected')
            office = request('/api/offices', dict(name='ABC Office', policy='MONTHLY', credit_limit_paise=6000), owner=a)[1]
            token = office['office_token']
            check(len(token) == 64, 'Private token generated')
            check(request('/api/office', office='0' * 64)[0] == 401, 'Unknown office rejected')
            check(request('/internal/office', dict(office_token=token), url=env['CHAY_MERCHANT_URL'])[0] == 401, 'Internal endpoint protected')
            check(request('/api/orders', dict(item_id=foreign_item, quantity=1), office=token, key='foreign-item')[0] == 400, 'Cross-stall menu denied')
            payload = dict(item_id=item, quantity=2, total_paise=1, stall_id=9999, office_id=9999)
            status, result = request('/api/orders', payload, office=token, key='first-order')
            check(status == 201 and result['total_paise'] == 3000, 'Server price and identity authoritative')
            order_id = result['id']
            check(request('/api/orders', payload, office=token, key='first-order')[1]['id'] == order_id, 'Idempotent replay')
            check(request('/api/orders', dict(item_id=item, quantity=1), office=token, key='first-order')[0] == 409, 'Conflicting replay rejected')
            check(request('/api/orders', owner=b)[1] == [], 'Other owner sees no orders')
            check(request(f'/api/orders/{order_id}/status', dict(status='ACCEPTED', version=1), owner=b)[0] == 404, 'Other owner cannot update order')
            check(request(f'/api/orders/{order_id}/status', dict(status='FULFILLED', version=1), owner=a)[0] == 409, 'Skipped state rejected')
            for version, state in enumerate(('ACCEPTED', 'READY', 'FULFILLED'), 1):
                check(request(f'/api/orders/{order_id}/status', dict(status=state, version=version), owner=a)[0] == 200, 'Valid transition')
            check(request(f'/api/orders/{order_id}/status', dict(status='CANCELLED', version=1), owner=a)[0] == 409, 'Stale state rejected')
            def competing(n):
                return request('/api/orders', dict(item_id=item, quantity=2), office=token, key=f'concurrent-{n}')[0]
            with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
                statuses = list(pool.map(competing, range(4)))
            check(statuses.count(201) == 1 and statuses.count(409) == 3, 'Atomic credit under concurrency')
            check(request('/api/office', office=token)[1]['committed_paise'] == 6000, 'Committed credit correct')
            active = next(o for o in request('/api/orders', owner=a)[1] if o['status'] == 'PLACED')
            check(request(f"/api/orders/{active['id']}/status", dict(status='CANCELLED', version=active['version']), owner=a)[0] == 200, 'Pending cancellation')
            check(request('/api/office', office=token)[1]['committed_paise'] == 3000, 'Cancellation releases reserved credit')
            def same_key(_):
                return request('/api/orders', dict(item_id=item, quantity=2), office=token, key='same-intent-key')
            with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
                replies = list(pool.map(same_key, range(4)))
            check([r[0] for r in replies].count(201) == 1 and [r[0] for r in replies].count(200) == 3, 'Concurrent same-key request deduplicated')
            check(len({r[1]['id'] for r in replies}) == 1, 'Concurrent replays return original ID')
            prepaid = request('/api/offices', dict(name='Prepaid Office', policy='PREPAID', credit_limit_paise=0), owner=a)[1]['office_token']
            check(request('/api/orders', dict(item_id=item, quantity=1), office=prepaid, key='no-live-payments')[0] == 409, 'Unimplemented prepaid fails closed')
            rotated = request(f"/api/offices/{office['id']}/rotate", {}, owner=a)[1]['office_token']
            check(request('/api/office', office=token)[0] == 401, 'Old link revoked')
            check(request('/api/orders', office=rotated)[0] == 200, 'New link valid')
            with sqlite3.connect(Path(temp) / 'merchant.db') as db:
                stored = db.execute('SELECT token_hash FROM offices WHERE id=?', (office['id'],)).fetchone()[0]
                check(stored != rotated, 'Raw office token not stored')
            # Restart Order Service against the same temporary database.
            processes[1].terminate()
            processes[1].wait(timeout=5)
            start('order')
            check(len(request('/api/orders', owner=a)[1]) == 3, 'Orders survive service restart')
            processes[0].terminate()
            processes[0].wait(timeout=5)
            check(request('/api/orders', owner=a)[0] == 503, 'Dependency outage fails closed')
            print(f'{count} HTTP integration checks passed; disposable DBs; no external provider calls.')
        except Exception:
            for name in ports:
                file = Path(temp) / f'{name}.log'
                if file.exists():
                    print(name, file.read_text()[-6000:])
            raise
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
