const assert=require('node:assert/strict');
const fs=require('node:fs');
const vm=require('node:vm');
function setup(status){
  const elements=new Map(),storage=new Map([['chayDevice','synthetic-browser-token']]);
  const make=()=>({hidden:true,value:'',children:[],append(...items){this.children.push(...items)},replaceChildren(...items){this.children=items},setAttribute(){},focus(){}});
  const get=id=>{if(!elements.has(id))elements.set(id,make());return elements.get(id)};
  const c={officeToken:'synthetic-browser-token',officePin:'',ownerToken:'',sellerState:{offices:[]},
    $:get,localStorage:{getItem:k=>storage.get(k)||null,setItem:(k,v)=>storage.set(k,v),removeItem:k=>storage.delete(k)},
    api:async()=>({status}),office:async()=>{c.opened=true},message:t=>{c.messageText=t},
    setInterval:()=>1,clearInterval:()=>{c.cleared=true},document:{hidden:false,addEventListener:(_,fn)=>{c.ready=fn},createElement:make},
    node:(_,text)=>Object.assign(make(),{textContent:text}),action:(text,fn)=>({textContent:text,onclick:fn}),bind:(id,fn)=>{c[id]=fn},
    location:{href:'',reload:()=>{c.reloaded=true}},confirm:()=>true};
  vm.createContext(c);vm.runInContext(fs.readFileSync(__dirname+'/../frontend/portal.js','utf8'),c);c.ready();return c;
}
(async()=>{
  let c=setup('PENDING');await c.resumeOffice();
  assert.equal(c.opened,undefined);assert.equal(c.$('accessPending').hidden,false);
  c.api=async()=>({status:'APPROVED'});await c.resumeOffice();
  assert.equal(c.opened,true);assert.equal(c.cleared,true);assert.equal(c.$('discovery').hidden,true);
  c=setup('REVOKED');await c.resumeOffice();assert.equal(c.opened,undefined);assert.equal(c.$('discovery').hidden,false);
  c=setup();c.api=async()=>{throw Error('Office access not found')};await c.resumeOffice();
  assert.equal(c.$('discovery').hidden,false);assert.equal(c.$('pinGate').hidden,true);
  c=setup();c.localStorage.removeItem('chayDevice');c.office=async()=>{throw Error('Office link invalid or revoked')};await c.resumeOffice();
  assert.equal(c.$('pinGate').hidden,true);assert.equal(c.$('discovery').hidden,false);
  c=setup();c.localStorage.removeItem('chayDevice');c.office=async()=>{throw Error('Office PIN required or incorrect')};await c.resumeOffice();
  assert.equal(c.$('pinGate').hidden,false);
  c.$('ownerEntry').onclick();assert.equal(c.location.href,'/#owner');assert.equal(c.reloaded,true);
  console.log('7 office browser UI scenarios passed (synthetic storage and API).');
})().catch(e=>{console.error(e);process.exitCode=1});
