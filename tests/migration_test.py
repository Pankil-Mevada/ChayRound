"""Upgrade actual v1-shaped databases; preserve records and import charges once."""
from pathlib import Path
import os
import secrets
import socket
import sqlite3
import subprocess
import tempfile
import time
import urllib.request

ROOT=Path(__file__).resolve().parents[1]
def main():
    with tempfile.TemporaryDirectory(prefix='chayround-migration-') as temp:
        temp=Path(temp)
        with sqlite3.connect(temp/'merchant.db') as db:
            db.executescript('''CREATE TABLE owners(id INTEGER PRIMARY KEY,email TEXT UNIQUE NOT NULL,salt TEXT NOT NULL,password TEXT NOT NULL);
            CREATE TABLE stalls(id INTEGER PRIMARY KEY,owner_id INTEGER UNIQUE NOT NULL,name TEXT NOT NULL);
            CREATE TABLE menu(id INTEGER PRIMARY KEY,stall_id INTEGER NOT NULL,name TEXT NOT NULL,price INTEGER NOT NULL);
            CREATE TABLE offices(id INTEGER PRIMARY KEY,stall_id INTEGER NOT NULL,name TEXT NOT NULL,token_hash TEXT UNIQUE NOT NULL,policy TEXT NOT NULL,credit_limit INTEGER NOT NULL);
            INSERT INTO owners VALUES(1,'legacy@example.test','salt','old-hash');
            INSERT INTO stalls VALUES(1,1,'Preserved legacy stall');
            INSERT INTO menu VALUES(1,1,'Legacy tea',1500);
            INSERT INTO offices VALUES(1,1,'Legacy office','legacy-token-hash','MONTHLY',10000);''')
        with sqlite3.connect(temp/'order.db') as db:
            db.executescript('''CREATE TABLE orders(id TEXT PRIMARY KEY,stall_id INTEGER NOT NULL,office_id INTEGER NOT NULL,office_name TEXT NOT NULL,item_id INTEGER NOT NULL,item_name TEXT NOT NULL,quantity INTEGER NOT NULL,total INTEGER NOT NULL,status TEXT NOT NULL,settlement TEXT NOT NULL DEFAULT 'UNPAID',version INTEGER NOT NULL DEFAULT 1,policy TEXT NOT NULL,idem TEXT NOT NULL,request_hash TEXT NOT NULL,created INTEGER NOT NULL,UNIQUE(office_id,idem));
            INSERT INTO orders VALUES('legacy_order',1,1,'Legacy office',1,'Legacy tea',2,3000,'FULFILLED','UNPAID',4,'MONTHLY','legacy-key','legacy-hash',1700000000);''')
        def boot(service):
            with socket.socket() as sock:
                sock.bind(('127.0.0.1',0));port=sock.getsockname()[1]
            env=dict(os.environ,CHAY_INTERNAL_KEY=secrets.token_hex(32),CHAY_DB=str(temp/f'{service}.db'),PORT=str(port),CHAY_RECONCILE_WORKER='0')
            with open(temp/f'{service}.log','a') as log:
                child=subprocess.Popen([str(ROOT/'build'/service)],env=env,stdout=log,stderr=log)
                try:
                    for _ in range(100):
                        if child.poll() is not None:raise RuntimeError(service+' migration failed')
                        try:
                            with urllib.request.urlopen(f'http://127.0.0.1:{port}/health',timeout=.3):break
                        except OSError:time.sleep(.05)
                    else:raise RuntimeError('Readiness timed out')
                finally:
                    child.terminate()
                    try:child.wait(timeout=5)
                    except subprocess.TimeoutExpired:child.kill();child.wait()
        for _ in range(2):
            boot('merchant');boot('order')
        checks=0
        def check(ok):
            nonlocal checks
            if not ok:raise AssertionError('Migration invariant failed')
            checks+=1
        with sqlite3.connect(temp/'merchant.db') as db:
            check(db.execute('SELECT name FROM stalls').fetchone()[0]=='Preserved legacy stall')
            check(db.execute('SELECT verified FROM owners').fetchone()[0]==0)
            check(db.execute('SELECT verified FROM offices').fetchone()[0]==0)
            check(db.execute('SELECT paused FROM stalls').fetchone()[0]==1)
            check(db.execute('PRAGMA user_version').fetchone()[0]==2)
        with sqlite3.connect(temp/'order.db') as db:
            check(db.execute('SELECT COUNT(*),SUM(amount) FROM ledger').fetchone()==(1,3000))
            check(db.execute('SELECT price,quantity FROM order_items').fetchone()==(1500,2))
            check(db.execute('SELECT status,version FROM orders').fetchone()==('FULFILLED',4))
            check(db.execute('PRAGMA user_version').fetchone()[0]==2)
        print(f'{checks} legacy migration/restart invariants passed; disposable databases.')
if __name__=='__main__':main()
