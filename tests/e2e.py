"""Phase 1/2 HTTP contracts, isolated providers, temporary DBs and controlled clock."""
import concurrent.futures
import datetime as dt
import hashlib
import hmac
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
from fixtures import Providers

ROOT=Path(__file__).resolve().parents[1]

def free_port():
    with socket.socket() as s:
        s.bind(('127.0.0.1',0))
        return s.getsockname()[1]

def main():
    count=0
    processes={}
    logs=[]
    providers=Providers()
    def check(value,message):
        nonlocal count
        if not value: raise AssertionError(message)
        count+=1
    with tempfile.TemporaryDirectory(prefix='chayround-phase12-') as temp:
        temp=Path(temp)
        data=temp/'data'
        data.mkdir()
        clock=temp/'clock'
        def set_time(iso): clock.write_text(str(int(dt.datetime.fromisoformat(iso).timestamp())))
        set_time('2026-09-20T06:00:00+00:00')
        config=temp/'accounts.local.json'
        config.write_text(json.dumps({'1':{'key_id':'rzp_test_one','key_secret':'isolated-one-secret','webhook_secret':'isolated-webhook-one'},'2':{'key_id':'rzp_test_two','key_secret':'isolated-two-secret','webhook_secret':'isolated-webhook-two'}}))
        ports={n:free_port() for n in ('merchant','order','gateway')}
        base=f"http://127.0.0.1:{ports['gateway']}"
        env=dict(os.environ,CHAY_TEST_FIXTURES='1',CHAY_TEST_CLOCK_FILE=str(clock),CHAY_RECONCILE_WORKER='0',
                 CHAY_INTERNAL_KEY=secrets.token_hex(32),CHAY_EMAIL_API_KEY='isolated-email-secret',
                 CHAY_EMAIL_FROM='test@example.test',CHAY_EMAIL_TEST_URL=providers.base+'/emails',
                 CHAY_RAZORPAY_ACCOUNTS_FILE=str(config),CHAY_RAZORPAY_TEST_URL=providers.base+'/v1',
                 CHAY_FRONTEND=str(ROOT/'frontend'),CHAY_MERCHANT_URL=f"http://127.0.0.1:{ports['merchant']}",
                 CHAY_ORDER_URL=f"http://127.0.0.1:{ports['order']}")
        def request(path,data=None,owner=None,office=None,pin='',key=None,headers=None,url=base):
            hdr={'Content-Type':'application/json'}
            if owner: hdr['Authorization']='Bearer '+owner
            if office: hdr['X-Office-Token']=office
            if pin: hdr['X-Office-Pin']=pin
            if key: hdr['Idempotency-Key']=key
            if headers: hdr.update(headers)
            raw=data.encode() if isinstance(data,str) else json.dumps(data).encode() if data is not None else None
            req=urllib.request.Request(url+path,data=raw,headers=hdr)
            try:
                with urllib.request.urlopen(req,timeout=30) as r:
                    content=r.read().decode()
                    return r.status,json.loads(content) if 'json' in r.headers.get('Content-Type','') else content
            except urllib.error.HTTPError as e:
                raw=e.read().decode()
                try: result=json.loads(raw)
                except json.JSONDecodeError: result=raw
                return e.code,result
        def expect(status,path,**kwargs):
            actual,value=request(path,**kwargs)
            check(actual==status,f'{path}: expected {status}, got {actual}: {value}')
            return value
        def start(name,folder=data,custom=None):
            log=open(temp/f'{name}-{len(logs)}.log','a')
            logs.append(log)
            child=dict(env,PORT=str(ports[name]),CHAY_DB=str(folder/f'{name}.db'))
            if custom: child.update(custom)
            p=subprocess.Popen([str(ROOT/'build'/name)],cwd=ROOT,env=child,stdout=log,stderr=log)
            processes[name]=p
            for _ in range(100):
                if p.poll() is not None: raise RuntimeError(f'{name} failed to start')
                try:
                    if request('/health',url=f'http://127.0.0.1:{ports[name]}')[0]==200:return
                except OSError: pass
                time.sleep(.05)
            raise RuntimeError('Readiness timeout')
        def stop(name):
            p=processes.pop(name,None)
            if p and p.poll() is None:
                p.terminate()
                try:p.wait(timeout=5)
                except subprocess.TimeoutExpired:p.kill();p.wait()
        def login(n):return expect(200,'/api/login',data={'email':f'owner{n}@example.test','password':'isolated-test-password-123'})['token']
        def verify(email,owner=None,office=None,pin=''):
            challenge=expect(200,'/api/verification/request',data={},owner=owner,office=office,pin=pin)['challenge_id']
            code=providers.emails[email]
            expect(400,'/api/verification/confirm',data={'challenge_id':challenge,'code':'000000'},owner=owner,office=office,pin=pin)
            expect(200,'/api/verification/confirm',data={'challenge_id':challenge,'code':code},owner=owner,office=office,pin=pin)
            expect(400,'/api/verification/confirm',data={'challenge_id':challenge,'code':code},owner=owner,office=office,pin=pin)
        def settings(auth,**changes):
            current=expect(200,'/api/owner',owner=auth)
            body={'name':current['stall_name'],'area':'Test Area','paused':False,'opens':0,'closes':0,'days':127,'version':current['version']}
            body.update(changes)
            expect(200,'/api/stall',data=body,owner=auth)
            return body
        def order(token,items,key,mode=None):
            body={'items':items}
            if mode:body['payment_mode']=mode
            return expect(201,'/api/orders',data=body,office=token,key=key)
        def state(auth,o,target,reason='test cancellation'):
            return expect(200,f"/api/orders/{o['id']}/status",owner=auth,data={'status':target,'version':o['version'],'reason':reason})
        def fulfil(auth,o):
            for target in ('ACCEPTED','READY','FULFILLED'):o=state(auth,o,target)
            return o
        def account(auth,oid):return expect(200,f'/api/accounts/{oid}',owner=auth)
        try:
            for name in ports:start(name)
            expect(200,'/');expect(200,'/app.js');expect(200,'/styles.css')
            expect(401,'/api/owner')
            owners=[]
            for n in range(2):
                expect(201,'/api/register',data={'email':f'owner{n}@example.test','password':'isolated-test-password-123','stall_name':f'Test Tea {n}'})
                auth=login(n)
                expect(403,'/api/menu',owner=auth,data={'name':'Tea','price_paise':1500})
                verify(f'owner{n}@example.test',owner=auth)
                settings(auth)
                owners.append(auth)
            a,b=owners
            expect(401,'/api/login',data={'email':'owner0@example.test','password':'wrong-password'})
            first=expect(201,'/api/menu',owner=a,data={'name':'Regular','price_paise':1500})['id']
            second=expect(201,'/api/menu',owner=a,data={'name':'Special','price_paise':2000})['id']
            foreign=expect(201,'/api/menu',owner=b,data={'name':'Foreign','price_paise':100})['id']
            expect(400,'/api/menu',owner=a,data={'name':'Fractional','price_paise':1.5})
            office=expect(201,'/api/offices',owner=a,data={'name':'ABC','email':'office@example.test','address':'Second floor','area':'Test Area','policy':'MONTHLY','credit_limit_paise':10000})
            token,oid=office['office_token'],office['id']
            check(not expect(200,'/api/office',office=token)['verified'],
                  'A valid link without PIN opens before email verification')
            expect(403,'/api/orders',office=token,key='unverified-office',data={'items':[{'item_id':first,'quantity':1}]})
            verify('office@example.test',office=token)
            expect(401,'/internal/office',data={},url=env['CHAY_MERCHANT_URL'])
            check(expect(401,'/api/office',office='0'*64)['error']=='Office link invalid or revoked',
                  'Invalid link has a specific public error')
            expect(404,f'/api/accounts/{oid}',owner=b)
            expect(409,'/api/orders',office=token,key='foreign-menu-item',data={'item_id':foreign,'quantity':1})
            payload={'items':[{'item_id':first,'quantity':2},{'item_id':second,'quantity':1}],'total_paise':1,'office_id':999,'stall_id':999}
            created=expect(201,'/api/orders',office=token,key='first-intent',data=payload)
            check(created['total_paise']==5000 and len(created['items'])==2,'Server-priced basket snapshots')
            check(expect(200,'/api/orders',office=token,key='first-intent',data=payload)['id']==created['id'],'Idempotent replay')
            expect(409,'/api/orders',office=token,key='first-intent',data={'item_id':first,'quantity':1})
            check(expect(200,'/api/orders',owner=b)['orders']==[],'Tenant list isolation')
            expect(404,f"/api/orders/{created['id']}/status",owner=b,data={'status':'ACCEPTED','version':1})
            expect(409,f"/api/orders/{created['id']}/status",owner=a,data={'status':'FULFILLED','version':1})
            # Availability, historical snapshots and version conflicts.
            expect(200,f'/api/menu/{first}',owner=a,data={'name':'Regular renamed','price_paise':1800,'available':False,'version':1})
            expect(409,'/api/orders',office=token,key='unavailable-item',data={'item_id':first,'quantity':1})
            expect(409,f'/api/menu/{first}',owner=a,data={'name':'stale','price_paise':1500,'available':True,'version':1})
            snapshot=expect(200,'/api/orders',office=token)['orders'][0]
            check(snapshot['total_paise']==5000 and snapshot['items'][0]['name']=='Regular','Historical prices/names unchanged')
            expect(200,f'/api/menu/{first}',owner=a,data={'name':'Regular','price_paise':1500,'available':True,'version':2})
            settings(a,paused=True)
            expect(409,'/api/orders',office=token,key='paused-stall',data={'item_id':first,'quantity':1})
            settings(a,opens=1,closes=2)
            expect(409,'/api/orders',office=token,key='outside-hours',data={'item_id':first,'quantity':1})
            settings(a)
            def spend(n):return request('/api/orders',office=token,key=f'credit-race-{n}',data={'item_id':first,'quantity':3})
            with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:replies=list(pool.map(spend,range(4)))
            check([r[0] for r in replies].count(201)==1 and [r[0] for r in replies].count(409)==3,'Atomic concurrent credit')
            placed=next(r[1] for r in replies if r[0]==201)
            state(a,placed,'CANCELLED')
            check(account(a,oid)['committed_paise']==5000,'Cancellation releases reservation')
            def replay(_):return request('/api/orders',office=token,key='concurrent-same-key',data={'item_id':first,'quantity':3})
            with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:replies=list(pool.map(replay,range(4)))
            check([r[0] for r in replies].count(201)==1 and [r[0] for r in replies].count(200)==3,'Concurrent idempotent retries')
            check(len({r[1]['id'] for r in replies})==1,'Same intent one ID')
            pending=next(r[1] for r in replies if r[0]==201)
            delivered=fulfil(a,created)
            check(account(a,oid)['outstanding_paise']==5000,'Fulfilment creates one charge')
            receipt=expect(201,f'/api/accounts/{oid}/payments',owner=a,key='cash-first-intent',data={'amount_paise':2000,'method':'CASH','reference':'receipt-001'})['receipt_id']
            expect(200,f'/api/accounts/{oid}/payments',owner=a,key='cash-first-intent',data={'amount_paise':2000,'method':'CASH','reference':'receipt-001'})
            expect(409,f'/api/accounts/{oid}/payments',owner=a,key='duplicate-reference',data={'amount_paise':2000,'method':'CASH','reference':'receipt-001'})
            check(account(a,oid)['outstanding_paise']==3000,'Partial allocation')
            expect(409,f'/api/accounts/{oid}/payments',owner=a,key='overpayment-attempt',data={'amount_paise':4000,'method':'BANK','reference':'too-large'})
            expect(201,f'/api/receipts/{receipt}/reverse',owner=a,data={'reason':'Fixture receipt entered in error'})
            expect(200,f'/api/receipts/{receipt}/reverse',owner=a,data={'reason':'Replay'})
            check(account(a,oid)['outstanding_paise']==5000,'Reversal reopens debt without deleting records')
            expect(201,f"/api/orders/{created['id']}/adjustment",owner=a,key='credit-adjustment',data={'amount_paise':-500,'reason':'Quantity correction'})
            expect(201,f"/api/orders/{created['id']}/adjustment",owner=a,key='debit-adjustment',data={'amount_paise':100,'reason':'Agreed extra item'})
            expect(409,f"/api/orders/{created['id']}/adjustment",owner=a,key='too-much-credit',data={'amount_paise':-99999,'reason':'Invalid credit'})
            check(account(a,oid)['outstanding_paise']==4600,'Adjustments reconciled')
            expect(201,f'/api/accounts/{oid}/payments',owner=a,key='bank-final-intent',data={'amount_paise':4600,'method':'BANK','reference':'bank-002'})
            check(account(a,oid)['outstanding_paise']==0,'Full allocation')
            state(a,pending,'CANCELLED')
            final_account=account(a,oid)
            check(final_account['ledger_balance_paise']==0 and final_account['committed_paise']==0,'Ledger and credit reconcile')
            # Policy edits are versioned and do not rewrite prior orders.
            info=expect(200,'/api/owner',owner=a)['offices'][0]
            edit={'name':'ABC','email':'office@example.test','address':'Second floor','area':'Test Area','instructions':'','policy':'PREPAID','credit_limit_paise':10000,'service_enabled':True,'credit_enabled':True,'version':info['version'],'reason':'Test policy change'}
            expect(200,f'/api/offices/{oid}',owner=a,data=edit)
            expect(409,f'/api/offices/{oid}',owner=a,data=edit)
            check(expect(200,'/api/orders',office=token)['orders'][-1]['policy']=='MONTHLY','Historical policy snapshot')
            edit.update(version=info['version']+1,policy='MONTHLY')
            expect(200,f'/api/offices/{oid}',owner=a,data=edit)
            # Address eligibility and PIN access.
            gated=expect(201,'/api/offices',owner=a,data={'name':'PIN office','email':'pin@example.test','address':'Third floor','area':'Elsewhere','pin':'123456'})
            check(expect(401,'/api/office',office=gated['office_token'])['error']=='Office PIN required or incorrect',
                  'Missing PIN is explained without implying email verification failed')
            check(expect(401,'/api/office',office=gated['office_token'],pin='0000')['error']=='Office PIN required or incorrect',
                  'Incorrect PIN has the same specific error as missing PIN')
            check(not expect(200,'/api/office',office=gated['office_token'],pin='123456')['verified'],
                  'Correct PIN opens office onboarding before email verification')
            verify('pin@example.test',office=gated['office_token'],pin='123456')
            expect(409,'/api/orders',office=gated['office_token'],pin='123456',key='wrong-service-area',data={'item_id':first,'quantity':1})
            # Prepaid: no browser assertion alone can confirm a capture.
            prepaid=expect(201,'/api/offices',owner=a,data={'name':'Prepaid','email':'prepay@example.test','address':'Fourth floor','area':'Test Area'})
            pt=prepaid['office_token']
            verify('prepay@example.test',office=pt)
            pre=order(pt,[{'item_id':first,'quantity':1}],'prepaid-first')
            check(pre['status']=='PAYMENT_PENDING','Prepaid request cannot prepare')
            expect(409,f"/api/orders/{pre['id']}/status",owner=a,data={'status':'ACCEPTED','version':pre['version']})
            providers.drop_order=True
            expect(503,f"/api/orders/{pre['id']}/checkout",office=pt,data={})
            checkout=expect(200,f"/api/orders/{pre['id']}/checkout",office=pt,data={})
            check(providers.order_count==1,'Lost setup reply recovered by receipt without duplicate creation')
            pid=providers.capture(checkout['provider_order_id'],status='authorized')
            sig=hmac.new(b'isolated-one-secret',(checkout['provider_order_id']+'|'+pid).encode(),hashlib.sha256).hexdigest()
            expect(400,f"/api/orders/{pre['id']}/verify",office=pt,data={'payment_id':pid,'signature':'0'*64})
            expect(409,f"/api/orders/{pre['id']}/verify",office=pt,data={'payment_id':pid,'signature':sig})
            providers.payments[pid]['status']='captured'
            providers.payments[pid]['amount']=1
            expect(409,f"/api/orders/{pre['id']}/verify",office=pt,data={'payment_id':pid,'signature':sig})
            providers.payments[pid]['amount']=1500
            paid=expect(200,f"/api/orders/{pre['id']}/verify",office=pt,data={'payment_id':pid,'signature':sig})
            expect(200,f"/api/orders/{pre['id']}/verify",office=pt,data={'payment_id':pid,'signature':sig})
            check(paid['settlement']=='PAID' and paid['status']=='PLACED','Server capture enables seller acceptance')
            cancelled=state(a,paid,'CANCELLED')
            check(cancelled['settlement']=='REFUND_PENDING','Cancellation records refund liability')
            providers.drop_refund=True
            expect(503,f"/api/orders/{pre['id']}/refund",owner=a,data={})
            refunded=expect(200,f"/api/orders/{pre['id']}/refund",owner=a,data={})
            expect(200,f"/api/orders/{pre['id']}/refund",owner=a,data={})
            check(refunded['settlement']=='REFUNDED' and providers.refund_count==1,'Provider-idempotent refund after lost reply')
            # Late capture does not reopen an already cancelled request.
            late=order(pt,[{'item_id':first,'quantity':1}],'late-capture')
            checkout=expect(200,f"/api/orders/{late['id']}/checkout",office=pt,data={})
            state(a,late,'CANCELLED')
            late_pid=providers.capture(checkout['provider_order_id'])
            raw=json.dumps({'event':'payment.captured','payload':{'payment':{'entity':{'id':late_pid}}}})
            signature=hmac.new(b'isolated-webhook-one',raw.encode(),hashlib.sha256).hexdigest()
            expect(401,'/api/webhooks/1',data=raw,headers={'X-Razorpay-Signature':'bad'})
            for _ in range(2):expect(200,'/api/webhooks/1',data=raw,headers={'X-Razorpay-Signature':signature,'X-Razorpay-Event-Id':'evt-fixture-late'})
            late_saved=next(o for o in expect(200,'/api/orders',office=pt)['orders'] if o['id']==late['id'])
            check(late_saved['status']=='CANCELLED' and late_saved['settlement']=='REFUND_PENDING','Late capture preserved cancellation')
            expect(200,f"/api/orders/{late['id']}/refund",owner=a,data={})
            check(expect(200,'/api/account',office=pt)['ledger_balance_paise']==0,'Prepaid captures/refunds reconcile')
            # Create a remaining monthly debt before closing September.
            debt=fulfil(a,order(token,[{'item_id':first,'quantity':1}],'unpaid-september'))
            expect(409,f'/api/accounts/{oid}/invoices',owner=a,data={'period':'2026-09'})
            set_time('2026-10-02T06:00:00+00:00')
            a,b=login(0),login(1)
            invoice=expect(201,f'/api/accounts/{oid}/invoices',owner=a,data={'period':'2026-09'})['invoice_id']
            check(expect(200,f'/api/accounts/{oid}/invoices',owner=a,data={'period':'2026-09'})['invoice_id']==invoice,'Invoice period idempotency')
            statement=expect(200,'/api/invoices/'+invoice,office=token)
            check(statement['total_paise']==6600 and statement['outstanding_paise']==1500,'Invoice groups charges without double charging')
            expect(404,'/api/invoices/'+invoice,owner=b)
            set_time('2026-10-10T06:00:00+00:00')
            a=login(0)
            check(account(a,oid)['overdue'],'Overdue debt detected')
            expect(409,'/api/orders',office=token,key='overdue-credit-block',data={'item_id':first,'quantity':1})
            fallback=order(token,[{'item_id':first,'quantity':1}],'overdue-prepaid-fallback',mode='PREPAID')
            check(fallback['policy']=='PREPAID','Explicit prepaid fallback')
            expect(201,f'/api/accounts/{oid}/payments',owner=a,key='clear-overdue',data={'amount_paise':1500,'method':'CASH','reference':'receipt-september-final'})
            check(not account(a,oid)['overdue'],'Allocated payment clears overdue block')
            # Prior verification code cannot verify a changed contact address.
            challenge=expect(200,'/api/verification/request',office=token,data={})['challenge_id']
            old_code=providers.emails['office@example.test']
            info=expect(200,'/api/owner',owner=a)['offices'][0]
            edit.update(version=info['version'],email='new-office@example.test')
            expect(200,f'/api/offices/{oid}',owner=a,data=edit)
            expect(400,'/api/verification/confirm',office=token,data={'challenge_id':challenge,'code':old_code})
            verify('new-office@example.test',office=token)
            new_token=expect(200,f'/api/offices/{oid}/rotate',owner=a,data={})['office_token']
            expect(401,'/api/office',office=token)
            expect(200,'/api/office',office=new_token)
            # Provider configuration absence never creates a paid order.
            missing=order(new_token,[{'item_id':first,'quantity':1}],'configured-prepaid',mode='PREPAID')
            stop('order')
            start('order',custom={'CHAY_RAZORPAY_ACCOUNTS_FILE':''})
            expect(503,f"/api/orders/{missing['id']}/checkout",office=new_token,data={})
            expect(503,'/api/orders',office=pt,key='missing-provider-config',data={'item_id':first,'quantity':1})
            # The real recovery worker runs against the isolated provider only.
            stop('order');start('order')
            paging=expect(201,'/api/offices',owner=a,data={'name':'Pagination Office','email':'paging@example.test','address':'Fifth floor','area':'Test Area','policy':'MONTHLY','credit_limit_paise':100000000})
            paging_token=paging['office_token']
            verify('paging@example.test',office=paging_token)
            for n in range(51):order(paging_token,[{'item_id':first,'quantity':1}],f'paging-intent-{n}')
            page_one=expect(200,'/api/orders',office=paging_token)
            page_two=expect(200,'/api/orders?before='+str(page_one['next_cursor']),office=paging_token)
            check(len(page_one['orders'])==50 and len(page_two['orders'])==1,'Keyset pages include all equal-time orders')
            check(not ({o['id'] for o in page_one['orders']}&{o['id'] for o in page_two['orders']}),'No duplicate orders across pages')
            expect(400,'/api/orders?before=invalid',office=paging_token)
            original_config=config.read_text()
            live_config=json.loads(original_config)
            live_config['1']['key_id']='rzp_live_deliberately_rejected'
            config.write_text(json.dumps(live_config))
            expect(503,'/api/orders',office=pt,key='reject-live-key',data={'item_id':first,'quantity':1})
            config.write_text(original_config)
            automatic=order(pt,[{'item_id':first,'quantity':1}],'worker-recovery')
            automatic_checkout=expect(200,f"/api/orders/{automatic['id']}/checkout",office=pt,data={})
            providers.capture(automatic_checkout['provider_order_id'])
            set_time('2026-10-10T06:02:00+00:00')
            stop('order');start('order',custom={'CHAY_RECONCILE_WORKER':'1'})
            def wait_settlement(expected):
                deadline=time.monotonic()+25
                while time.monotonic()<deadline:
                    result=request('/api/orders',office=pt)[1]
                    match=next(o for o in result['orders'] if o['id']==automatic['id'])
                    if match['settlement']==expected:return match
                    time.sleep(.15)
                raise AssertionError('Recovery worker did not reach '+expected)
            auto_paid=wait_settlement('PAID')
            check(auto_paid['status']=='PLACED','Worker recovers capture without browser callback')
            state(a,auto_paid,'CANCELLED')
            set_time('2026-10-10T06:04:00+00:00')
            check(wait_settlement('REFUNDED')['status']=='CANCELLED','Worker processes durable refund recovery')
            stop('order');start('order')
            # No duplicate legacy migration charges on restart.
            # Discoverable stalls and owner-approved browser access, no office email/PIN.
            a,b=login(0),login(1)  # The earlier controlled-clock scenarios advance past session expiry.
            directory=expect(200,'/api/stalls')['stalls']
            check(len(directory)==2 and all(set(s)=={'id','name','area'} for s in directory),
                  'Public directory exposes only stall identity and area')
            easy=expect(201,'/api/offices',owner=a,data={'name':'Easy Office','address':'Sixth floor','area':'Test Area','policy':'MONTHLY','credit_limit_paise':10000})
            access=expect(201,'/api/access/request',data={'stall_id':1,'office_name':'Easy Office'})
            device=access['device_token']
            check(expect(200,'/api/access',office=device)['status']=='PENDING','New browser waits for owner approval')
            expect(401,'/api/office',office=device)
            expect(401,'/api/orders',office=device,key='pending-device',data={'item_id':first,'quantity':1})
            expect(401,'/api/access/requests')
            queue=expect(200,'/api/access/requests',owner=a)['requests']
            rid=next(r['id'] for r in queue if r['office_name']=='Easy Office')
            expect(404,'/api/access/requests/'+rid,owner=b,data={'status':'APPROVED','office_id':easy['id']})
            expect(404,'/api/access/requests/'+rid,owner=a,data={'status':'APPROVED','office_id':999999})
            foreign_office=expect(201,'/api/offices',owner=b,data={'name':'Foreign office','address':'Other floor','area':'Test Area'})
            expect(404,'/api/access/requests/'+rid,owner=a,data={'status':'APPROVED','office_id':foreign_office['id']})
            expect(200,'/api/access/requests/'+rid,owner=a,data={'status':'APPROVED','office_id':easy['id']})
            connected=expect(200,'/api/office',office=device)
            check(connected['owner_approved'] and connected['verified'] and not connected['email_verified'] and connected['email']=='',
                  'Owner approval is separate from email verification and needs no email')
            easy_order=order(device,[{'item_id':first,'quantity':1}],'approved-device-order')
            check(easy_order['office_id']==easy['id'],'Approved browser derives its assigned office server-side')
            stop('merchant');start('merchant')
            check(expect(200,'/api/access',office=device)['status']=='APPROVED','Browser approval survives restart')
            expect(200,'/api/access/requests/'+rid,owner=a,data={'status':'REVOKED'})
            expect(401,'/api/office',office=device)
            expect(401,'/api/account',office=device)
            expect(401,'/api/orders',office=device,key='revoked-device',data={'item_id':first,'quantity':1})
            expect(409,'/api/access/requests/'+rid,owner=a,data={'status':'APPROVED','office_id':easy['id']})
            expect(404,'/api/access/request',data={'stall_id':999999,'office_name':'Invalid stall'})
            expect(200,'/portal.js')
            with sqlite3.connect(data/'order.db') as db:
                check(db.execute("SELECT COUNT(*) FROM ledger WHERE id LIKE 'legacy_%'").fetchone()[0]==0,'Migration does not duplicate new charges')
                for table in ('ledger','allocations','invoices','audit'):
                    try:db.execute(f'UPDATE {table} SET rowid=rowid')
                    except sqlite3.DatabaseError:check(True,table+' immutable')
                    else:raise AssertionError(table+' unexpectedly mutable')
                before=db.execute('SELECT COUNT(*) FROM orders').fetchone()[0]
                check(db.execute('SELECT COUNT(*) FROM webhook_events').fetchone()[0]==1,'Webhook replay deduplicated')
            # Backup/restore is offline and restores into a new directory only.
            stop('order');stop('merchant')
            backup=temp/'backup'
            restore=temp/'restore'
            subprocess.run(['python3',str(ROOT/'scripts/backup.py'),'backup','--data-dir',str(data),'--output',str(backup)],check=True,capture_output=True)
            subprocess.run(['python3',str(ROOT/'scripts/backup.py'),'restore','--data-dir',str(restore),'--source',str(backup)],check=True,capture_output=True)
            with sqlite3.connect(restore/'order.db') as db:check(db.execute('SELECT COUNT(*) FROM orders').fetchone()[0]==before,'Backup/restore preserves orders')
            refusal=subprocess.run(['python3',str(ROOT/'scripts/backup.py'),'restore','--data-dir',str(data),'--source',str(backup)],capture_output=True)
            check(refusal.returncode!=0,'Restore refuses existing data')
            start('merchant',folder=restore);start('order',folder=restore)
            expect(200,'/api/orders',owner=a)
            stop('merchant')
            expect(503,'/api/orders',owner=a)
            print(f'{count} Phase 1/2 HTTP, concurrency, provider-contract and recovery checks passed.')
            print('All databases and provider fixtures were disposable. No external mail, money or provider calls.')
        except Exception:
            for file in temp.glob('*.log'):print(file.name,file.read_text()[-6000:])
            raise
        finally:
            for name in list(processes):stop(name)
            for log in logs:log.close()
            providers.close()

if __name__=='__main__':main()
