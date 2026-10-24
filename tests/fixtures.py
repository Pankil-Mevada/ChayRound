"""Isolated provider contracts. Imported only by tests; never served by the app."""
import base64
import json
import re
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlsplit, parse_qs

class Providers:
    def __init__(self):
        self.lock=threading.RLock()
        self.emails={}
        self.mailbox_file=None
        self.orders={}
        self.payments={}
        self.refunds={}
        self.refund_keys={}
        self.drop_order=False
        self.drop_refund=False
        self.order_count=0
        self.refund_count=0
        state=self
        class Handler(BaseHTTPRequestHandler):
            def log_message(self,*args): pass
            def respond(self,status,value):
                raw=json.dumps(value).encode()
                self.send_response(status)
                self.send_header('Content-Type','application/json')
                self.send_header('Content-Length',str(len(raw)))
                self.end_headers()
                self.wfile.write(raw)
            def do_POST(self): self.handle_api()
            def do_GET(self): self.handle_api()
            def handle_api(self):
                with state.lock:
                    url=urlsplit(self.path)
                    data=json.loads(self.rfile.read(int(self.headers.get('Content-Length','0'))) or b'{}')
                    if url.path=='/emails':
                        if self.headers.get('Authorization')!='Bearer isolated-email-secret':
                            return self.respond(401,{'error':'unauthorized'})
                        recipient=data['to'][0]
                        state.emails[recipient]=re.search(r'\b\d{6}\b',data['text']).group()
                        if state.mailbox_file:
                            state.mailbox_file.write_text(json.dumps(state.emails))
                        return self.respond(200,{'id':'email_fixture'})
                    raw=self.headers.get('Authorization','')
                    auth=base64.b64decode(raw[6:]).decode() if raw.startswith('Basic ') else ''
                    if auth not in ('rzp_test_one:isolated-one-secret','rzp_test_two:isolated-two-secret'):
                        return self.respond(401,{'error':'unauthorized'})
                    account=auth.split(':')[0]
                    path=url.path.removeprefix('/v1')
                    if path=='/orders' and self.command=='POST':
                        state.order_count+=1
                        order={'id':f'order_fixture_{state.order_count}','amount':data['amount'],
                               'currency':data['currency'],'receipt':data['receipt'],'account':account}
                        state.orders[order['id']]=order
                        if state.drop_order:
                            state.drop_order=False
                            self.close_connection=True
                            return
                        return self.respond(200,order)
                    if path=='/orders':
                        receipt=parse_qs(url.query).get('receipt',[''])[0]
                        return self.respond(200,{'items':[o for o in state.orders.values() if o['account']==account and o['receipt']==receipt]})
                    if path.startswith('/orders/') and path.endswith('/payments'):
                        oid=path.split('/')[2]
                        return self.respond(200,{'items':[p for p in state.payments.values() if p['order_id']==oid and state.orders[oid]['account']==account]})
                    if path.startswith('/payments/') and path.endswith('/refund'):
                        pid=path.split('/')[2]
                        payment=state.payments.get(pid)
                        if not payment or state.orders[payment['order_id']]['account']!=account:
                            return self.respond(404,{'error':'missing'})
                        key=(account,self.headers.get('X-Refund-Idempotency'))
                        if key in state.refund_keys:
                            return self.respond(200,state.refunds[state.refund_keys[key]])
                        state.refund_count+=1
                        refund={'id':f'rfnd_fixture_{state.refund_count}','payment_id':pid,
                                'amount':data['amount'],'status':'processed'}
                        state.refunds[refund['id']]=refund
                        state.refund_keys[key]=refund['id']
                        payment['amount_refunded']=data['amount']
                        if state.drop_refund:
                            state.drop_refund=False
                            self.close_connection=True
                            return
                        return self.respond(200,refund)
                    if path.startswith('/payments/'):
                        payment=state.payments.get(path.split('/')[2])
                        if payment and state.orders[payment['order_id']]['account']==account:
                            return self.respond(200,payment)
                    if path.startswith('/refunds/'):
                        refund=state.refunds.get(path.split('/')[2])
                        if refund and state.orders[state.payments[refund['payment_id']]['order_id']]['account']==account:
                            return self.respond(200,refund)
                    return self.respond(404,{'error':'missing'})
        self.server=ThreadingHTTPServer(('127.0.0.1',0),Handler)
        self.thread=threading.Thread(target=self.server.serve_forever,daemon=True)
        self.thread.start()
        self.base=f'http://127.0.0.1:{self.server.server_port}'
    def capture(self,oid,status='captured'):
        with self.lock:
            order=self.orders[oid]
            pid=f'pay_fixture_{len(self.payments)+1}'
            self.payments[pid]={'id':pid,'order_id':oid,'amount':order['amount'],
                                'currency':order['currency'],'status':status,'amount_refunded':0}
            return pid
    def close(self):
        self.server.shutdown()
        self.server.server_close()
        self.thread.join(timeout=3)
