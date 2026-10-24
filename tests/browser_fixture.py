"""Opt-in disposable browser QA stack. Never used by scripts/dev.py."""
from pathlib import Path
import os
import secrets
import signal
import json
import subprocess
import tempfile
import time
import urllib.request
from fixtures import Providers

ROOT=Path(__file__).resolve().parents[1]

def main():
    providers=Providers()
    children=[]
    logs=[]
    with tempfile.TemporaryDirectory(prefix='chayround-browser-') as temp:
        temp=Path(temp)
        providers.mailbox_file=temp/'mailbox.json'
        config=temp/'accounts.local.json'
        config.write_text(json.dumps({'1':{'key_id':'rzp_test_one','key_secret':'isolated-one-secret','webhook_secret':'isolated-webhook-one'}}))
        env=dict(os.environ,CHAY_TEST_FIXTURES='1',CHAY_INTERNAL_KEY=secrets.token_hex(32),
            CHAY_EMAIL_API_KEY='isolated-email-secret',CHAY_EMAIL_FROM='fixture@example.test',
            CHAY_EMAIL_TEST_URL=providers.base+'/emails',CHAY_RAZORPAY_TEST_URL=providers.base+'/v1',
            CHAY_RAZORPAY_ACCOUNTS_FILE=str(config),CHAY_MERCHANT_URL='http://127.0.0.1:9281',
            CHAY_ORDER_URL='http://127.0.0.1:9282',CHAY_FRONTEND=str(ROOT/'frontend'),CHAY_RECONCILE_WORKER='0')
        try:
            for name,port in [('merchant',9281),('order',9282),('gateway',9280)]:
                import socket
                with socket.socket() as probe:probe.bind(('127.0.0.1',port))
                log=open(temp/f'{name}.log','a')
                logs.append(log)
                p=subprocess.Popen([str(ROOT/'build'/name)],cwd=ROOT,env=dict(env,PORT=str(port),CHAY_DB=str(temp/f'{name}.db')),stdout=log,stderr=log)
                children.append(p)
                for _ in range(60):
                    if p.poll() is not None:raise RuntimeError(name+' exited')
                    try:
                        urllib.request.urlopen(f'http://127.0.0.1:{port}/health',timeout=.5).close()
                        break
                    except OSError:time.sleep(.1)
                else:raise RuntimeError(name+' readiness timeout')
            print('DISPOSABLE BROWSER QA: http://localhost:9280',flush=True)
            print('Synthetic mailbox file: '+str(providers.mailbox_file),flush=True)
            print('Do not use actual personal data or attempt the real Razorpay popup with fixture keys.',flush=True)
            while all(p.poll() is None for p in children):time.sleep(1)
        except KeyboardInterrupt:pass
        finally:
            for p in children:
                if p.poll() is None:p.terminate()
            for p in children:
                try:p.wait(timeout=5)
                except subprocess.TimeoutExpired:p.kill();p.wait()
            for log in logs:log.close()
            providers.close()

if __name__=='__main__':main()
