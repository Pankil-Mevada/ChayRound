'use strict';
let availableStalls=[], chosenStall=null, accessTimer=null;
async function startDiscovery(){
  $('auth').hidden=true; $('discovery').hidden=false;
  $('heading').textContent='Your usual chai. A few taps away.';
  availableStalls=(await api('stalls')).stalls;
  renderStalls();
}
function renderStalls(){
  const query=$('stallSearch').value.trim().toLowerCase();
  const box=$('stallResults'); box.replaceChildren();
  const matches=availableStalls.filter(s=>(s.name+' '+s.area).toLowerCase().includes(query));
  matches.forEach(s=>box.append(action(s.name+' · '+(s.area||'Area not set')+' · #'+s.id,()=>{
    chosenStall=s.id; $('chosenStall').textContent=s.name;
    $('joinOffice').hidden=false; $('joinName').focus();
  })));
  if(!matches.length)box.append(node('p','No matching stall yet. Ask your tea-stall owner to finish setting up their tea desk.'));
}
async function resumeOffice(){
  if(localStorage.getItem('chayDevice')===officeToken){
    let state;
    try{state=await api('access');}
    catch(e){
      $('auth').hidden=true;$('discovery').hidden=false;$('accessPending').hidden=true;
      message('Saved office access is unavailable. Choose another stall or office to reconnect.');
      return;
    }
    if(state.status==='PENDING'){
      $('auth').hidden=true; $('discovery').hidden=false;
      $('joinOffice').hidden=true; $('stallResults').replaceChildren();
      $('accessPending').hidden=false;
      message('Your office request is waiting for the owner.');
      if(!accessTimer)accessTimer=setInterval(()=>{if(!document.hidden)resumeOffice().catch(e=>message(e.message))},10000);
      return;
    }
    if(state.status==='REVOKED'){
      if(accessTimer){clearInterval(accessTimer);accessTimer=null;}
      $('discovery').hidden=false; $('accessPending').hidden=true;
      message('The owner removed this browser’s access. Choose your stall and request access again.');
      return;
    }
  }
  if(accessTimer){clearInterval(accessTimer);accessTimer=null;}
  $('discovery').hidden=true;
  try{await office();message('Your office is ready to order.');}
  catch(e){
    if(e.message==='Office PIN required or incorrect')$('pinGate').hidden=false;
    else {$('discovery').hidden=false;$('accessPending').hidden=true;}
    message(e.message);
  }
}
async function loadAccessRequests(){
  const requests=(await api('access/requests')).requests, box=$('accessRequests');box.replaceChildren();
  requests.filter(r=>r.status!=='REVOKED').forEach(r=>{
    const row=node('div',r.office_name+' · '+(r.status==='PENDING'?'Waiting for your approval':'Connected browser'),'row');
    if(r.status==='PENDING'){
      const select=document.createElement('select');select.setAttribute('aria-label','Office account for '+r.office_name);
      const empty=node('option','Choose the correct saved office');empty.value='';select.append(empty);
      sellerState.offices.forEach(o=>{const option=node('option',o.office_name);option.value=o.office_id;select.append(option)});
      row.append(select,action('Approve office',async()=>{
        if(!select.value)throw Error('Save the office below, then choose it here.');
        await api('access/requests/'+r.id,'POST',{status:'APPROVED',office_id:Number(select.value)});
        await seller();message('Office connected. They can now order on this browser.');
      },false));
    }
    row.append(action(r.status==='PENDING'?'Decline':'Remove browser access',async()=>{
      if(!confirm('Remove this browser’s office access?'))return;
      await api('access/requests/'+r.id,'POST',{status:'REVOKED'});await loadAccessRequests();
    }));box.append(row);
  });
  if(!box.children.length)box.append(node('p','No office access requests. Customers can find your stall from the ChayRound home page.'));
}
document.addEventListener('DOMContentLoaded',()=>{
  $('ownerEntry').onclick=()=>{location.href='/#owner';location.reload();return false;};
  $('stallSearch').oninput=renderStalls;
  bind('joinOffice',async()=>{
    if(!chosenStall)throw Error('Choose your tea stall first.');
    const request=await api('access/request','POST',{stall_id:chosenStall,office_name:$('joinName').value});
    officeToken=request.device_token;officePin='';localStorage.setItem('chayDevice',officeToken);
    await resumeOffice();
  });
  $('checkAccess').onclick=()=>resumeOffice().catch(e=>message(e.message));
  const changeOffice=()=>{
    if(!confirm('Choose another office on this browser?'))return;
    localStorage.removeItem('chayDevice');location.href='/';
  };
  $('changeOffice').onclick=changeOffice; $('switchOffice').onclick=changeOffice;
});
