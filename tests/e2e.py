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
            # Growth, private conversations and explicit recurring consent.
            expect(200,'/communication.js')
            expect(200,'/api/public-profile',owner=a,data={'description':'Fresh tea','terms':'Pickup or agreed delivery','contact':'Business desk','photo_url':''})
            profile=expect(200,'/api/public/stalls/1')
            check(profile['description']=='Fresh tea' and 'offices' not in profile and 'email' not in profile,'Public profile contains no private office data')
            expect(400,'/api/public-profile',owner=a,data={'photo_url':'javascript:alert(1)'})
            expect(401,'/api/conversations/0')
            mid=expect(200,'/api/conversations/0',office=device,data={'body':'Please deliver to reception','request_id':'message-one'})['id']
            check(expect(200,'/api/conversations/0',office=device,data={'body':'Please deliver to reception','request_id':'message-one'})['id']==mid,'Message retries are deduplicated')
            expect(409,'/api/conversations/0',office=device,data={'body':'Changed','request_id':'message-one'})
            expect(404,f"/api/conversations/{easy['id']}",owner=b)
            expect(404,f"/api/conversations/{foreign_office['id']}",office=device)
            messages=expect(200,f"/api/conversations/{easy['id']}",owner=a)['messages']
            check(len(messages)==1 and messages[0]['body']=='Please deliver to reception','Owner sees only the scoped office thread')
            expect(200,f"/api/conversations/{easy['id']}/read",owner=a,data={'last_id':mid})
            check(expect(200,'/api/conversations/0',office=device)['other_read_through']==mid,'Explicit read acknowledgement visible to office')
            expect(200,f"/api/conversations/{easy['id']}",owner=a,data={'body':'Confirmed, reception','request_id':'reply-one'})
            check(len(expect(200,'/api/conversations/0',office=device)['messages'])==2,'Office receives owner reply')
            now_epoch=int(clock.read_text()); india=dt.datetime.fromtimestamp(now_epoch+19800,dt.timezone.utc)
            recurring={'items':[{'item_id':first,'quantity':1}],'days':127,'minute':india.hour*60+india.minute,'maximum_total_paise':5000,'consent':True}
            expect(400,'/api/rounds',office=device,data={**recurring,'consent':False})
            round_id=expect(201,'/api/rounds',office=device,data=recurring)['id']
            expect(404,'/api/rounds/'+round_id,office=foreign_office['office_token'],data={'paused':True})
            tick=dict(url=f"http://127.0.0.1:{ports['order']}",headers={'X-Internal-Key':env['CHAY_INTERNAL_KEY']},data={})
            expect(401,'/internal/rounds/tick',url=tick['url'],data={})
            expect(200,'/internal/rounds/tick',**tick)
            expect(200,'/internal/rounds/tick',**tick)
            with sqlite3.connect(data/'order.db') as db:
                check(db.execute("SELECT COUNT(*) FROM orders WHERE idem=?",('round-'+round_id+'-'+str((now_epoch+19800)//86400),)).fetchone()[0]==1,'Recurring worker retry creates one order')
            expect(200,'/api/rounds/'+round_id,office=device,data={'paused':True})
            bad_round=expect(201,'/api/rounds',office=device,data={**recurring,'maximum_total_paise':1})['id']
            expect(200,'/internal/rounds/tick',**tick)
            result=expect(200,'/api/rounds',office=device)['rounds']
            check(next(r for r in result if r['id']==bad_round)['last_result']=='Price exceeds approved recurring budget','Price ceiling prevents unapproved recurring spending')
            expect(200,'/api/rounds/'+bad_round,office=device,data={'cancelled':True})
            expect(409,'/api/rounds/'+bad_round,office=device,data={'paused':False})
            skipped=expect(201,'/api/rounds',office=device,data=recurring)['id']
            expect(200,'/api/rounds/'+skipped,office=device,data={'skip_day':(now_epoch+19800)//86400})
            expect(200,'/internal/rounds/tick',**tick)
            check(next(r for r in expect(200,'/api/rounds',office=device)['rounds'] if r['id']==skipped)['last_result']=='Skipped by office','Explicit skipped round creates no unwanted order')
            done=fulfil(a,easy_order)
            expect(200,f"/api/orders/{done['id']}/feedback",office=device,data={'rating':4,'comment':'Good tea'})
            expect(409,f"/api/orders/{done['id']}/feedback",office=device,data={'rating':1,'comment':'Changed'})
            expect(409,f"/api/orders/{done['id']}/feedback",office=foreign_office['office_token'],data={'rating':5})
            # Marketing is in-app, opt-in, paused by default and frequency capped.
            campaign={'body':'Welcome to our tea desk','request_id':'campaign-one','maximum_recipients':10,'approved':True}
            expect(409,'/api/campaigns',owner=a,data=campaign)
            expect(200,'/api/marketing-controls',owner=a,data={'paused':False,'weekly_cap':1})
            check(not expect(200,'/api/marketing-consent',office=device)['enabled'],'Marketing defaults to no consent')
            check(expect(200,'/api/campaigns',owner=a,data=campaign)['stored_messages']==0,'No update is sent without consent')
            expect(200,'/api/marketing-consent',office=device,data={'enabled':True})
            campaign['request_id']='campaign-two'
            check(expect(200,'/api/campaigns',owner=a,data=campaign)['stored_messages']==1,'Consenting office receives one in-app update')
            check(expect(200,'/api/campaigns',owner=a,data=campaign)['stored_messages']==1,'Campaign retry reuses deliveries')
            campaign['request_id']='campaign-three'
            check(expect(200,'/api/campaigns',owner=a,data=campaign)['stored_messages']==0,'Rolling weekly cap suppresses excess marketing')
            expect(200,'/api/marketing-consent',office=device,data={'enabled':False})
            expect(200,f"/api/accounts/{easy['id']}/remind",owner=a,key='reminder-one',data={})
            expect(200,f"/api/accounts/{easy['id']}/remind",owner=a,key='reminder-one',data={})
            expect(429,f"/api/accounts/{easy['id']}/remind",owner=a,key='reminder-two',data={})
            # Financial operations are blocked until a review is recorded; fixtures use a synthetic review.
            topup={'kind':'TOPUP','amount_paise':4500,'method':'BANK','reference':'advance-bank-one','confirmed':True}
            expect(503,f"/api/accounts/{easy['id']}/advance",owner=a,key='advance-topup-one',data=topup)
            stop('order');start('order',custom={'CHAY_FINANCIAL_REVIEW_ID':'synthetic-fixture-review'})
            expect(200,f"/api/accounts/{easy['id']}/advance",owner=a,key='advance-topup-one',data=topup)
            expect(200,f"/api/accounts/{easy['id']}/advance",owner=a,key='advance-topup-one',data=topup)
            check(expect(200,'/api/advance',office=device)['balance_paise']==4500,'Top-up retry credits funds once')
            expect(409,f"/api/accounts/{easy['id']}/payments",owner=a,key='no-double-reference',data={'amount_paise':1,'method':'BANK','reference':'advance-bank-one'})
            def advance_race(n):return request('/api/orders',office=device,key=f'advance-race-{n}',data={'items':[{'item_id':first,'quantity':2}],'payment_mode':'ADVANCE'})
            with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool: raced=list(pool.map(advance_race,range(2)))
            check(sorted(status for status,_ in raced)==[201,409],'Concurrent orders cannot over-reserve advance funds')
            advance_order=next(value for status,value in raced if status==201)
            check(expect(200,'/api/advance',office=device)['available_paise']==1500,'Advance reservation reduces spendable funds')
            expect(409,f"/api/accounts/{easy['id']}/advance",owner=a,key='advance-oversized-refund',data={**topup,'kind':'REFUND','amount_paise':2000,'reference':'too-large-refund'})
            advance_order=fulfil(a,advance_order)
            check(advance_order['settlement']=='PAID' and expect(200,'/api/advance',office=device)['balance_paise']==1500,'Fulfilment deducts advance and balances the paid order')
            cancelled=order(device,[{'item_id':first,'quantity':1}],'advance-cancellation','ADVANCE')
            state(a,cancelled,'CANCELLED')
            check(expect(200,'/api/advance',office=device)['available_paise']==1500,'Cancellation releases advance reservation')
            expect(200,f"/api/accounts/{easy['id']}/advance",owner=a,key='advance-refund-one',data={**topup,'kind':'REFUND','amount_paise':1500,'reference':'refund-bank-one'})
            check(expect(200,'/api/advance',office=device)['balance_paise']==0,'Confirmed refund reduces advance balance')
            referred=expect(201,'/api/offices',owner=a,data={'name':'Referral New Office','address':'Unique new building','area':'Test Area','policy':'MONTHLY','credit_limit_paise':10000})
            join=expect(201,'/api/access/request',data={'stall_id':1,'office_name':'Referral New Office'})
            new_request=next(x for x in expect(200,'/api/access/requests',owner=a)['requests'] if x['office_name']=='Referral New Office')
            expect(200,'/api/access/requests/'+new_request['id'],owner=a,data={'status':'APPROVED','office_id':referred['id']})
            expect(200,f"/api/accounts/{referred['id']}/referral",owner=a,data={'referrer_office_id':easy['id'],'identity_checked':True})
            expect(400,f"/api/accounts/{easy['id']}/referral",owner=a,data={'referrer_office_id':easy['id'],'identity_checked':True})
            expect(200,'/api/reward-rules',owner=a,data={'reward_paise':500,'budget_paise':500,'paused':False})
            expect(409,f"/api/accounts/{referred['id']}/referral-reward",owner=a,data={})
            qualified=fulfil(a,order(join['device_token'],[{'item_id':first,'quantity':1}],'referral-qualifying-order'))
            expect(201,f"/api/accounts/{referred['id']}/payments",owner=a,key='referral-paid-receipt',data={'amount_paise':1500,'method':'CASH','reference':'referral-receipt'})
            expect(200,f"/api/accounts/{referred['id']}/referral-reward",owner=a,data={})
            expect(200,f"/api/accounts/{referred['id']}/referral-reward",owner=a,data={})
            check(expect(200,'/api/advance',office=device)['balance_paise']==500,'Referral reward is credited exactly once')
            expect(409,f"/api/accounts/{easy['id']}/advance",owner=a,key='reward-no-cashout',data={**topup,'kind':'REFUND','amount_paise':500,'reference':'no-reward-cashout'})
            expect(200,f"/api/orders/{qualified['id']}/loyalty",office=join['device_token'],data={})
            expect(200,f"/api/orders/{qualified['id']}/loyalty",office=join['device_token'],data={})
            with sqlite3.connect(data/'order.db') as db:
                check(db.execute('SELECT COUNT(*) FROM loyalty_points WHERE order_id=?',(qualified['id'],)).fetchone()[0]==1,'Loyalty claim is idempotent')
            current_office=expect(200,'/api/office',office=device)
            expect(200,f"/api/offices/{easy['id']}/billing-cycle",owner=a,data={'cycle':'DAILY','reason':'Agreed daily terms','version':current_office['version']})
            expect(409,f"/api/offices/{easy['id']}/billing-cycle",owner=a,data={'cycle':'WEEKLY','reason':'Stale terms','version':current_office['version']})
            # Offer budgets, capacity, notice and settlement statements use server snapshots.
            offer={'price_paise':1000,'cost_paise':800,'minimum_margin_paise':100,'maximum_units':1,'discount_budget_paise':500,'starts_at':now_epoch-60,'ends_at':now_epoch+3600,'notice_minutes':0,'paused':False}
            expect(409,f'/api/menu/{first}/promotion',owner=a,data={**offer,'minimum_margin_paise':300})
            expect(409,f'/api/menu/{first}/promotion',owner=a,data={**offer,'discount_budget_paise':100})
            expect(200,f'/api/menu/{first}/promotion',owner=a,data=offer)
            expect(409,'/api/orders',office=device,key='over-offer-capacity',data={'items':[{'item_id':first,'quantity':2}]})
            discounted=order(device,[{'item_id':first,'quantity':1}],'offer-one')
            check(discounted['total_paise']==1000,'Offer uses a server-calculated discounted price snapshot')
            expect(409,'/api/orders',office=device,key='offer-exhausted',data={'items':[{'item_id':first,'quantity':1}]})
            state(a,discounted,'CANCELLED')
            expect(200,f'/api/menu/{first}/promotion',owner=a,data={**offer,'notice_minutes':60})
            expect(409,'/api/orders',office=device,key='pack-without-notice',data={'items':[{'item_id':first,'quantity':1}]})
            pack=expect(201,'/api/orders',office=device,key='pack-with-notice',data={'items':[{'item_id':first,'quantity':1}],'delivery_at':now_epoch+7200})
            state(a,pack,'CANCELLED')
            expect(200,f'/api/menu/{first}/promotion',owner=a,data={**offer,'paused':True})
            daily_order=fulfil(a,order(device,[{'item_id':first,'quantity':1}],'daily-cycle-order'))
            day_label=india.strftime('%Y-%m-%d')
            set_time(dt.datetime.fromtimestamp(now_epoch+3*86400,dt.timezone.utc).isoformat());a,b=login(0),login(1)
            before_statement=account(a,easy['id'])['outstanding_paise']
            issued=expect(201,f"/api/accounts/{easy['id']}/settlement-statement",owner=a,data={'cycle':'DAILY','start_date':day_label})
            check(expect(200,f"/api/accounts/{easy['id']}/settlement-statement",owner=a,data={'cycle':'DAILY','start_date':day_label})['invoice_id']==issued['invoice_id'],'Daily statement issuance is idempotent')
            check(account(a,easy['id'])['outstanding_paise']==before_statement,'Daily statement does not create duplicate debt')
            expect(400,f"/api/accounts/{easy['id']}/settlement-statement",owner=a,data={'cycle':'DAILY','start_date':'2026-02-31'})
            expect(409,'/api/orders',office=device,key='overdue-daily-order',data={'items':[{'item_id':first,'quantity':1}]})
            expect(200,'/internal/reminders/tick',**tick)
            expect(200,'/internal/reminders/tick',**tick)
            with sqlite3.connect(data/'merchant.db') as db:
                check(db.execute("SELECT COUNT(*) FROM messages WHERE office_id=? AND sender='SYSTEM' AND idem LIKE 'auto-billing-%'",(easy['id'],)).fetchone()[0]==1,'Automatic overdue reminder is stored once per day')
            expect(201,f"/api/accounts/{easy['id']}/payments",owner=a,key='daily-balance-payment',data={'amount_paise':before_statement,'method':'BANK','reference':'daily-cleared'})
            check(not account(a,easy['id'])['overdue'],'Confirmed settlement clears overdue credit block')
            expect(200,'/api/stall/capacity',owner=a,data={'daily_units':1})
            expect(409,'/api/orders',office=device,key='stall-capacity-full',data={'items':[{'item_id':first,'quantity':2}]})
            expect(200,'/api/stall/capacity',owner=a,data={'daily_units':10000})
            today=(int(clock.read_text())+19800)//86400
            expect(200,'/api/stall/holidays',owner=a,data={'india_day':today,'closed':True})
            check(not expect(200,'/api/office',office=device)['ordering_open'],'Holiday closes office ordering')
            expect(409,'/api/orders',office=device,key='holiday-order-blocked',data={'items':[{'item_id':first,'quantity':1}]})
            expect(200,'/api/stall/holidays',owner=a,data={'india_day':today,'closed':False})
            expect(200,'/api/marketing-automation',owner=a,data={'welcome':'Welcome to the office tea desk','reactivation':'Our current menu is ready when you need it','inactive_days':7,'paused':False,'approved':True})
            expect(200,'/api/marketing-consent',office=join['device_token'],data={'enabled':True})
            expect(200,'/internal/marketing/tick',**tick)
            expect(200,'/internal/marketing/tick',**tick)
            with sqlite3.connect(data/'merchant.db') as db:
                check(db.execute("SELECT COUNT(*) FROM automatic_marketing_deliveries WHERE office_id=? AND kind='WELCOME'",(referred['id'],)).fetchone()[0]==1,'Welcome automation sends once with consent')
            set_time(dt.datetime.fromtimestamp(now_epoch+10*86400,dt.timezone.utc).isoformat());a,b=login(0),login(1)
            expect(200,'/internal/marketing/tick',**tick)
            expect(200,'/internal/marketing/tick',**tick)
            with sqlite3.connect(data/'merchant.db') as db:
                check(db.execute("SELECT COUNT(*) FROM automatic_marketing_deliveries WHERE office_id=? AND kind='REACTIVATION'",(referred['id'],)).fetchone()[0]==1,'Inactive office reactivation is deduplicated and frequency capped')
            expect(200,'/api/marketing-consent',office=join['device_token'],data={'enabled':False})
            # Team roles are checked by Merchant for both direct and Order-service actions.
            expect(200,'/api/workers',owner=a,data={'email':'owner1@example.test','role':'FULFILMENT'})
            expect(200,'/api/workspaces/switch',owner=b,data={'workspace_id':1})
            expect(200,'/api/orders',owner=b)
            expect(403,'/api/menu',owner=b,data={'name':'Forbidden edit','price_paise':100})
            expect(403,'/api/reward-rules',owner=b,data={'reward_paise':1,'budget_paise':1,'paused':False})
            expect(403,f"/api/accounts/{easy['id']}/payments",owner=b,key='forbidden-worker-receipt',data={'amount_paise':1,'method':'CASH','reference':'forbidden'})
            expect(403,'/api/branches',owner=b,data={'name':'Forbidden branch'})
            expect(403,'/api/workers',owner=b,data={'email':'owner1@example.test','role':'MANAGER'})
            pending_order=next(x for x in expect(200,'/api/orders',owner=b)['orders'] if x['office_id']==easy['id'] and x['status']=='PLACED')
            state(b,pending_order,'ACCEPTED')
            expect(200,'/api/workers',owner=a,data={'email':'owner1@example.test','role':'REVOKED'})
            expect(401,'/api/orders',owner=b)
            b=login(1)
            check(expect(200,'/api/owner',owner=b)['stall_id']==2,'Revoked worker can return to their own stall after signing in')
            branch=expect(201,'/api/branches',owner=a,data={'name':'Second branch'})['workspace_id']
            expect(403,'/api/workspaces/switch',owner=b,data={'workspace_id':branch})
            expect(200,'/api/workspaces/switch',owner=a,data={'workspace_id':branch})
            check(expect(200,'/api/orders',owner=a)['orders']==[],'New branch has an isolated order queue')
            expect(404,f"/api/accounts/{easy['id']}",owner=a)
            branch_item=expect(201,'/api/menu',owner=a,data={'name':'Branch Tea','price_paise':1000})['id']
            settings(a)
            expect(200,f'/api/menu/{branch_item}/cost',owner=a,data={'cost_paise':300})
            branch_office=expect(201,'/api/offices',owner=a,data={'name':'Branch Office','address':'Branch building','area':'Test Area','policy':'MONTHLY','credit_limit_paise':10000})
            branch_access=expect(201,'/api/access/request',data={'stall_id':3,'office_name':'Branch Office','source':'qa-source'})
            branch_request=expect(200,'/api/access/requests',owner=a)['requests'][0]
            expect(200,'/api/access/requests/'+branch_request['id'],owner=a,data={'status':'APPROVED','office_id':branch_office['id']})
            check('internal_costs' not in expect(200,'/api/office',office=branch_access['device_token']),'Private cost quote is never exposed to the office browser')
            branch_order=fulfil(a,order(branch_access['device_token'],[{'item_id':branch_item,'quantity':1}],'branch-cost-snapshot'))
            expect(201,f"/api/accounts/{branch_office['id']}/payments",owner=a,key='branch-paid-receipt',data={'amount_paise':1000,'method':'CASH','reference':'branch-receipt'})
            expect(409,f"/api/accounts/{branch_office['id']}/advance",owner=a,key='no-double-bank-receipt',data={'kind':'TOPUP','amount_paise':1000,'method':'BANK','reference':'branch-receipt','confirmed':True})
            expense={'kind':'MARKETING','source':'qa-source','amount_paise':100,'reference':'branch-poster-cost'}
            expense_id=expect(200,'/api/business-expenses',owner=a,data=expense)['id']
            expect(200,'/api/business-expenses',owner=a,data=expense)
            branch_report=expect(200,'/api/growth-report',owner=a)
            check(branch_report['contribution_available'] and branch_report['contribution_estimate_paise']==600,'Contribution uses net sales minus cost snapshots and actual expenses once')
            check(branch_report['sources'][0]['acquisition_cost_paise']==100,'Source report divides recorded acquisition spending by paying offices')
            expect(200,'/api/business-expenses/'+expense_id+'/reverse',owner=a,data={'reason':'Incorrect recorded spend'})
            expect(200,'/api/business-expenses/'+expense_id+'/reverse',owner=a,data={'reason':'Retry correction'})
            check(expect(200,'/api/growth-report',owner=a)['contribution_estimate_paise']==700,'Expense reversal corrects reporting once without deleting history')

            expect(200,'/api/workspaces/switch',owner=a,data={'workspace_id':1})
            report=expect(200,'/api/growth-report',owner=a)
            check(any(f['order_id']==done['id'] for f in report['feedback']),'Owner report includes completed-order feedback')
            stop('merchant');start('merchant')
            check(expect(200,'/api/access',office=device)['status']=='APPROVED','Browser approval survives restart')
            expect(200,'/api/access/requests/'+rid,owner=a,data={'status':'REVOKED'})
            expect(401,'/api/office',office=device)
            expect(401,'/api/account',office=device)
            expect(401,'/api/conversations/0',office=device)
            expect(401,'/api/rounds',office=device)
            expect(401,'/api/orders',office=device,key='revoked-device',data={'item_id':first,'quantity':1})
            expect(409,'/api/access/requests/'+rid,owner=a,data={'status':'APPROVED','office_id':easy['id']})
            expect(404,'/api/access/request',data={'stall_id':999999,'office_name':'Invalid stall'})
            expect(200,'/portal.js')
            with sqlite3.connect(data/'order.db') as db:
                check(db.execute("SELECT COUNT(*) FROM ledger WHERE id LIKE 'legacy_%'").fetchone()[0]==0,'Migration does not duplicate new charges')
                for table in ('ledger','allocations','invoices','audit','advance_entries','business_expenses'):
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
            print(f'{count} ChayRound HTTP, money, permissions, automation, provider-contract and recovery checks passed.')
            print('All databases and provider fixtures were disposable. No external mail, money or provider calls.')
        except Exception:
            for file in temp.glob('*.log'):print(file.name,file.read_text()[-6000:])
            raise
        finally:
            for name in list(processes):stop(name)
            for log in logs:log.close()
            providers.close()

if __name__=='__main__':main()
